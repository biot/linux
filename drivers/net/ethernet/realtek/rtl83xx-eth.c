// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/net/ethernet/rtl83xx_eth.c
 * Copyright (C) 2020 B. Koblitz
 */

#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/phylink.h>
#include <net/dsa.h>

#include "rtl83xx-eth.h"


extern const struct rtl83xx_soc rtl838x_reg;
extern const struct rtl83xx_soc rtl839x_reg;


static void rtl83xx_hw_ring_setup(struct rtl83xx_eth_priv *ethpriv)
{
	int i;
	struct ringset *rs = ethpriv->ringset;

	for (i = 0; i < RTL_RX_NUM_RINGS; i++) {
		ethpriv->soc->dma_rx_ring_membase(ethpriv, i, CPHYSADDR(&rs->rx_r[i]));
	}

	for (i = 0; i < RTL_TX_NUM_RINGS; i++) {
		ethpriv->soc->dma_tx_ring_membase(ethpriv, i, CPHYSADDR(&rs->tx_r[i]));
	}
}

static void rtl83xx_setup_ringbuf(struct ringset *rs)
{
	int i, j;
	struct p_hdr *h;

	for (i = 0; i < RTL_RX_NUM_RINGS; i++) {
		for (j=0; j < RTL_RX_RING_LEN; j++) {
			h = &rs->rx_header[i][j];
			h->buf = (u8 *)CPHYSADDR(rs->rx_space + i * j * RTL_FRAMEBUF_SIZE);
			h->reserved = 0;
			h->size = RTL_FRAMEBUF_SIZE;
			h->offset = 0;
			h->len = 0;
			/* All rings owned by switch, last one wraps */
			rs->rx_r[i][j] = CPHYSADDR(h) | 0x1 | (j == (RTL_RX_RING_LEN - 1)? WRAP : 0x0);
		}
		rs->c_rx[i] = 0;
	}

	for (i = 0; i < RTL_TX_NUM_RINGS; i++) {
		for (j=0; j < RTL_TX_RING_LEN; j++) {
			h = &rs->tx_header[i][j];
			h->buf = (u8 *)CPHYSADDR(rs->tx_space + i * j * RTL_FRAMEBUF_SIZE);
			h->reserved = 0;
			h->size = RTL_FRAMEBUF_SIZE;
			h->offset = 0;
			h->len = 0;
			rs->tx_r[i][j] = CPHYSADDR(&rs->tx_header[i][j]);
		}
		/* Last header is wrapping around */
		rs->tx_r[i][j-1] |= 0x2;
		rs->c_tx[i] = 0;
	}
}

static int rtl83xx_eth_open(struct net_device *ndev)
{
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);
	struct ringset *rs = ethpriv->ringset;
	unsigned long flags;
	int err;

	spin_lock_irqsave(&ethpriv->lock, flags);

	ethpriv->soc->reset(ethpriv);
	rtl83xx_setup_ringbuf(rs);
	rtl83xx_hw_ring_setup(ethpriv);
	err = request_irq(ndev->irq, ethpriv->soc->nic_irq_handler, IRQF_SHARED,
			  ndev->name, ndev);
	if (err) {
		netdev_err(ndev, "%s: could not acquire interrupt: %d\n",
			   __func__, err);
		return err;
	}
	phylink_start(ethpriv->phylink);

	napi_enable(&ethpriv->napi);
	netif_start_queue(ndev);

	ethpriv->soc->enable_rxtx(ethpriv);

	spin_unlock_irqrestore(&ethpriv->lock, flags);

	return 0;
}

static int rtl83xx_eth_stop(struct net_device *ndev)
{
	unsigned long flags;
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);

	spin_lock_irqsave(&ethpriv->lock, flags);
	phylink_stop(ethpriv->phylink);
	ethpriv->soc->stop(ethpriv);
	free_irq(ndev->irq, ndev);
	napi_disable(&ethpriv->napi);
	netif_stop_queue(ndev);
	spin_unlock_irqrestore(&ethpriv->lock, flags);
	return 0;
}

static int rtl83xx_eth_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(dev);
	struct ringset *rs = ethpriv->ringset;
	static int num = 0;
	int ret;
	unsigned long flags;
	struct p_hdr *h;
	int dest_port = -1;

	spin_lock_irqsave(&ethpriv->lock, flags);
	len = skb->len;
	/* Check for DSA tagging at the end of the buffer */
	if (netdev_uses_dsa(dev) && skb->data[len-4] == 0x80 && skb->data[len-3] >0 
			&& skb->data[len-3] < 28 &&  skb->data[len-2] == 0x10 
			&&  skb->data[len-1] == 0x00) {
		/* Reuse tag space for CRC */
		dest_port = skb->data[len-3];
		len -= 4;
	}
	if (len < ETH_ZLEN)
		len = ETH_ZLEN;
	/* ASIC expects that packet includes CRC, so we extend by 4 bytes */
	len += 4;

	if (skb_padto(skb, len )) {
		ret = NETDEV_TX_OK;
		goto txdone;
	}
	num++;

	/* We can send this packet if CPU owns the descriptor */
	if (!(rs->tx_r[0][rs->c_tx[0]] & 0x1)) {
		/* Set descriptor for tx */
		h = &rs->tx_header[0][rs->c_tx[0]];

		h->buf = (u8 *)CPHYSADDR(rs->tx_space);
		h->size = len;
		h->len = len;

		/* Create cpu_tag */
		if (dest_port > 0) {
			h->cpu_tag[0] = 0x0400;
			h->cpu_tag[1] = 0x0200;
			h->cpu_tag[2] = 0x0000;
			h->cpu_tag[3] = (1 << dest_port) >> 16;
			h->cpu_tag[4] = (1 << dest_port) & 0xffff;
		} else {
			h->cpu_tag[0] = 0;
			h->cpu_tag[1] = 0;
			h->cpu_tag[2] = 0;
			h->cpu_tag[3] = 0;
			h->cpu_tag[4] = 0;
		}

		/* Copy packet data to tx buffer */
		memcpy((void *)KSEG1ADDR(h->buf), skb->data, len);
		mb(); /* wmb() works, too */

		/* Hand over to switch */
		rs->tx_r[0][rs->c_tx[0]] = rs->tx_r[0][rs->c_tx[0]] | 0x1; 

		/* Tell switch to send data */
		ethpriv->soc->dma_if_fetch(ethpriv, 1);

		dev->stats.tx_packets++;
		dev->stats.tx_bytes += len;
		dev_kfree_skb(skb);
		rs->c_tx[0] = (rs->c_tx[0] + 1) % RTL_TX_RING_LEN;
		ret = NETDEV_TX_OK;
	} else {
		dev_warn(&ethpriv->pdev->dev, "Data is owned by switch\n");
		ret = NETDEV_TX_BUSY;
	}
txdone:
	spin_unlock_irqrestore(&ethpriv->lock, flags);
	return ret;
}

static int rtl83xx_set_mac_address(struct net_device *ndev, void *p)
{
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);
	const struct sockaddr *addr = p;
	u8 *mac = (u8*)(addr->sa_data);

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(ndev->dev_addr, addr->sa_data, ETH_ALEN);
	ethpriv->soc->set_mac_addr(ethpriv, mac);

	return 0;
}

static void rtl83xx_eth_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	unsigned long flags;
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);
	spin_lock_irqsave(&ethpriv->lock, flags);
	ethpriv->soc->stop(ethpriv);
	rtl83xx_hw_ring_setup(ethpriv);
	ethpriv->soc->enable_rxtx(ethpriv);
	netif_trans_update(ndev);
	netif_start_queue(ndev);
	spin_unlock_irqrestore(&ethpriv->lock, flags);
}

static const struct net_device_ops rtl83xx_eth_netdev_ops = {
	.ndo_open = rtl83xx_eth_open,
	.ndo_stop = rtl83xx_eth_stop,
	.ndo_start_xmit = rtl83xx_eth_tx,
	.ndo_set_mac_address = rtl83xx_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_tx_timeout = rtl83xx_eth_tx_timeout,
};

static void rtl83xx_validate(struct phylink_config *config, unsigned long *supported,
			     struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask) = { 0, };

	if (!phy_interface_mode_is_rgmii(state->interface) &&
	    state->interface != PHY_INTERFACE_MODE_1000BASEX &&
	    state->interface != PHY_INTERFACE_MODE_MII &&
	    state->interface != PHY_INTERFACE_MODE_REVMII &&
	    state->interface != PHY_INTERFACE_MODE_GMII &&
	    state->interface != PHY_INTERFACE_MODE_QSGMII &&
	    state->interface != PHY_INTERFACE_MODE_INTERNAL &&
	    state->interface != PHY_INTERFACE_MODE_SGMII) {
		bitmap_zero(supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
		pr_err("Unsupported interface: %d\n", state->interface);
		return;
	}

	/* Allow all the expected bits */
	phylink_set(mask, Autoneg);
	phylink_set_port_modes(mask);
	phylink_set(mask, Pause);
	phylink_set(mask, Asym_Pause);

	/* With the exclusion of MII and Reverse MII, we support Gigabit,
	 * including Half duplex
	 */
	if (state->interface != PHY_INTERFACE_MODE_MII &&
	    state->interface != PHY_INTERFACE_MODE_REVMII) {
		phylink_set(mask, 1000baseT_Full);
		phylink_set(mask, 1000baseT_Half);
	}

	phylink_set(mask, 10baseT_Half);
	phylink_set(mask, 10baseT_Full);
	phylink_set(mask, 100baseT_Half);
	phylink_set(mask, 100baseT_Full);

	bitmap_and(supported, supported, mask,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
	bitmap_and(state->advertising, state->advertising, mask,
		   __ETHTOOL_LINK_MODE_MASK_NBITS);
}


static void rtl83xx_mac_config(struct phylink_config *config, unsigned int mode,
			       const struct phylink_link_state *state)
{
	/* This is only being called for the master device,
	 * i.e. the CPU-Port
	 */

	return;
}

static void rtl83xx_mac_an_restart(struct phylink_config *config)
{
	struct net_device *dev = container_of(config->dev, struct net_device, dev);
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(dev);

	/* Restart by disabling and re-enabling link */
	ethpriv->soc->mac_force_mode(ethpriv, 0);
	mdelay(20);
	ethpriv->soc->mac_force_mode(ethpriv, 1);
}

static void rtl83xx_mac_link_down(struct phylink_config *config, unsigned int mode,
				  phy_interface_t interface)
{
	struct net_device *dev = container_of(config->dev, struct net_device, dev);
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(dev);

	/* Stop TX/RX to port */
	ethpriv->soc->mac_port_rxtx(ethpriv, 0);
}

static void rtl83xx_mac_link_up(struct phylink_config *config,
				struct phy_device *phy, unsigned int mode,
				phy_interface_t interface, int speed, int duplex,
				bool tx_pause, bool rx_pause)
{
	struct net_device *dev = container_of(config->dev, struct net_device, dev);
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(dev);

	/* Restart TX/RX to port */
	ethpriv->soc->mac_port_rxtx(ethpriv, 1);
}

static const struct phylink_mac_ops rtl83xx_phylink_ops = {
	.validate = rtl83xx_validate,
	.mac_config = rtl83xx_mac_config,
	.mac_an_restart = rtl83xx_mac_an_restart,
	.mac_link_down = rtl83xx_mac_link_down,
	.mac_link_up = rtl83xx_mac_link_up,
};

static int rtl83xx_get_link_ksettings(struct net_device *ndev,
				      struct ethtool_link_ksettings *kset)
{
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);

	return phylink_ethtool_ksettings_get(ethpriv->phylink, kset);
}

static int rtl83xx_set_link_ksettings(struct net_device *ndev,
				      const struct ethtool_link_ksettings *kset)
{
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);

	return phylink_ethtool_ksettings_set(ethpriv->phylink, kset);
}

static const struct ethtool_ops rtl83xx_ethtool_ops = {
	.get_link_ksettings     = rtl83xx_get_link_ksettings,
	.set_link_ksettings     = rtl83xx_set_link_ksettings,
};

static int rtl83xx_hw_receive(struct net_device *dev, int r, int budget)
{
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(dev);
	struct ringset *rs = ethpriv->ringset;
	struct sk_buff *skb;
	unsigned long flags;
	int i, len, work_done = 0;
	u8 *data, *skb_data;
	u32 *last, val;
	struct p_hdr *h;
	bool dsa = netdev_uses_dsa(dev);

	spin_lock_irqsave(&ethpriv->lock, flags);
	ethpriv->soc->dma_rx_ring_cur_get(ethpriv, r, &val);
	last = (u32 *)KSEG1ADDR(val);

	if ( &rs->rx_r[r][rs->c_rx[r]] == last ) {
		spin_unlock_irqrestore(&ethpriv->lock, flags);
		return 0;
	}
	do {
		if ((rs->rx_r[r][rs->c_rx[r]] & 0x1)) {
			netdev_warn(dev, "WARNING Ring contention: ring %x, last %x, current %x, cPTR %x\n", r, (uint32_t)last,
				    (u32) &rs->rx_r[r][rs->c_rx[r]],
				    rs->rx_r[r][rs->c_rx[r]]);
			break;
		}

		h = &rs->rx_header[r][rs->c_rx[r]];
		data = (u8 *)KSEG1ADDR(h->buf);
		len = h->len;

		if (!len)
			break;
		h->buf = (u8 *)CPHYSADDR(rs->rx_space
				+ r * rs->c_rx[r] * RTL_FRAMEBUF_SIZE);
		h->size = RTL_FRAMEBUF_SIZE;
		h->len = 0;
		work_done++;

		len -= 4; /* strip the CRC */
		/* Add 4 bytes for cpu_tag */
		if (dsa)
			len += 4;

		skb = alloc_skb(len + 4, GFP_KERNEL);
		skb_reserve(skb, NET_IP_ALIGN);

		if (likely(skb)) {
			/* BUG: Prevent bug */
			ethpriv->soc->dma_rx_ring_size(ethpriv, 0, 0xffffffff);
			// sw_w32(0xffffffff, ethpriv->soc->dma_if_rx_ring_size(0));
			for(i = 0; i < RTL_RX_NUM_RINGS; i++) {
				/* Update each ring cnt */
				ethpriv->soc->dma_rx_ring_cntr_get(ethpriv, i, &val);
				ethpriv->soc->dma_rx_ring_cntr_set(ethpriv, i, val);
			}

			skb_data = skb_put(skb, len);
			mb();
			memcpy(skb->data, (u8 *)KSEG1ADDR(data), len);
			/* Overwrite CRC with cpu_tag */
			if (dsa) {
				skb->data[len-4] = 0x80;
				skb->data[len-3] = h->cpu_tag[0] & 0x1f;
				skb->data[len-2] = 0x10;
				skb->data[len-1] = 0x00;
			}
			skb->protocol = eth_type_trans(skb, dev);
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += len;

			netif_receive_skb(skb);
		} else {
			if (net_ratelimit())
				dev_warn(&dev->dev,
				    "low on memory - packet dropped\n");
			dev->stats.rx_dropped++;
		}
		rs->rx_r[r][rs->c_rx[r]]
			= CPHYSADDR(h) | 0x1 | (rs->c_rx[r] == (RTL_RX_RING_LEN-1)? WRAP : 0x1);
		rs->c_rx[r] = (rs->c_rx[r] + 1) % RTL_RX_RING_LEN;
	} while (&rs->rx_r[r][rs->c_rx[r]] != last && work_done < budget);

	spin_unlock_irqrestore(&ethpriv->lock, flags);
	return work_done;
}

static int rtl83xx_poll_rx(struct napi_struct *napi, int budget)
{
	struct rtl83xx_eth_priv *ethpriv = container_of(napi, struct rtl83xx_eth_priv, napi);
	int work_done = 0, r = 0;

	while (work_done < budget && r < RTL_RX_NUM_RINGS) {
		work_done += rtl83xx_hw_receive(ethpriv->netdev, r, budget - work_done);
		r++;
	}

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		/* Enable RX interrupt */
		ethpriv->soc->dma_irq(ethpriv, 1);
	}
	return work_done;
}

static int __init rtl83xx_eth_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct device_node *dn;
	struct rtl83xx_eth_priv *ethpriv;
	const void *mac;
	phy_interface_t phy_mode;
	struct phylink *phylink;
	unsigned int family;
	int err = 0;

	dn = pdev->dev.of_node;

	ndev = devm_alloc_etherdev(&pdev->dev, sizeof(*ethpriv));
	if (!ndev) {
		err = -ENOMEM;
		goto err_free;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ethpriv = netdev_priv(ndev);

	// TODO: get this from DT
	ethpriv->iobase = (void *)0xbb000000;
	// ethpriv->iobase = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	// if (IS_ERR(ethpriv->iobase))
	// 	return -ENXIO;

	ethpriv->ringset = dmam_alloc_coherent(&pdev->dev, sizeof(struct ringset),
					       &ethpriv->dma_handle, GFP_KERNEL);
	if (!ethpriv->ringset)
		return -ENOMEM;

	err = platform_get_irq(pdev, 0);
	if (err <= 0) {
		return err;
	}
	ndev->irq = err;

	spin_lock_init(&ethpriv->lock);

	ndev->ethtool_ops = &rtl83xx_ethtool_ops;

	ethpriv->soc_id = (unsigned int)of_device_get_match_data(&pdev->dev);
	family = ethpriv->soc_id & 0xfff0;

	if (family == 0x8380) {
		ethpriv->cpu_port = RTL838X_CPU_PORT;
		ethpriv->soc = &rtl838x_reg;
		ethpriv->soc->init_mac(ethpriv);
	} else if (family == 0x8390) {
		ethpriv->cpu_port = RTL839X_CPU_PORT;
		ethpriv->soc = &rtl839x_reg;
		ethpriv->soc->init_mac(ethpriv);
	} else {
		pr_err("Unknown chip family\n");
		return -ENODEV;
	}

	mac = of_get_mac_address(dn);
	if (!IS_ERR(mac)) {
		memcpy(ndev->dev_addr, mac, ETH_ALEN);
		ethpriv->soc->set_mac_addr(ethpriv, (u8 *)mac);
	} else {
		/* Use MAC address U-Boot left in the hardware */
		ethpriv->soc->get_mac_addr(ethpriv, (u8 *)ndev->dev_addr);
	}
	if (!is_valid_ether_addr(ndev->dev_addr)) {
		netdev_warn(ndev, "Invalid MAC address, using random\n");
		eth_hw_addr_random(ndev);
		ethpriv->soc->set_mac_addr(ethpriv, (u8 *)ndev->dev_addr);
	}

	strcpy(ndev->name, "eth%d");
	ndev->netdev_ops = &rtl83xx_eth_netdev_ops;
	ethpriv->pdev = pdev;
	ethpriv->netdev = ndev;

	// TODO: make sure mdio driver is live

	err = register_netdev(ndev);
	if (err)
		goto err_free;

	netif_napi_add(ndev, &ethpriv->napi, rtl83xx_poll_rx, 64);
	platform_set_drvdata(pdev, ndev);

	err = of_get_phy_mode(dn, &phy_mode);
	if (err < 0) {
		dev_err(&pdev->dev, "incorrect phy-mode\n");
		goto err_free;
	}
	ethpriv->phylink_config.dev = &ndev->dev;
	ethpriv->phylink_config.type = PHYLINK_NETDEV;

	phylink = phylink_create(&ethpriv->phylink_config, pdev->dev.fwnode,
				 phy_mode, &rtl83xx_phylink_ops);
	if (IS_ERR(phylink)) {
		err = PTR_ERR(phylink);
		goto err_free;
	}
	ethpriv->phylink = phylink;
	return 0;

err_free:
	pr_err("Error initializing ethernet.\n");
	return err;
}

static int rtl83xx_eth_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(dev);
	if (dev) {
		ethpriv->soc->stop(ethpriv);
		netif_stop_queue(dev);
		netif_napi_del(&ethpriv->napi);
		unregister_netdev(dev);
	}
	return 0;
}

static const struct of_device_id rtl83xx_eth_of_ids[] = {
	{ .compatible = "realtek,rtl8382-eth", .data = (void *)0x8382 },
	{ .compatible = "realtek,rtl8392-eth", .data = (void *)0x8392 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtl83xx_eth_of_ids);

static struct platform_driver rtl83xx_eth_driver = {
	.probe = rtl83xx_eth_probe,
	.remove = rtl83xx_eth_remove,
	.driver = {
		.name = "rtl83xx-eth",
		.pm = NULL,
		.of_match_table = rtl83xx_eth_of_ids,
	},
};

module_platform_driver(rtl83xx_eth_driver);

MODULE_AUTHOR("B. Koblitz");
MODULE_DESCRIPTION("RTL83XX SoC Ethernet Driver");
MODULE_LICENSE("GPL");

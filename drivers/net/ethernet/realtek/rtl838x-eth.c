// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/net/ethernet/rtl83xx_eth.c
 * Copyright (C) 2020 B. Koblitz
 */

#include <linux/io.h>
#include <linux/etherdevice.h>
#include <linux/phylink.h>

#include "rtl83xx-eth.h"
#include "rtl838x-eth.h"


int rtl838x_smi_wait_op(int timeout)
{
	do {
		timeout--;
		udelay(10);
	} while ((sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & 0x1) && (timeout >= 0));
	if (timeout <= 0) {
		return -1;
	}
	return 0;
}

/* TODO: call this from mdio_probe, when we have a list of PHYs */
static inline void init_congestion_timers(struct rtl83xx_eth_priv *ethpriv)
{
	int i;

	if (ethpriv->soc_id == 0x8382) {
		for (i = 0; i <= 28; i++)
			sw_w32(0, REG(RTL838X_CNGST_EN) + i * 0x80);
	} else if (ethpriv->soc_id == 0x8380) {
		for (i = 8; i <= 28; i++)
			sw_w32(0, REG(RTL838X_CNGST_EN) + i * 0x80);
	}

}

static inline void set_eee_tx_timers(struct rtl83xx_eth_priv *ethpriv, u32 reg,
				     u8 pause_wake, u16 delay, u8 wake)
{
	sw_w32(pause_wake << 24 | delay << 12 | wake, REG(reg));
}

static void rtl838x_init_mac(struct rtl83xx_eth_priv *ethpriv)
{
	/* pause_wake, delay and wake timers in usec */
	set_eee_tx_timers(ethpriv, RTL838X_EEE_TX_TIMER_GIGA_CTRL, 5, 20, 17);
	set_eee_tx_timers(ethpriv, RTL838X_EEE_TX_TIMER_GELITE_CTRL, 5, 20, 23);

	init_congestion_timers(ethpriv);
}

static void rtl838x_get_mac_addr(struct rtl83xx_eth_priv *ethpriv, u8 *mac)
{
	u32 val[2];

	val[0] = sw_r32(REG(RTL838X_MAC_ADDR));
	val[1] = sw_r32(REG(RTL838X_MAC_ADDR) + 4);
	memcpy(mac, (u8 *)val + 2, 8);
}

static void rtl838x_set_mac_addr(struct rtl83xx_eth_priv *ethpriv, u8 *mac)
{
	unsigned long flags;
	u32 regs[2];

	spin_lock_irqsave(&ethpriv->lock, flags);

	regs[0] = (mac[0] << 8) | mac[1];
	regs[1] = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];

	sw_w32(regs[0], REG(RTL838X_MAC_ADDR));
	sw_w32(regs[1], REG(RTL838X_MAC_ADDR) + 4);

	sw_w32(regs[0], REG(RTL838X_MAC_ALE));
	sw_w32(regs[1], (REG(RTL838X_MAC_ALE) + 4));

	sw_w32(regs[0], REG(RTL838X_MAC2));
	sw_w32(regs[1], REG(RTL838X_MAC2) + 4);

	spin_unlock_irqrestore(&ethpriv->lock, flags);
}

static void dma_if_fetch(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL838X_DMA_IF_CTRL));
	if (enable)
		val |= _RTL838X_DMAIF_TX_FETCH;
	else
		val &= ~(_RTL838X_DMAIF_TX_FETCH);
	sw_w32(val, REG(RTL838X_DMA_IF_CTRL));
}

static void mac_port_rxtx(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL838X_MAC_PORT_CTRL(ethpriv->cpu_port)));
	if (enable)
		val |= _RTL838X_MACPORT_TXRX_EN;
	else
		val &= ~_RTL838X_MACPORT_TXRX_EN;
	sw_w32(val, REG(RTL838X_MAC_PORT_CTRL(ethpriv->cpu_port)));
}

static void mac_force_mode(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL838X_MAC_FORCE_MODE_CTRL(ethpriv->cpu_port)));
	u32 bits = _RTL838X_MACFORCEMODE_SMI_GLITE_MASTER_SLV_MANUAL_SEL \
		| _RTL838X_MACFORCEMODE_SMI_GLITE_PORT_TYPE \
		| _RTL838X_MACFORCEMODE_PHY_MASTER_SLV_MANUAL_SEL \
		| _RTL838X_MACFORCEMODE_PHY_PORT_TYPE \
		| _RTL838X_MACFORCEMODE_MAC_FORCE_FC_EN \
		| (2 << _RTL838X_MACFORCEMODE_SPD_SEL) \
		| _RTL838X_MACFORCEMODE_DUP_SEL \
		| _RTL838X_MACFORCEMODE_NWAY_EN;
	u32 bits_on = _RTL838X_MACFORCEMODE_FORCE_LINK_EN \
		| _RTL838X_MACFORCEMODE_MAC_FORCE_EN;

	if (enable)
		val |= bits | bits_on;
	else
		val &= ~(bits);
	sw_w32(val, REG(RTL838X_MAC_FORCE_MODE_CTRL(ethpriv->cpu_port)));
}

static void dma_rx_ring_membase(struct rtl83xx_eth_priv *ethpriv, int ring, u32 membase)
{
	sw_w32(membase, REG(RTL838X_DMA_IF_RX_BASE_DESC_ADDR_CTRL(ring)));
}

static void dma_tx_ring_membase(struct rtl83xx_eth_priv *ethpriv, int ring, u32 membase)
{
	sw_w32(membase, REG(RTL838X_DMA_IF_TX_BASE_DESC_ADDR_CTRL(ring)));
}

static void dma_rx_ring_size(struct rtl83xx_eth_priv *ethpriv, int ring, u32 size)
{
	sw_w32(size, REG(RTL838X_DMA_IF_RX_RING_SIZE(ring)));
}

static void dma_rx_ring_cntr_get(struct rtl83xx_eth_priv *ethpriv, int ring, u32 *value)
{
	*value = sw_r32(REG(RTL838X_DMA_IF_RX_RING_CNTR(ring)));
}

static void dma_rx_ring_cntr_set(struct rtl83xx_eth_priv *ethpriv, int ring, u32 value)
{
	sw_w32(value, REG(RTL838X_DMA_IF_RX_RING_CNTR(ring)));
}

static void dma_rx_ring_cur_get(struct rtl83xx_eth_priv *ethpriv, int ring, u32 *value)
{
	*value = sw_r32(REG(RTL838X_DMA_IF_RX_CUR(ring)));
}

/*
 * Discard the RX ring-buffers, called as part of the net-ISR
 * when the buffer runs over
 * Caller needs to hold priv->lock
 */
static void rtl83xx_rb_cleanup(struct rtl83xx_eth_priv *ethpriv)
{
	struct ringset *rs = ethpriv->ringset;
	struct p_hdr *h;
	int r;
	u32 *last, val;

	for (r = 0; r < RTL_RX_NUM_RINGS; r++) {
		ethpriv->soc->dma_rx_ring_cur_get(ethpriv, r, &val);
		last = (u32 *)KSEG1ADDR(val);
		do {
			if ((rs->rx_r[r][rs->c_rx[r]] & 0x1))
				break;
			h = &rs->rx_header[r][rs->c_rx[r]];
			h->buf = (u8 *)CPHYSADDR(rs->rx_space
					+ r * rs->c_rx[r] * RTL_FRAMEBUF_SIZE);
			h->size = RTL_FRAMEBUF_SIZE;
			h->len = 0;
			mb();

			rs->rx_r[r][rs->c_rx[r]] = CPHYSADDR(h) | 0x1
				| (rs->c_rx[r] == (RTL_RX_RING_LEN-1)? WRAP : 0x1);
			rs->c_rx[r] = (rs->c_rx[r] + 1) % RTL_RX_RING_LEN;
		} while (&rs->rx_r[r][rs->c_rx[r]] != last);
	}
}

static void dma_if_rxtx(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL838X_DMA_IF_CTRL));
	if (enable)
		val |= _RTL838X_DMAIF_TX_EN | _RTL838X_DMAIF_RX_EN;
	else
		val &= ~(_RTL838X_DMAIF_TX_EN | _RTL838X_DMAIF_RX_EN);
	sw_w32(val, REG(RTL838X_DMA_IF_CTRL));
}

static void enable_rxtx(struct rtl83xx_eth_priv *ethpriv)
{
	/* Disable Head of Line features for all RX rings */
	ethpriv->soc->dma_rx_ring_size(ethpriv, 0, 0xffffffff);

	// TODO hardcoded for 838x, doesn't enable RX_TRUNCATE_EN
	/* Truncate RX buffer to 0x640 (1600) bytes, pad TX */
	// sw_w32(0x06400020, ethpriv->soc->dma_if_ctrl);

	/* Enable RX done, RX overflow and TX done interrupts */
	sw_w32(0xfffff, REG(RTL838X_DMA_IF_INTR_MSK));

	/* Enable traffic, engine expects empty FCS field */
	dma_if_rxtx(ethpriv, 1);
}

static void reset_nic(struct rtl83xx_eth_priv *ethpriv)
{
	sw_w32(_RTL838X_SW_NIC_RST, REG(RTL838X_RST_GLB_CTRL));

	while (sw_r32(REG(RTL838X_RST_GLB_CTRL)) & _RTL838X_SW_NIC_RST)
		cpu_relax();
}

static void reset(struct rtl83xx_eth_priv *ethpriv)
{
	/* Stop TX/RX */
	ethpriv->soc->mac_port_rxtx(ethpriv, 0);
	mdelay(500);

	reset_nic(ethpriv);
	mdelay(100);

	/* Restart TX/RX to CPU port */
	ethpriv->soc->mac_port_rxtx(ethpriv, 1);

	/* Force CPU port link up */
	ethpriv->soc->mac_force_mode(ethpriv, 1);

	/* Disable and clear interrupts */
	sw_w32(0x00000000, REG(RTL838X_DMA_IF_INTR_MSK));
	sw_w32(0xffffffff, REG(RTL838X_DMA_IF_INTR_STS));
}

// TODO: belongs in DSA, this driver shouldn't know about non-CPU ports
static void l2_flush_ports(struct rtl83xx_eth_priv *ethpriv)
{
	u32 val;
	int p;

	for (p = 0; p < ethpriv->cpu_port; p++) {
		val = _RTL838X_L2FLUSH_STS | _RTL838X_L2FLUSH_PORT_CMP | p << 5;
		sw_w32(val, REG(RTL838X_L2_TBL_FLUSH_CTRL));
		while (sw_r32(REG(RTL838X_L2_TBL_FLUSH_CTRL)) & _RTL838X_L2FLUSH_STS)
			cpu_relax();
	}
}

static void stop(struct rtl83xx_eth_priv *ethpriv)
{
	u32 data[3];

	// TODO: mask out all but ports 50/51 in table MC_PMSK, why?
	/* Block all ports */
	data[0] = 0x03000000;
	data[1] = 0x00000000;
	data[2] = 0;
	rtl_table_write(ethpriv, RTL838X_TBL_0, 2, 0, data);

	l2_flush_ports(ethpriv);

	/* CPU-Port: Link down BUG: Works only for RTL838x */
	ethpriv->soc->mac_force_mode(ethpriv, 0);
	mdelay(100);

	/* Disable traffic */
	dma_if_rxtx(ethpriv, 0);
	mdelay(200);

	/* Disable all TX/RX interrupts */
	sw_w32(0x00000000, REG(RTL838X_DMA_IF_INTR_MSK));
	sw_w32(0x000fffff, REG(RTL838X_DMA_IF_INTR_STS));
	mdelay(200);
}

static void dma_irq(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL838X_DMA_IF_INTR_MSK));
	u32 rxtx = (0xff << _RTL838X_DMAIFINTRMSK_RX_RUN_OUT) \
		| (0xff << _RTL838X_DMAIFINTRMSK_RX_DONE) \
		| (0x3 << _RTL838X_DMAIFINTRMSK_TX_DONE) \
		| (0x3 << _RTL838X_DMAIFINTRMSK_TX_ALL_DONE);

	if (enable)
		val |= rxtx;
	else
		val &= rxtx;
	sw_w32(val, REG(RTL838X_DMA_IF_INTR_MSK));
}

static irqreturn_t nic_irq_handler(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);
	u32 status = sw_r32(REG(RTL838X_DMA_IF_INTR_STS));
	u32 val;

	spin_lock(&ethpriv->lock);

	/*  Ignore TX interrupt */
	val = 0x3 << _RTL838X_DMAIFINTRSTS_TX_DONE;
	val |= 0x3 << _RTL838X_DMAIFINTRSTS_TX_ALL_DONE;
	if (status & val) {
		/* Clear ISR */
		sw_w32(val, REG(RTL838X_DMA_IF_INTR_STS));
	}

	/* RX interrupt */
	if (status & (0xff << _RTL838X_DMAIFINTRSTS_RX_DONE)) {
		/* Disable RX interrupt */
		val = sw_r32(REG(RTL838X_DMA_IF_INTR_MSK));
		val &= ~(0xff << _RTL838X_DMAIFINTRMSK_RX_DONE);
		sw_w32(val, REG(RTL838X_DMA_IF_INTR_MSK));
		sw_w32(0xff << _RTL838X_DMAIFINTRSTS_RX_DONE, REG(RTL838X_DMA_IF_INTR_STS));
		napi_schedule(&ethpriv->napi);
	}

	/* RX buffer overrun */
	if (status & (0xff << _RTL838X_DMAIFINTRSTS_RX_RUN_OUT)) {
		sw_w32(0xff << _RTL838X_DMAIFINTRSTS_RX_RUN_OUT, REG(RTL838X_DMA_IF_INTR_STS));
		rtl83xx_rb_cleanup(ethpriv);
	}

	spin_unlock(&ethpriv->lock);

	return IRQ_HANDLED;
}


const struct rtl83xx_soc rtl838x_reg = {
	.init_mac = rtl838x_init_mac,
	.get_mac_addr = rtl838x_get_mac_addr,
	.set_mac_addr = rtl838x_set_mac_addr,
	.dma_if_fetch = dma_if_fetch,
	.mac_port_rxtx = mac_port_rxtx,
	.mac_force_mode = mac_force_mode,
	.dma_rx_ring_membase = dma_rx_ring_membase,
	.dma_tx_ring_membase = dma_tx_ring_membase,
	.dma_rx_ring_size = dma_rx_ring_size,
	.dma_rx_ring_cntr_get = dma_rx_ring_cntr_get,
	.dma_rx_ring_cntr_set = dma_rx_ring_cntr_set,
	.dma_rx_ring_cur_get = dma_rx_ring_cur_get,
	.nic_irq_handler = nic_irq_handler,
	.enable_rxtx = enable_rxtx,
	.reset = reset,
	.stop = stop,
	.dma_irq = dma_irq,
};

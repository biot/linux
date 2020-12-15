// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/net/ethernet/rtl83xx_eth.c
 * Copyright (C) 2020 B. Koblitz
 */

#include <linux/io.h>
#include <linux/etherdevice.h>
#include <linux/phylink.h>

#include "rtl83xx-eth.h"
#include "rtl839x-eth.h"


static void rtl839x_init_mac(struct rtl83xx_eth_priv *ethpriv)
{
	u32 val, data[3];
	int i;

	sw_w32(_PKT_BUF_SIZE_SEL, REG(RTL839X_MAC_EFUSE_CTRL));

	sw_w32(_RTL839X_SW_Q_RST, REG(RTL839X_RST_GLB_CTRL));

	val = _IPG_1G_COMPS_EN | (0x2 << _DEFER_IPG_SEL) | _BKPRES_MTHD_SEL;
	val |= _HALF_48PASS1_EN | _BKOFF_SEL | _LIMIT_PAUSE_EN;
	val |= _LATE_COLI_DROP_EN | (0x6 << _DDR_CLK_SEL) | _EFUSE_EN;
	val |= _MAX_RETX_SEL | _MAC_DROP_48PASS1_EN | _CPU_CRC_RC_EN;
	sw_w32(val, REG(RTL839X_MAC_GLB_CTRL));

	/* Multicast portmask (MC_PMSK) table default entry:
	 * enable on all ports except CPU port (mask starts at bit 11).
	 */
	data[0] = 0x7FFFFFFF;
	data[1] = 0xFFFFF800;
	data[2] = 0;
	rtl_table_write(ethpriv, RTL839X_TBL_L2, 2, 0, data);

	/* Scheduler, 250 MHz */
	val = 0x61 << _SCHED_BYTE_PER_TKN;
	val |= 0x89 << _SCHED_TICK_PERIOD;
	val |= 0x97 << _SCHED_BYTE_PER_TKN_10G;
	val |= 0x12 << _SCHED_TICK_PERIOD_10G;
	sw_w32(val, REG(RTL839X_SCHED_LB_TICK_TKN_CTRL));

	/* Storm control, 1G and 10G */
	val = 0x01 << _STORM_TKN;
	val |= 0xee << _STORM_TICK_PERIOD;
	sw_w32(val, REG(RTL839X_STORM_CTRL_LB_TICK_TKN_CTRL_1G));
	val = 0x01 << _STORM_TKN_10G;
	val |= 0xee << _STORM_TICK_PERIOD_10G;
	sw_w32(val, REG(RTL839X_STORM_CTRL_LB_TICK_TKN_CTRL_10G));

	/* Protocol storm control - PPS */
	val = 0xee6b3;
	sw_w32(val, REG(RTL839X_STORM_CTRL_SPCL_LB_TICK_TKN_CTRL));

	/* ACL Policer - bps */
	for (i = 0; i < 16; i++) {
		val = 0x97 << _METER_TKN;
		val |= 0x120 << _METER_TICK_PERIOD;
		sw_w32(val, REG(RTL839X_METER_LB_TICK_TKN_CTRL(i)));
	}

	/* Ingress bandwidth control, 1G and 10G */
	val = 0x81 << _BWCTRL_TKN;
	val |= 0xf6 << _BWCTRL_TICK_PERIOD;
	sw_w32(val, REG(RTL839X_IGR_BWCTRL_LB_TICK_TKN_CTRL_1G));
	val = 0x97 << _BWCTRL_TKN;
	val |= 0x12 << _BWCTRL_TICK_PERIOD;
	sw_w32(val, REG(RTL839X_IGR_BWCTRL_LB_TICK_TKN_CTRL_10G));

	/* EEE timers, for 100M, 500M and 1G */
	val = 14 << _EEEP_RX_WAKE_TIMER_100M;
	val |= 14 << _EEEP_RX_MIN_SLEEP_TIMER_100M;
	val |= 0xff << _EEEP_RX_SLEEP_TIMER_100M;
	val |= 0x15 << _EEEP_RX_PAUSE_ON_TIMER_100M;
	sw_w32(val, REG(RTL839X_EEEP_RX_TIMER_100M_CTRL));

	val = 14 << _EEEP_RX_WAKE_TIMER_500M;
	val |= 14 << _EEEP_RX_MIN_SLEEP_TIMER_500M;
	val |= 0xff << _EEEP_RX_SLEEP_TIMER_500M;
	val |= 0x0d << _EEEP_RX_PAUSE_ON_TIMER_500M;
	sw_w32(val, REG(RTL839X_EEEP_RX_TIMER_500M_CTRL0));

	val = 14 << _EEEP_RX_WAKE_TIMER_GIGA;
	val |= 14 << _EEEP_RX_MIN_SLEEP_TIMER_GIGA;
	val |= 0xff << _EEEP_RX_SLEEP_TIMER_GIGA;
	val |= 0x0b << _EEEP_RX_PAUSE_ON_TIMER_GIGA;
	sw_w32(val, REG(RTL839X_EEEP_RX_TIMER_GIGA_CTRL0));
}

static void rtl839x_get_mac_addr(struct rtl83xx_eth_priv *ethpriv, u8 *mac)
{
	u32 val[2];

	val[0] = sw_r32(REG(RTL839X_MAC_ADDR));
	val[1] = sw_r32(REG(RTL839X_MAC_ADDR) + 4);
	memcpy(mac, (u8 *)val + 2, 8);
}

static void rtl839x_set_mac_addr(struct rtl83xx_eth_priv *ethpriv, u8 *mac)
{
	unsigned long flags;

	spin_lock_irqsave(&ethpriv->lock, flags);

	sw_w32((mac[0] << 8) | mac[1], REG(RTL839X_MAC_ADDR));
	sw_w32((mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5], REG(RTL839X_MAC_ADDR) + 4);

	spin_unlock_irqrestore(&ethpriv->lock, flags);
}

static void dma_if_fetch(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL839X_DMA_IF_CTRL));
	if (enable)
		val |= _RTL839X_DMAIF_TX_FETCH;
	else
		val &= ~(_RTL839X_DMAIF_TX_FETCH);
	sw_w32(val, REG(RTL839X_DMA_IF_CTRL));
}

static void mac_port_rxtx(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL839X_MAC_PORT_CTRL(ethpriv->cpu_port)));
	if (enable)
		val |= _RTL839X_MACPORT_TX_EN | _RTL839X_MACPORT_RX_EN;
	else
		val &= ~(_RTL839X_MACPORT_TX_EN | _RTL839X_MACPORT_RX_EN);
	sw_w32(val, REG(RTL839X_MAC_PORT_CTRL(ethpriv->cpu_port)));
}

static void mac_force_mode(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL839X_MAC_FORCE_MODE_CTRL(ethpriv->cpu_port)));
	if (enable)
		val |= _RTL839X_MACFORCEMODE_FORCE_LINK_EN | _RTL839X_MACFORCEMODE_MAC_FORCE_EN;
	else
		val &= ~(_RTL839X_MACFORCEMODE_FORCE_LINK_EN | _RTL839X_MACFORCEMODE_MAC_FORCE_EN);
	sw_w32(val, REG(RTL839X_MAC_FORCE_MODE_CTRL(ethpriv->cpu_port)));
}

static void dma_rx_ring_membase(struct rtl83xx_eth_priv *ethpriv, int ring, u32 membase)
{
	sw_w32(membase, REG(RTL839X_DMA_IF_RX_BASE_DESC_ADDR_CTRL(ring)));
}

static void dma_tx_ring_membase(struct rtl83xx_eth_priv *ethpriv, int ring, u32 membase)
{
	sw_w32(membase, REG(RTL839X_DMA_IF_TX_BASE_DESC_ADDR_CTRL(ring)));
}

static void dma_rx_ring_size(struct rtl83xx_eth_priv *ethpriv, int ring, u32 size)
{
	sw_w32(size, REG(RTL839X_DMA_IF_RX_RING_SIZE(ring)));
}

static void dma_rx_ring_cntr_get(struct rtl83xx_eth_priv *ethpriv, int ring, u32 *value)
{
	*value = sw_r32(REG(RTL839X_DMA_IF_RX_RING_CNTR(ring)));
}

static void dma_rx_ring_cntr_set(struct rtl83xx_eth_priv *ethpriv, int ring, u32 value)
{
	sw_w32(value, REG(RTL839X_DMA_IF_RX_RING_CNTR(ring)));
}

static void dma_rx_ring_cur_get(struct rtl83xx_eth_priv *ethpriv, int ring, u32 *value)
{
	*value = sw_r32(REG(RTL839X_DMA_IF_RX_CUR(ring)));
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
	u32 val = sw_r32(REG(RTL839X_DMA_IF_CTRL));
	if (enable)
		val |= _RTL839X_DMAIF_TX_EN | _RTL839X_DMAIF_RX_EN;
	else
		val &= ~(_RTL839X_DMAIF_TX_EN | _RTL839X_DMAIF_RX_EN);
	sw_w32(val, REG(RTL839X_DMA_IF_CTRL));
}

static void enable_rxtx(struct rtl83xx_eth_priv *ethpriv)
{
	u32 mc_pmsk[3];

	/* Disable Head of Line features for all RX rings */
	ethpriv->soc->dma_rx_ring_size(ethpriv, 0, 0xffffffff);

	// TODO hardcoded for 838x, doesn't enable RX_TRUNCATE_EN
	/* Truncate RX buffer to 0x640 (1600) bytes, pad TX */
	// sw_w32(0x06400020, ethpriv->soc->dma_if_ctrl);

	/* Enable RX done, RX overflow and TX done interrupts */
	sw_w32(0xfffff, REG(RTL839X_DMA_IF_INTR_MSK));

	/* Enable traffic, engine expects empty FCS field */
	dma_if_rxtx(ethpriv, 1);

	// TODO: why? and why only on 8390?
	/* Make sure to flood all traffic to CPU_PORT */
	rtl_table_read(ethpriv, RTL839X_TBL_L2, 2, 0, mc_pmsk);
	mc_pmsk[0] |= 0x80000000;
	rtl_table_write(ethpriv, RTL839X_TBL_L2, 2, 0, mc_pmsk);
}

static void reset_nic(struct rtl83xx_eth_priv *ethpriv)
{
	sw_w32(_RTL839X_SW_NIC_RST, REG(RTL839X_RST_GLB_CTRL));

	while (sw_r32(REG(RTL839X_RST_GLB_CTRL)) & _RTL839X_SW_NIC_RST)
		cpu_relax();
}

static void reset(struct rtl83xx_eth_priv *ethpriv)
{
	u32 val, notify_saved, notify_mask;

	/* Stop TX/RX */
	ethpriv->soc->mac_port_rxtx(ethpriv, 0);
	mdelay(500);

	notify_mask = _RTL839X_DMAIFINTRMSK_NTFY_DONE \
		| _RTL839X_DMAIFINTRMSK_NTFY_BUF_RUN_OUT \
		| _RTL839X_DMAIFINTRMSK_NTFY_BUF_RUN_OUT;
	notify_saved = sw_r32(REG(RTL839X_DMA_IF_INTR_MSK)) & notify_mask;

	reset_nic(ethpriv);
	mdelay(100);

	/* Restore notification settings */
	val = sw_r32(REG(RTL839X_DMA_IF_INTR_MSK));
	val = (val & ~notify_mask) | notify_saved;
	sw_w32(val, REG(RTL839X_DMA_IF_INTR_MSK));

	/* Restart TX/RX to CPU port */
	ethpriv->soc->mac_port_rxtx(ethpriv, 1);

	/* Force CPU port link up */
	ethpriv->soc->mac_force_mode(ethpriv, 1);

	/* Disable and clear interrupts */
	sw_w32(0x00000000, REG(RTL839X_DMA_IF_INTR_MSK));
	sw_w32(0xffffffff, REG(RTL839X_DMA_IF_INTR_STS));
}

// TODO: belongs in DSA, this driver shouldn't know about non-CPU ports
static void l2_flush_ports(struct rtl83xx_eth_priv *ethpriv)
{
	u32 val;
	int p;

	for (p = 0; p < ethpriv->cpu_port; p++) {
		val = _RTL839X_L2FLUSH_STS | _RTL839X_L2FLUSH_PORT_CMP | p << 5;
		sw_w32(val, REG(RTL839X_L2_TBL_FLUSH_CTRL));
		while (sw_r32(REG(RTL839X_L2_TBL_FLUSH_CTRL)) & _RTL839X_L2FLUSH_STS)
			cpu_relax();
	}
}

static void stop(struct rtl83xx_eth_priv *ethpriv)
{
	l2_flush_ports(ethpriv);

	/* CPU-Port: Link down BUG: Works only for RTL838x */
	ethpriv->soc->mac_force_mode(ethpriv, 0);
	mdelay(100);

	/* Disable traffic */
	dma_if_rxtx(ethpriv, 0);
	mdelay(200);

	/* Disable all TX/RX interrupts */
	sw_w32(0x00000000, REG(RTL839X_DMA_IF_INTR_MSK));
	sw_w32(0x000fffff, REG(RTL839X_DMA_IF_INTR_STS));
	mdelay(200);
}

static void dma_irq(struct rtl83xx_eth_priv *ethpriv, bool enable)
{
	u32 val = sw_r32(REG(RTL839X_DMA_IF_INTR_MSK));
	u32 rxtx = (0xff << _RTL839X_DMAIFINTRMSK_RX_RUN_OUT) \
		| (0xff << _RTL839X_DMAIFINTRMSK_RX_DONE) \
		| (0x3 << _RTL839X_DMAIFINTRMSK_TX_DONE) \
		| (0x3 << _RTL839X_DMAIFINTRMSK_TX_ALL_DONE);

	if (enable)
		val |= rxtx;
	else
		val &= rxtx;
	sw_w32(val, REG(RTL839X_DMA_IF_INTR_MSK));
}

static irqreturn_t nic_irq_handler(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct rtl83xx_eth_priv *ethpriv = netdev_priv(ndev);
	u32 status = sw_r32(REG(RTL839X_DMA_IF_INTR_STS));
	u32 val;

	spin_lock(&ethpriv->lock);

	/*  Ignore TX interrupt */
	val = 0x3 << _RTL839X_DMAIFINTRSTS_TX_DONE;
	val |= 0x3 << _RTL839X_DMAIFINTRSTS_TX_ALL_DONE;
	if (status & val) {
		/* Clear ISR */
		sw_w32(val, REG(RTL839X_DMA_IF_INTR_STS));
	}

	/* RX interrupt */
	if (status & (0xff << _RTL839X_DMAIFINTRSTS_RX_DONE)) {
		/* Disable RX interrupt */
		val = sw_r32(REG(RTL839X_DMA_IF_INTR_MSK));
		val &= ~(0xff << _RTL839X_DMAIFINTRMSK_RX_DONE);
		sw_w32(val, REG(RTL839X_DMA_IF_INTR_MSK));
		sw_w32(0xff << _RTL839X_DMAIFINTRSTS_RX_DONE, REG(RTL839X_DMA_IF_INTR_STS));
		napi_schedule(&ethpriv->napi);
	}

	/* RX buffer overrun */
	if (status & (0xff << _RTL839X_DMAIFINTRSTS_RX_RUN_OUT)) {
		sw_w32(0xff << _RTL839X_DMAIFINTRSTS_RX_RUN_OUT, REG(RTL839X_DMA_IF_INTR_STS));
		rtl83xx_rb_cleanup(ethpriv);
	}

	spin_unlock(&ethpriv->lock);

	return IRQ_HANDLED;
}

const struct rtl83xx_soc rtl839x_reg = {
	.init_mac = rtl839x_init_mac,
	.get_mac_addr = rtl839x_get_mac_addr,
	.set_mac_addr = rtl839x_set_mac_addr,
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

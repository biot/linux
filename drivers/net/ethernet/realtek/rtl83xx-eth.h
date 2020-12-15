// SPDX-License-Identifier: GPL-2.0-only
#ifndef _RTL83XX_ETH_H
#define _RTL83XX_ETH_H

#include <linux/netdevice.h>
#include <linux/phylink.h>

#define REG(x)		(ethpriv->iobase + x)

struct rtl83xx_eth_priv;

struct rtl83xx_soc {
	void (*init_mac)(struct rtl83xx_eth_priv *ethpriv);
	void (*get_mac_addr)(struct rtl83xx_eth_priv *ethpriv, u8 *mac);
	void (*set_mac_addr)(struct rtl83xx_eth_priv *ethpriv, u8 *mac);
	void (*dma_if_fetch)(struct rtl83xx_eth_priv *ethpriv, bool enable);
	void (*mac_port_rxtx)(struct rtl83xx_eth_priv *ethpriv, bool enable);
	void (*mac_force_mode)(struct rtl83xx_eth_priv *ethpriv, bool enable);
	void (*dma_rx_ring_membase)(struct rtl83xx_eth_priv *ethpriv, int ring, u32 membase);
	void (*dma_tx_ring_membase)(struct rtl83xx_eth_priv *ethpriv, int ring, u32 membase);
	void (*dma_rx_ring_size)(struct rtl83xx_eth_priv *ethpriv, int ring, u32 size);
	void (*dma_rx_ring_cntr_get)(struct rtl83xx_eth_priv *ethpriv, int ring, u32 *value);
	void (*dma_rx_ring_cntr_set)(struct rtl83xx_eth_priv *ethpriv, int ring, u32 value);
	void (*dma_rx_ring_cur_get)(struct rtl83xx_eth_priv *ethpriv, int ring, u32 *value);
	irqreturn_t (*nic_irq_handler)(int irq, void *dev_id);
	void (*enable_rxtx)(struct rtl83xx_eth_priv *ethpriv);
	void (*reset)(struct rtl83xx_eth_priv *ethpriv);
	void (*stop)(struct rtl83xx_eth_priv *ethpriv);
	void (*dma_irq)(struct rtl83xx_eth_priv *ethpriv, bool enable);
};

struct rtl83xx_eth_priv {
	struct net_device *netdev;
	struct platform_device *pdev;
	void *iobase;
	void *ringset;
	dma_addr_t dma_handle;
	unsigned int soc_id;
	spinlock_t lock;
	struct napi_struct napi;
	struct phylink *phylink;
	struct phylink_config phylink_config;
	const struct rtl83xx_soc *soc;
	u8 cpu_port;
};

/* Table control */
enum rtl_table_idx {
	RTL838X_TBL_0,
	RTL838X_TBL_1,
	RTL838X_TBL_2,
	RTL838X_TBL_L2,

	RTL839X_TBL_0,
	RTL839X_TBL_1,
	RTL839X_TBL_2,
	RTL839X_TBL_L2,
};

#define RTL_TBL_DATA(reg_offset, idx)		(reg_offset + ((idx) << 2))

void rtl_table_read(struct rtl83xx_eth_priv *ethpriv, int tableidx,
		    int subtable, u32 address, u32 *data);
void rtl_table_write(struct rtl83xx_eth_priv *ethpriv, int tableidx,
		    int subtable, u32 address, u32 *data);


/*
 * Maximum number of RX rings is 8, assigned by switch based on
 * packet/port priortity (not implemented)
 * Maximum number of TX rings is 2 (only ring 0 used)
 * RX ringlength needs to be at least 200, otherwise CPU and Switch
 * may gridlock.
 */
#define RTL_RX_NUM_RINGS		2
#define RTL_RX_RING_LEN			300
#define RTL_TX_NUM_RINGS		2
#define RTL_TX_RING_LEN			160
#define WRAP				0x2
#define RTL_FRAMEBUF_SIZE		1600

#define RTL838X_CPU_PORT			28
#define RTL839X_CPU_PORT			52

struct p_hdr {
	uint8_t		*buf;
	uint16_t	reserved;
	uint16_t	size;   /* buffer size */
	uint16_t	offset;
	uint16_t	len;    /* pkt len */
	uint16_t	reserved2;
	uint16_t	cpu_tag[5];
} __attribute__ ((aligned(1), packed));

struct ringset {
	uint32_t	rx_r[RTL_RX_NUM_RINGS][RTL_RX_RING_LEN];
	uint32_t	tx_r[RTL_TX_NUM_RINGS][RTL_TX_RING_LEN];
	struct	p_hdr	rx_header[RTL_RX_NUM_RINGS][RTL_RX_RING_LEN];
	struct	p_hdr	tx_header[RTL_TX_NUM_RINGS][RTL_TX_RING_LEN];
	uint32_t	c_rx[RTL_RX_NUM_RINGS];
	uint32_t	c_tx[RTL_TX_NUM_RINGS];
	uint8_t		rx_space[RTL_RX_NUM_RINGS * RTL_RX_RING_LEN * RTL_FRAMEBUF_SIZE];
	uint8_t		tx_space[RTL_TX_RING_LEN * RTL_FRAMEBUF_SIZE];
};



/* ----- old -------------------- */
// TODO: move to DT
// #define RTL838X_SW_BASE_ETH ((volatile void __iomem *)0xBB000000)

#define sw_r32(reg)		__raw_readl(reg)
#define sw_w32(val, reg)	__raw_writel(val, reg)

// TODO: move to DT
#define RTL83XX_SWITCH_BASE			((volatile void *) 0xBB000000)
#define RTL838X_SMI_ACCESS_PHY_CTRL_1		(RTL83XX_SWITCH_BASE + 0xa1bc)

#endif /* _RTL83XX_ETH_H */

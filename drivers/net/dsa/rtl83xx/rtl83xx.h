/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _NET_DSA_RTL83XX_H
#define _NET_DSA_RTL83XX_H

#include <net/dsa.h>

#define RTL8380_FAMILY_ID	0x8380
#define RTL8390_FAMILY_ID	0x8390

#define RTL8380_VERSION_A 'A'
#define RTL8390_VERSION_A 'A'
#define RTL8380_VERSION_B 'B'

#define RTL838X_SW_BASE_DSA	(0xBB000000)

#define sw_r32(reg)		__raw_readl(RTL838X_SW_BASE_DSA + reg)
#define sw_w32(val, reg)	__raw_writel(val, RTL838X_SW_BASE_DSA + reg)
#define sw_w32_mask(clear, set, reg) \
				sw_w32((sw_r32(reg) & ~(clear)) | (set), reg)


struct fdb_update_work {
	struct work_struct work;
	struct net_device *ndev;
	u64 macs[];
};

#define MIB_DESC(_size, _offset, _name) {.size = _size, .offset = _offset, .name = _name}
struct rtl83xx_mib_desc {
	unsigned int size;
	unsigned int offset;
	const char *name;
};

struct rtl83xx_reg {
	void (*mask_port_reg_be)(u64 clear, u64 set, int reg);
	void (*set_port_reg_be)(u64 set, int reg);
	u64 (*get_port_reg_be)(int reg);
	void (*mask_port_reg_le)(u64 clear, u64 set, int reg);
	void (*set_port_reg_le)(u64 set, int reg);
	u64 (*get_port_reg_le)(int reg);
	int stat_port_rst;
	int stat_rst;
	int (*stat_port_std_mib)(int p);
	int (*port_iso_ctrl)(int p);
	int l2_ctrl_0;
	int l2_ctrl_1;
	int l2_port_aging_out;
	int smi_poll_ctrl;
	int l2_tbl_flush_ctrl;
	void (*exec_tbl0_cmd)(u32 cmd);
	void (*exec_tbl1_cmd)(u32 cmd);
	int (*tbl_access_data_0)(int i);
	int isr_glb_src;
	int isr_port_link_sts_chg;
	int imr_port_link_sts_chg;
	int imr_glb;
	void (*vlan_tables_read)(u32 vlan, struct rtl838x_vlan_info *info);
	void (*vlan_set_tagged)(u32 vlan, struct rtl838x_vlan_info *info);
	void (*vlan_set_untagged)(u32 vlan, u64 portmask);
	int  (*mac_force_mode_ctrl)(int port);
	int  (*mac_port_ctrl)(int port);
	int  (*l2_port_new_salrn)(int port);
	int  (*l2_port_new_sa_fwd)(int port);
	int  (*mir_ctrl)(int group);
	int  (*mir_dpm)(int group);
	int  (*mir_spm)(int group);
	int mac_link_sts;
	int mac_link_dup_sts;
	int  (*mac_link_spd_sts)(int port);
	int mac_rx_pause_sts;
	int mac_tx_pause_sts;
	u64 (*read_l2_entry_using_hash)(u32 hash, u32 position, struct rtl838x_l2_entry *e);
	u64 (*read_cam)(int idx, struct rtl838x_l2_entry *e);
	int (*vlan_profile)(int profile);
	int (*vlan_port_egr_filter)(int port);
	int (*vlan_port_igr_filter)(int port);
	int (*vlan_port_pb)(int port);
};

struct rtl838x_switch_priv {
	/* Switch operation */
	struct dsa_switch *ds;
	struct device *dev;
	u16 id;
	u16 family_id;
	char version;
	struct rtl838x_port ports[54]; /* TODO: correct size! */
	struct mutex reg_mutex;
	int link_state_irq;
	int mirror_group_ports[4];
	struct mii_bus *mii_bus;
	const struct rtl83xx_reg *r;
	u8 cpu_port;
	u8 port_mask;
	u32 fib_entries;
	struct dentry *dbgfs_dir;
};

void __init rtl83xx_storm_control_init(struct rtl838x_switch_priv *priv);

/* RTL838x-specific */
u32 rtl838x_hash(struct rtl838x_switch_priv *priv, u64 seed);
irqreturn_t rtl838x_switch_irq(int irq, void *dev_id);
void rtl838x_vlan_profile_dump(int index);
int rtl83xx_dsa_phy_read(struct dsa_switch *ds, int phy_addr, int phy_reg);

/* RTL839x-specific */
u32 rtl839x_hash(struct rtl838x_switch_priv *priv, u64 seed);
irqreturn_t rtl839x_switch_irq(int irq, void *dev_id);
void rtl839x_vlan_profile_dump(int index);
int rtl83xx_dsa_phy_write(struct dsa_switch *ds, int phy_addr, int phy_reg, u16 val);

#endif /* _NET_DSA_RTL83XX_H */

// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>

#include "rtl83xx-eth.h"
#include "rtl838x-eth.h"
#include "rtl839x-eth.h"


struct rtl_tables_s {
	u32 ctrl_reg;
	u32 data_base;
	/* Bit offsets into ctrl_reg */
	int exec;
	int op;
	int subtable;
	int addrsize;
	int op_read;
	int op_write;
};

static struct rtl_tables_s rtl_tables[] = {
	[RTL838X_TBL_0] = {
		.ctrl_reg = RTL838X_TBL_ACCESS_CTRL_0,
		.data_base = RTL838X_TBL_ACCESS_DATA_0,
		.exec = 15,
		.op = 14,
		.subtable = 12,
		.addrsize = 12,
		.op_read = 1,
		.op_write = 0,
	},
	[RTL838X_TBL_1] = {
		.ctrl_reg = RTL838X_TBL_ACCESS_CTRL_1,
		.data_base = RTL838X_TBL_ACCESS_DATA_1,
		.exec = 15,
		.op = 14,
		.subtable = 12,
		.addrsize = 12,
		.op_read = 1,
		.op_write = 0,
	},
	[RTL838X_TBL_L2] = {
		.ctrl_reg = RTL838X_TBL_ACCESS_L2_CTRL,
		.data_base = RTL838X_TBL_ACCESS_L2_DATA,
		.exec = 16,
		.op = 15,
		.subtable = 13,
		.addrsize = 13,
		.op_read = 1,
		.op_write = 0,
	},

	[RTL839X_TBL_0] = {
		.ctrl_reg = RTL839X_TBL_ACCESS_CTRL_0,
		.data_base = RTL839X_TBL_ACCESS_DATA_0,
		.exec = 16,
		.op = 15,
		.subtable = 12,
		.addrsize = 12,
		.op_read = 0,
		.op_write = 1,
	},
	[RTL839X_TBL_1] = {
		.ctrl_reg = RTL839X_TBL_ACCESS_CTRL_1,
		.data_base = RTL839X_TBL_ACCESS_DATA_1,
		.exec = 15,
		.op = 14,
		.subtable = 12,
		.addrsize = 12,
		.op_read = 0,
		.op_write = 1,
	},
	[RTL839X_TBL_2] = {
		.ctrl_reg = RTL839X_TBL_ACCESS_CTRL_2,
		.data_base = RTL839X_TBL_ACCESS_DATA_2,
		.exec = 9,
		.op = 8,
		.subtable = 6,
		.addrsize = 5,
		.op_read = 0,
		.op_write = 1,
	},
	[RTL839X_TBL_L2] = {
		.ctrl_reg = RTL839X_TBL_ACCESS_L2_CTRL,
		.data_base = RTL839X_TBL_ACCESS_L2_DATA,
		.exec = 17,
		.op = 16,
		.subtable = 14,
		.addrsize = 14,
		.op_read = 0,
		.op_write = 1,
	},
};

static void run_table_cmd(struct rtl83xx_eth_priv *ethpriv, struct rtl_tables_s *tbl, u32 cmd)
{
	sw_w32(cmd, REG(tbl->ctrl_reg));

	while (sw_r32(REG(tbl->ctrl_reg)) & (BIT(tbl->exec)))
		cpu_relax();
}

void rtl_table_read(struct rtl83xx_eth_priv *ethpriv, int tableidx,
		    int subtable, u32 address, u32 *data)
{
	struct rtl_tables_s *tbl = &rtl_tables[tableidx];
	u32 cmd;

	cmd = BIT(tbl->exec) | tbl->op_read << tbl->op | subtable << tbl->subtable \
		| (address & tbl->addrsize);
	run_table_cmd(ethpriv, tbl, cmd);

	data[0] = sw_r32(REG(RTL_TBL_DATA(tbl->data_base, 0)));
	data[1] = sw_r32(REG(RTL_TBL_DATA(tbl->data_base, 1)));
	data[2] = sw_r32(REG(RTL_TBL_DATA(tbl->data_base, 2)));
	pr_info("%s: table %d cmd %.4x read %x %x %x\n", __func__, tableidx,
		cmd, data[0], data[1], data[2]);
}

void rtl_table_write(struct rtl83xx_eth_priv *ethpriv, int tableidx,
		     int subtable, u32 address, u32 *data)
{
	struct rtl_tables_s *tbl = &rtl_tables[tableidx];
	u32 cmd;

	sw_w32(data[0], REG(RTL_TBL_DATA(tbl->data_base, 0)));
	sw_w32(data[1], REG(RTL_TBL_DATA(tbl->data_base, 1)));
	sw_w32(data[2], REG(RTL_TBL_DATA(tbl->data_base, 2)));

	cmd = BIT(tbl->exec) | tbl->op_write << tbl->op | subtable << tbl->subtable \
		| (address & tbl->addrsize);
	run_table_cmd(ethpriv, tbl, cmd);
	pr_info("%s: table %d cmd %.4x wrote %x %x %x\n", __func__, tableidx,
		cmd, data[0], data[1], data[2]);
}

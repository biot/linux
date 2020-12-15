/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _NET_DSA_RTL839X_H
#define _NET_DSA_RTL839X_H

#include <net/dsa.h>

/* Address table lookup */
#define RTL839X_L2_CTRL_0			0x3800
#define   _L2CTRL_ALL_ZERO_SA_TRAP		BIT(18)
#define   _L2CTRL_ALL_ZERO_SA_DROP		BIT(17)
#define   _L2CTRL_MC_BC_SA_TRAP			BIT(16)
#define   _L2CTRL_SECURE_SA			BIT(15)
#define   _L2CTRL_FLUSH_NOTIFY_EN		BIT(14)
#define   _L2CTRL_FORBID_ACT			12
#define   _L2CTRL_LUTCAM_EN			BIT(11)
#define   _L2CTRL_PPPOE_PARSE_EN		BIT(10)
#define   _L2CTRL_MC_BC_SA_DROP			BIT(9)
#define   _L2CTRL_IP_MC_DIP_CHK			BIT(8)
#define   _L2CTRL_LINK_DOWN_P_INVLD		BIT(7)
#define   _L2CTRL_SA_ALL_ZERO_LRN		BIT(6)
#define   _L2CTRL_IP_MC_FVID_CMP		BIT(5)
#define   _L2CTRL_IPV6_MC_HASH_KEY_FMT		3
#define   _L2CTRL_IPV4_MC_HASH_KEY_FMT		1
#define   _L2CTRL_L2_HASH_ALGO			BIT(0)

#endif /* _NET_DSA_RTL839X_H */

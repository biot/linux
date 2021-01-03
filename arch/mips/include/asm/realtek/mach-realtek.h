// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#ifndef _MACH_REALTEK_RTL_H_
#define _MACH_REALTEK_RTL_H_

/* Used to detect address length pin strapping on RTL833x/RTL838x */
#define RTL_INT_RW_CTRL		(RTL_SWITCH_BASE + 0x58)
#define RTL_EXT_VERSION		(RTL_SWITCH_BASE + 0xD0)
#define RTL_PLL_CML_CTRL	(RTL_SWITCH_BASE + 0xFF8)
#define RTL_STRAP_DBG		(RTL_SWITCH_BASE + 0x100C)

#endif /* _MACH_REALTEK_RTL_H_ */

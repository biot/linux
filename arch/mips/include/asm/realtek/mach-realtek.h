// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#ifndef _MACH_REALTEK_H_
#define _MACH_REALTEK_H_

struct realtek_soc_info {
	unsigned char *name;
	unsigned int id;
	unsigned int family;
};

/* High 16 bits of MODEL_NAME_INFO register */
#define RTL8389_FAMILY_ID		0x8389
#define RTL8328_FAMILY_ID		0x8328
#define RTL8390_FAMILY_ID		0x8390
#define RTL8350_FAMILY_ID		0x8350
#define RTL8380_FAMILY_ID		0x8380
#define RTL8330_FAMILY_ID		0x8330

/* Interrupt numbers/bits */
#define RTL8380_IRQ_UART0		31
#define RTL8380_IRQ_UART1		30
#define RTL8380_IRQ_TC0			29
#define RTL8380_IRQ_TC1			28
#define RTL8380_IRQ_OCPTO		27
#define RTL8380_IRQ_HLXTO		26
#define RTL8380_IRQ_SLXTO		25
#define RTL8380_IRQ_NIC			24
#define RTL8380_IRQ_GPIO_ABCD		23
#define RTL8380_IRQ_GPIO_EFGH		22
#define RTL8380_IRQ_RTC			21
#define RTL8380_IRQ_SWCORE		20
#define RTL8380_IRQ_WDT_IP1		19
#define RTL8380_IRQ_WDT_IP2		18


// TODO: move to DT
#define RTL8380_SWITCH_BASE		((volatile void *) 0xBB000000)
#define RTL8380_MODEL_NAME_INFO		(RTL8380_SWITCH_BASE + 0x00D4)
#define RTL8390_MODEL_NAME_INFO		(RTL8380_SWITCH_BASE + 0x0FF0)

/* Used to detect address length pin strapping on RTL833x/RTL838x */
#define RTL8380_INT_RW_CTRL		(RTL8380_SWITCH_BASE + 0x58)
#define RTL8380_EXT_VERSION		(RTL8380_SWITCH_BASE + 0xD0)
#define RTL8380_PLL_CML_CTRL		(RTL8380_SWITCH_BASE + 0xFF8)
#define RTL8380_STRAP_DBG		(RTL8380_SWITCH_BASE + 0x100C)

#endif /* _MACH_RTL8380_H_ */

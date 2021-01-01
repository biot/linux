// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#ifndef _MACH_REALTEK_RTL_H_
#define _MACH_REALTEK_RTL_H_

/* Global Interrupt Mask Register */
#define RTL_ICTL_GIMR		0x00
/* Global Interrupt Status Register */
#define RTL_ICTL_GISR		0x04
/* Interrupt routing register */
#define RTL_ICTL_IRR0		0x08
#define RTL_ICTL_IRR1		0x0c
#define RTL_ICTL_IRR2		0x10
#define RTL_ICTL_IRR3		0x14

/* Cascade map */
#define RTL_IRQ_CASCADE_UART0		2
#define RTL_IRQ_CASCADE_UART1		1
#define RTL_IRQ_CASCADE_TC0		5
#define RTL_IRQ_CASCADE_TC1		1
#define RTL_IRQ_CASCADE_OCPTO		1
#define RTL_IRQ_CASCADE_HLXTO		1
#define RTL_IRQ_CASCADE_SLXTO		1
#define RTL_IRQ_CASCADE_NIC		4
#define RTL_IRQ_CASCADE_GPIO_ABCD	4
#define RTL_IRQ_CASCADE_GPIO_EFGH	4
#define RTL_IRQ_CASCADE_RTC		4
#define RTL_IRQ_CASCADE_SWCORE		3
#define RTL_IRQ_CASCADE_WDT_IP1		4
#define RTL_IRQ_CASCADE_WDT_IP2		5

/* Pack cascade map into interrupt routing registers */
#define RTL_ICTL_IRR0_SETTING (\
	(RTL_IRQ_CASCADE_UART0		<< 28) | \
	(RTL_IRQ_CASCADE_UART1		<< 24) | \
	(RTL_IRQ_CASCADE_TC0		<< 20) | \
	(RTL_IRQ_CASCADE_TC1		<< 16) | \
	(RTL_IRQ_CASCADE_OCPTO		<< 12) | \
	(RTL_IRQ_CASCADE_HLXTO		<< 8)  | \
	(RTL_IRQ_CASCADE_SLXTO		<< 4)  | \
	(RTL_IRQ_CASCADE_NIC		<< 0))
#define RTL_ICTL_IRR1_SETTING (\
	(RTL_IRQ_CASCADE_GPIO_ABCD	<< 28) | \
	(RTL_IRQ_CASCADE_GPIO_EFGH	<< 24) | \
	(RTL_IRQ_CASCADE_RTC		<< 20) | \
	(RTL_IRQ_CASCADE_SWCORE		<< 16))
#define RTL_ICTL_IRR2_SETTING		0
#define RTL_ICTL_IRR3_SETTING		0

/* Used to detect address length pin strapping on RTL833x/RTL838x */
#define RTL_INT_RW_CTRL		(RTL_SWITCH_BASE + 0x58)
#define RTL_EXT_VERSION		(RTL_SWITCH_BASE + 0xD0)
#define RTL_PLL_CML_CTRL	(RTL_SWITCH_BASE + 0xFF8)
#define RTL_STRAP_DBG		(RTL_SWITCH_BASE + 0x100C)

#endif /* _MACH_REALTEK_RTL_H_ */

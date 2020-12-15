// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#ifndef _RTL8380_IRQ_H_
#define _RTL8380_IRQ_H_

#define NR_IRQS 32
#include_next <irq.h>

/* Global Interrupt Mask Register */
#define RTL8380_ICTL_GIMR	0x00
/* Global Interrupt Status Register */
#define RTL8380_ICTL_GISR	0x04

/* Cascaded interrupts */
#define RTL8380_CPU_IRQ_SHARED0		(MIPS_CPU_IRQ_BASE + 2)
#define RTL8380_CPU_IRQ_UART		(MIPS_CPU_IRQ_BASE + 3)
#define RTL8380_CPU_IRQ_SWITCH		(MIPS_CPU_IRQ_BASE + 4)
#define RTL8380_CPU_IRQ_SHARED1		(MIPS_CPU_IRQ_BASE + 5)
#define RTL8380_CPU_IRQ_EXTERNAL	(MIPS_CPU_IRQ_BASE + 6)
#define RTL8380_CPU_IRQ_COUNTER		(MIPS_CPU_IRQ_BASE + 7)


/* Interrupt routing register */
#define RTL8380_IRR0		0x08
#define RTL8380_IRR1		0x0c
#define RTL8380_IRR2		0x10
#define RTL8380_IRR3		0x14

/* Cascade map */
#define RTL8380_IRQ_CASCADE_UART0	2
#define RTL8380_IRQ_CASCADE_UART1	1
#define RTL8380_IRQ_CASCADE_TC0		5
#define RTL8380_IRQ_CASCADE_TC1		1
#define RTL8380_IRQ_CASCADE_OCPTO	1
#define RTL8380_IRQ_CASCADE_HLXTO	1
#define RTL8380_IRQ_CASCADE_SLXTO	1
#define RTL8380_IRQ_CASCADE_NIC		4
#define RTL8380_IRQ_CASCADE_GPIO_ABCD	4
#define RTL8380_IRQ_CASCADE_GPIO_EFGH	4
#define RTL8380_IRQ_CASCADE_RTC		4
#define RTL8380_IRQ_CASCADE_SWCORE	3
#define RTL8380_IRQ_CASCADE_WDT_IP1	4
#define RTL8380_IRQ_CASCADE_WDT_IP2	5

/* Pack cascade map into interrupt routing registers */
#define RTL8380_IRR0_SETTING (\
	(RTL8380_IRQ_CASCADE_UART0	<< 28) | \
	(RTL8380_IRQ_CASCADE_UART1	<< 24) | \
	(RTL8380_IRQ_CASCADE_TC0	<< 20) | \
	(RTL8380_IRQ_CASCADE_TC1	<< 16) | \
	(RTL8380_IRQ_CASCADE_OCPTO	<< 12) | \
	(RTL8380_IRQ_CASCADE_HLXTO	<< 8)  | \
	(RTL8380_IRQ_CASCADE_SLXTO	<< 4)  | \
	(RTL8380_IRQ_CASCADE_NIC	<< 0))
#define RTL8380_IRR1_SETTING (\
	(RTL8380_IRQ_CASCADE_GPIO_ABCD	<< 28) | \
	(RTL8380_IRQ_CASCADE_GPIO_EFGH	<< 24) | \
	(RTL8380_IRQ_CASCADE_RTC	<< 20) | \
	(RTL8380_IRQ_CASCADE_SWCORE	<< 16))
#define RTL8380_IRR2_SETTING	0
#define RTL8380_IRR3_SETTING	0

#endif /* _RTL8380_IRQ_H_ */

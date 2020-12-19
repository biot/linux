// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#include <linux/irqchip.h>
#include <linux/spinlock.h>
#include <linux/of_address.h>
#include <asm/irq_cpu.h>
#include <linux/of_irq.h>
#include <asm/cevt-r4k.h>

#include <asm/mach-realtek/mach-realtek.h>

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

#define REG(x)		(realtek_ictl_base + x)

static DEFINE_RAW_SPINLOCK(irq_lock);
static void __iomem *realtek_ictl_base;


static void realtek_ictl_enable_irq(struct irq_data *i)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&irq_lock, flags);

	value = readl(REG(RTL8380_ICTL_GIMR));
	value |= BIT(i->hwirq);
	writel(value, REG(RTL8380_ICTL_GIMR));

	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static void realtek_ictl_disable_irq(struct irq_data *i)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&irq_lock, flags);

	value = readl(REG(RTL8380_ICTL_GIMR));
	value &= ~BIT(i->hwirq);
	writel(value, REG(RTL8380_ICTL_GIMR));

	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static struct irq_chip realtek_ictl_irq = {
	.name = "rtl8380",
	.irq_enable = realtek_ictl_enable_irq,
	.irq_disable = realtek_ictl_disable_irq,
	.irq_ack = realtek_ictl_disable_irq,
	.irq_mask = realtek_ictl_disable_irq,
	.irq_unmask = realtek_ictl_enable_irq,
	.irq_eoi = realtek_ictl_enable_irq,
};

static int intc_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	irq_set_chip_and_handler(hw, &realtek_ictl_irq, handle_level_irq);

	return 0;
}

static const struct irq_domain_ops irq_domain_ops = {
	.map = intc_map,
	.xlate = irq_domain_xlate_onecell,
};

static void realtek_irq_dispatch(struct irq_desc *desc)
{
	unsigned int pending = readl(REG(RTL8380_ICTL_GIMR)) & readl(REG(RTL8380_ICTL_GISR));

	if (pending) {
		struct irq_domain *domain = irq_desc_get_handler_data(desc);
		generic_handle_irq(irq_find_mapping(domain, __ffs(pending)));
	} else {
		spurious_interrupt();
	}
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending;

	pending =  read_c0_cause() & read_c0_status() & ST0_IM;

	if (pending & CAUSEF_IP7)
		do_IRQ(RTL8380_CPU_IRQ_COUNTER);

	else if (pending & CAUSEF_IP6)
		do_IRQ(RTL8380_CPU_IRQ_EXTERNAL);

	else if (pending & CAUSEF_IP5)
		do_IRQ(RTL8380_CPU_IRQ_SHARED1);

	else if (pending & CAUSEF_IP4)
		do_IRQ(RTL8380_CPU_IRQ_SWITCH);

	else if (pending & CAUSEF_IP3)
		do_IRQ(RTL8380_CPU_IRQ_UART);

	else if (pending & CAUSEF_IP2)
		do_IRQ(RTL8380_CPU_IRQ_SHARED0);

	else
		spurious_interrupt();
}

static int __init rtl8380_of_init(struct device_node *node, struct device_node *parent)
{
	struct irq_domain *domain;

	domain = irq_domain_add_simple(node, 32, 0,
				       &irq_domain_ops, NULL);
        irq_set_chained_handler_and_data(2, realtek_irq_dispatch, domain);
        irq_set_chained_handler_and_data(5, realtek_irq_dispatch, domain);

	realtek_ictl_base = of_iomap(node, 0);
	if (!realtek_ictl_base)
		return -ENXIO;

	/* Disable all cascaded interrupts */
	writel(0, REG(RTL8380_ICTL_GIMR));

	/* Set up interrupt routing */
	writel(RTL8380_IRR0_SETTING, REG(RTL8380_IRR0));
	writel(RTL8380_IRR1_SETTING, REG(RTL8380_IRR1));
	writel(RTL8380_IRR2_SETTING, REG(RTL8380_IRR2));
	writel(RTL8380_IRR3_SETTING, REG(RTL8380_IRR3));

	/* Clear timer interrupt */
	write_c0_compare(0);

	/* Enable all CPU interrupts */
	write_c0_status(read_c0_status() | ST0_IM);

	/* Enable timer0 and uart0 interrupts */
	writel(BIT(RTL8380_IRQ_TC0) | BIT(RTL8380_IRQ_UART0), REG(RTL8380_ICTL_GIMR));

	return 0;
}

IRQCHIP_DECLARE(realtek_rtl8380_intc, "realtek,rtl8380-intc", rtl8380_of_init);

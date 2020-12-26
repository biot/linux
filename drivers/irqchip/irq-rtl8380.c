// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#include <linux/of_irq.h>
#include <linux/irqchip.h>
#include <linux/spinlock.h>
#include <linux/of_address.h>
#include <linux/irqchip/chained_irq.h>

#include <mach-realtek.h>

#define REG(x)		(realtek_ictl_base + x)

static DEFINE_RAW_SPINLOCK(irq_lock);
static void __iomem *realtek_ictl_base;

static void realtek_ictl_unmask_irq(struct irq_data *i)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&irq_lock, flags);

	value = readl(REG(RTL8380_ICTL_GIMR));
	value |= BIT(i->hwirq);
	writel(value, REG(RTL8380_ICTL_GIMR));

	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static void realtek_ictl_mask_irq(struct irq_data *i)
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
	.irq_mask = realtek_ictl_mask_irq,
	.irq_unmask = realtek_ictl_unmask_irq,
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
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct irq_domain *domain;
	unsigned int pending;

	chained_irq_enter(chip, desc);
	pending = readl(REG(RTL8380_ICTL_GIMR)) & readl(REG(RTL8380_ICTL_GISR));
	if (unlikely(!pending)) {
		spurious_interrupt();
		goto out;
	}
	domain = irq_desc_get_handler_data(desc);
	generic_handle_irq(irq_find_mapping(domain, __ffs(pending)));

out:
	chained_irq_exit(chip, desc);
}

static int __init rtl8380_of_init(struct device_node *node, struct device_node *parent)
{
	struct irq_domain *domain;

	domain = irq_domain_add_simple(node, 32, 0,
				       &irq_domain_ops, NULL);
	irq_set_chained_handler_and_data(2, realtek_irq_dispatch, domain);
	irq_set_chained_handler_and_data(3, realtek_irq_dispatch, domain);
	irq_set_chained_handler_and_data(4, realtek_irq_dispatch, domain);
	irq_set_chained_handler_and_data(5, realtek_irq_dispatch, domain);

	realtek_ictl_base = of_iomap(node, 0);
	if (!realtek_ictl_base)
		return -ENXIO;

	/* Disable all cascaded interrupts */
	writel(0, REG(RTL8380_ICTL_GIMR));

	/*
	 * Set up interrupt routing - this defines the mapping between
	 * cpu and realtek interrupt controller. These values are static
	 * and taken from the SDK code.
	 */
	writel(RTL8380_ICTL_IRR0_SETTING, REG(RTL8380_ICTL_IRR0));
	writel(RTL8380_ICTL_IRR1_SETTING, REG(RTL8380_ICTL_IRR1));
	writel(RTL8380_ICTL_IRR2_SETTING, REG(RTL8380_ICTL_IRR2));
	writel(RTL8380_ICTL_IRR3_SETTING, REG(RTL8380_ICTL_IRR3));

	/* Clear timer interrupt */
	write_c0_compare(0);

	return 0;
}

IRQCHIP_DECLARE(realtek_rtl8380_intc, "realtek,rtl8380-intc", rtl8380_of_init);

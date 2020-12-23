/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RTL8380_IOREMAP_H_
#define _RTL8380_IOREMAP_H_

#include <linux/of.h>

static inline int is_rtl8380_internal_registers(phys_addr_t offset)
{
	struct device_node *np = NULL;
	const __be32 *prop;
	int lenp;
	u32 start, stop;

	if (offset & BIT(31))
		/* already mapped into register space */
		return 1;

	do {
		np = of_find_node_with_property(np, "ranges");
		if (!np)
			continue;
		prop = of_get_property(np, "ranges", &lenp);
		if (lenp != 12)
			continue;
		start = be32_to_cpup(prop + 1);
		stop = start + be32_to_cpup(prop + 2);
		of_node_put(np);
		if (offset >= start && offset < stop)
			return 1;

	} while (np);
	return 0;
}

static inline void __iomem *plat_ioremap(phys_addr_t offset, unsigned long size,
					 unsigned long flags)
{
	if (is_rtl8380_internal_registers(offset))
		return (void __iomem *)offset;
	return NULL;
}

static inline int plat_iounmap(const volatile void __iomem *addr)
{
	return is_rtl8380_internal_registers((unsigned long)addr);
}

#endif /* _RTL8380_IOREMAP_H_ */

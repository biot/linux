// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu <tonywu@realtek.com>
 * Copyright (C) 2020 Birger Koblitz <mail@birger-koblitz.de>
 * Copyright (C) 2020 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2020 John Crispin <john@phrozen.org>
 */

#include <linux/of_fdt.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <mach-realtek.h>

extern struct realtek_soc_info soc_info;

static void realtek_restart(char *command)
{
	if (soc_info.family == RTL8380_FAMILY_ID) {
		/* Reset Global Control1 Register */
		writel(1, RTL8380_RST_GLB_CTRL_1);
	} else if (soc_info.family == RTL8390_FAMILY_ID) {
		/* If calling reset vector fails, reset entire chip */
		writel(0xFFFFFFFF, RTL8390_RST_GLB_CTRL);
	}
}

void __init plat_mem_setup(void)
{
	set_io_port_base(KSEG1);

	_machine_restart = realtek_restart;
}

void __init plat_time_init(void)
{
	struct device_node *np;
	u32 freq = 500000000;

	np = of_find_node_by_name(NULL, "cpus");
	if (!np) {
		pr_err("Missing 'cpus' DT node, using default frequency.");
	} else {
		if (of_property_read_u32(np, "frequency", &freq) < 0)
			pr_err("No 'frequency' property in DT, using default.");
		else
			pr_info("CPU frequency from device tree: %d", freq);
		of_node_put(np);
	}

	mips_hpt_frequency = freq / 2;
}

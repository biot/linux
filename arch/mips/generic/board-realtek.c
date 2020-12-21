// SPDX-License-Identifier: GPL-2.0-only

#include <asm/setup.h>
#include <asm/machine.h>
#include <linux/init.h>
#include <linux/printk.h>

#include <mach-realtek.h>

struct realtek_soc_info soc_info;


static int __init realtek_init(void)
{
	uint32_t model;

	/* uart0 */
	setup_8250_early_printk_port(0xb8002000, 2, 0);

	model = readl(RTL8380_MODEL_NAME_INFO) >> 16;
	if (model != 0x8330 && model != 0x8332 &&
	    model != 0x8380 && model != 0x8382 )
		model = readl(RTL8390_MODEL_NAME_INFO) >> 16;

	soc_info.id = model;

	switch (model) {
		case 0x8328:
			soc_info.name="RTL8328";
			soc_info.family = RTL8328_FAMILY_ID;
			break;
		case 0x8332:
			soc_info.name="RTL8332";
			soc_info.family = RTL8380_FAMILY_ID;
			break;
		case 0x8380:
			soc_info.name="RTL8380";
			soc_info.family = RTL8380_FAMILY_ID;
			break;
		case 0x8382:
			soc_info.name="RTL8382";
			soc_info.family = RTL8380_FAMILY_ID;
			break;
		case 0x8390:
			soc_info.name="RTL8390";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		case 0x8391:
			soc_info.name="RTL8391";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		case 0x8392:
			soc_info.name="RTL8392";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		case 0x8393:
			soc_info.name="RTL8393";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		default:
			soc_info.name="DEFAULT";
			soc_info.family = 0;
	}
	pr_info("SoC Type: %s\n", soc_info.name);

	return true;
}

early_initcall(realtek_init);


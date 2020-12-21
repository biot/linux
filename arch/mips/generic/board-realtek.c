// SPDX-License-Identifier: GPL-2.0-only

#include <asm/setup.h>
#include <asm/machine.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/printk.h>

#include <mach-realtek.h>

struct realtek_soc_info soc_info;

#define REALTEK_SOC_NAME_MAX_LEN	32
static char model_name[REALTEK_SOC_NAME_MAX_LEN] = "Unknown Realtek";

#define REALTEK_SOC_COMPATIBLE_8380	0x8380
#define REALTEK_SOC_COMPATIBLE_8390	0x8390
#define REALTEK_SOC_COMPATIBLE_9300	0x9300
#define REALTEK_SOC_COMPATIBLE_9310	0x9310

#define RTL8380_MODEL_NAME_INFO_REG	0x00D4
#define RTL8380_MODEL_EXT_VERSION_REG	0x00D0

#define RTL8390_MODEL_NAME_INFO_REG	0x0FF0
#define RTL9300_MODEL_NAME_INFO_REG	0x4
#define RTL9310_MODEL_NAME_INFO_REG	RTL9300_MODEL_NAME_INFO_REG

static inline int realtek_soc_family(uint32_t model)
{
	return (model >> 16) & 0xfff0;
}

static inline char realtek_int_to_char(unsigned letter)
{
	return 'A' + letter - 1;
}

static void __iomem *realtek_of_iobase(struct device_node *np, int offset)
{
	void __iomem *base;

	if (of_property_read_u32_index(np, "ranges", 1, (u32 *)&base))
		return ERR_PTR(-EINVAL);

	return base;
}

static void __init realtek_read_soc_info(struct device_node *np,
	int soc_compatible, struct realtek_soc_info *soc_info)
{
	void __iomem *base;
	uint32_t model, chip_rev;
	bool is_engineering_sample;
	char model_letters[4];
	unsigned letter, max_letters;
	char revision = '0';
	unsigned i;

	base = realtek_of_iobase(np, 0);
	if (IS_ERR(base)) {
		pr_err("failed to remap switch-bus");
		return;
	}

	switch (soc_compatible) {
	case REALTEK_SOC_COMPATIBLE_8380:
		model = readl(base + RTL8380_MODEL_NAME_INFO_REG);
		chip_rev = readl(base + RTL8380_MODEL_EXT_VERSION_REG);
		max_letters = 3;
		break;
	case REALTEK_SOC_COMPATIBLE_8390:
		model = readl(base + RTL8390_MODEL_NAME_INFO_REG);
		chip_rev = (model >> 1) & 0x1f;
		is_engineering_sample = !!chip_rev;
		max_letters = 2;
		break;
	case REALTEK_SOC_COMPATIBLE_9300:
	case REALTEK_SOC_COMPATIBLE_9310:
		model = readl(base + RTL9300_MODEL_NAME_INFO_REG);
		chip_rev = model & 0xf;
		is_engineering_sample = (model >> 4) & 0x1;
		max_letters = 2;
		break;
	default:
		return;
	}

	if (soc_compatible != realtek_soc_family(model)) {
		pr_err("SoC family (%04x) does not match expected value (%04x)",
			realtek_soc_family(model), soc_compatible);
		return;
	}

	soc_info->family = realtek_soc_family(model);
	soc_info->id = model >> 16;

	for (i = 0; i < max_letters; i++) {
		letter = (model >> (16 - 5*(i+1))) & 0x1f;
		if (letter)
			model_letters[i] = realtek_int_to_char(letter);
	}

	if (chip_rev)
		revision = realtek_int_to_char(chip_rev);

	snprintf(model_name, REALTEK_SOC_NAME_MAX_LEN-1,
		"RTL%04x%s rev. %c%s",
		soc_info->id, model_letters, revision,
		is_engineering_sample ? "-ES" : "");

	soc_info->name = model_name;
}

static const struct of_device_id of_realtek_soc_match[] = {
	{
		.compatible = "realtek,rtl8380-soc",
		.data = (void *) REALTEK_SOC_COMPATIBLE_8380
	},
	{
		.compatible = "realtek,rtl8390-soc",
		.data = (void *) REALTEK_SOC_COMPATIBLE_8390
	},
	{
		.compatible = "realtek,rtl9300-soc",
		.data = (void *) REALTEK_SOC_COMPATIBLE_9300
	},
	{
		.compatible = "realtek,rtl9310-soc",
		.data = (void *) REALTEK_SOC_COMPATIBLE_9310
	},
	{}
};

static int __init realtek_init(void)
{
	const struct of_device_id *match;
	struct device_node *np_bus;

	/* uart0 */
	setup_8250_early_printk_port(0xb8002000, 2, 0);

	soc_info.name = model_name;

	match = of_match_node(of_realtek_soc_match, of_root);
	np_bus = of_find_node_by_name(of_root, "switch-bus");
	if (match && np_bus)
		realtek_read_soc_info(np_bus, (int) match->data, &soc_info);

	of_node_put(np_bus);

	pr_info("SoC is %s\n", soc_info.name);

	return true;
}

early_initcall(realtek_init);


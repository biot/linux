// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sys_soc.h>

#define REALTEK_SOC_COMPATIBLE_8380	0x8380
#define REALTEK_SOC_COMPATIBLE_8390	0x8390
#define REALTEK_SOC_COMPATIBLE_9300	0x9300
#define REALTEK_SOC_COMPATIBLE_9310	0x9310

#define RTL8380_REG_PROTECT		0x00
#define RTL8380_MODEL_EXT_VERSION	0x00
#define RTL8380_MODEL_NAME		0x04
#define RTL8380_CHIP_INFO		0x08
#define RTL8380_MODEL_INFO		0x0c

#define RTL8390_MODEL_NAME		0x00
#define RTL8390_CHIP_INFO		0x04

#define RTL9300_MODEL_NAME		0x00
#define RTL9300_CHIP_INFO		0x04
#define RTL9310_MODEL_NAME		RTL9300_MODEL_NAME

#define REALTEK_REG_PROTECT_READ	BIT(0)
#define REALTEK_REG_PROTECT_WRITE	BIT(1)
#define REALTEK_REG_PROTECT_READ_WRITE	\
	(REALTEK_REG_PROTECT_READ | REALTEK_REG_PROTECT_WRITE)

static inline int realtek_soc_id(u32 model)
{
	return (model >> 16);
}

static inline int realtek_soc_family(u32 model)
{
	return realtek_soc_id(model) & 0xfff0;
}

static inline int realtek_soc_5b_char(u32 model, int index)
{
	return ((model >> (16 - 5*(index+1))) & 0x1f);
}

static inline char realtek_5b_char(unsigned letter)
{
	if (letter && letter <= 26)
		return 'A' + letter - 1;
	else
		return '\0';
}

static const struct of_device_id of_realtek_chipid_match[] = {
	{
		.compatible = "realtek,rtl8380-chipid",
		.data = (void *) REALTEK_SOC_COMPATIBLE_8380
	},
	{
		.compatible = "realtek,rtl8390-chipid",
		.data = (void *) REALTEK_SOC_COMPATIBLE_8390
	},
	{
		.compatible = "realtek,rtl9300-chipid",
		.data = (void *) REALTEK_SOC_COMPATIBLE_9300
	},
	{
		.compatible = "realtek,rtl9310-chipid",
		.data = (void *) REALTEK_SOC_COMPATIBLE_9310
	},
	{ /* sentinel */ }
};

static inline bool rtl8380_read_modelinfo(struct device *dev,
	struct regmap *regmap, int chip_info_base, int *model,
	int *chip_rev, bool *is_engineering_sample)
{
	int err, reg_rw_protect;
	const __be32 *offset;

	/* FIXME restore register value */
	offset = of_get_address(dev->of_node, 1, NULL, NULL);
	if (!offset)
		dev_warn(dev, "INT_RW register base address found\n");
	else {
		reg_rw_protect = __be32_to_cpu(*offset);
		err = regmap_set_bits(regmap, reg_rw_protect,
			REALTEK_REG_PROTECT_READ_WRITE);
		if (err)
		    return err;
	}

	err = regmap_read(regmap, chip_info_base + RTL8380_MODEL_NAME, model);
	if (err)
		return err;

	err = regmap_read(regmap, chip_info_base + RTL8380_MODEL_EXT_VERSION,
		chip_rev);
	if (err)
		return err;

	/*
	 * FIXME Correct detection of RTL8381M
	 * BIT(23) in register 0x1024 (RTL8380_MODE_DEFINE_CTL)
	 * indicates if the SoC is RTL8380M (1) or RTL8381M (0)
	 * The RTL8381M has only one QSGMII and two SGMII phys
	 */

	return 0;
}

static int realtek_read_modelinfo(struct device *dev, int family_id,
	const char **family, const char **revision, const char **soc_id)
{
	int chip_info_base;
	u32 chip_rev;
	bool is_engineering_sample;
	unsigned letter;
	unsigned max_letters;
	u32 model;
	char model_letters[4];
	const __be32 *offset;
	struct regmap *regmap;
	int err;

	regmap = syscon_node_to_regmap(dev->of_node->parent);
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to get regmap\n");
		return PTR_ERR(regmap);
	}

	offset = of_get_address(dev->of_node, 0, NULL, NULL);
	chip_info_base = __be32_to_cpu(*offset);

	switch (family_id) {
	case REALTEK_SOC_COMPATIBLE_8380:
		max_letters = 3;
		err = rtl8380_read_modelinfo(dev, regmap, chip_info_base,
			&model, &chip_rev, &is_engineering_sample);
		break;
	case REALTEK_SOC_COMPATIBLE_8390:
		max_letters = 2;
		err = regmap_read(regmap, chip_info_base + RTL8390_MODEL_NAME,
			&model);
		chip_rev = (model >> 1) & 0x1f;
		is_engineering_sample = !!chip_rev;
		break;
	case REALTEK_SOC_COMPATIBLE_9300:
	case REALTEK_SOC_COMPATIBLE_9310:
		max_letters = 2;
		err = regmap_read(regmap, chip_info_base + RTL9300_MODEL_NAME,
			&model);
		chip_rev = model & 0xf;
		is_engineering_sample = (model >> 4) & 0x1;
		break;
	default:
		return -EINVAL;
	}

	if (err)
		return err;

	for (letter = 0; letter < max_letters; letter++)
		model_letters[letter] = realtek_5b_char(
			realtek_soc_5b_char(model, letter));
	model_letters[letter] = '\0';

	*family = kasprintf(GFP_KERNEL, "RTL%04x", realtek_soc_family(model));
	*revision = kasprintf(GFP_KERNEL, "%c%s",
		chip_rev ? realtek_5b_char(chip_rev) : '0',
		is_engineering_sample ? "-ES" : "");
	*soc_id = kasprintf(GFP_KERNEL, "RTL%04x%s", realtek_soc_id(model),
		model_letters);

	return 0;
}

static int realtek_chipinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	int match_family;

	soc_dev_attr = devm_kzalloc(dev, sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	match = of_match_node(of_realtek_chipid_match, dev->of_node);
	if (!match) {
		dev_err(dev, "no device match\n");
		return -ENODEV;
	}

	soc_dev_attr->data = match->data;
	match_family = (int) match->data;

	of_property_read_string(NULL, "model", &soc_dev_attr->machine);
	if(realtek_read_modelinfo(dev, match_family, &soc_dev_attr->family,
		&soc_dev_attr->revision, &soc_dev_attr->soc_id)) {
		dev_err(dev, "failed to read model info\n");
		return -EINVAL;
	}

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev))
		goto err_soc_dev;

	dev_info(dev, "Realtek switch SoC is %s rev. %s\n",
		soc_dev_attr->soc_id, soc_dev_attr->revision);

	return 0;

err_soc_dev:
	kfree(soc_dev_attr->family);
	kfree(soc_dev_attr->revision);
	kfree(soc_dev_attr->soc_id);
	return PTR_ERR(soc_dev);
}

static struct platform_driver realtek_chipid_driver = {
	.driver = {
		.name = "realtek-rtl-chipid",
		.of_match_table = of_realtek_chipid_match
	},
	.probe = realtek_chipinfo_probe
};

static int __init realtek_chipid_init(void)
{
	return platform_driver_register(&realtek_chipid_driver);
}
subsys_initcall(realtek_chipid_init);

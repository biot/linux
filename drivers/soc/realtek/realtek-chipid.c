// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sys_soc.h>

#define REALTEK_SOC_COMPATIBLE_8380	0x8380
#define REALTEK_SOC_COMPATIBLE_8390	0x8390
#define REALTEK_SOC_COMPATIBLE_9300	0x9300
#define REALTEK_SOC_COMPATIBLE_9310	0x9310

#define RTL8380_MODEL_EXT_VERSION	0x00
#define RTL8380_MODEL_NAME		0x04
#define RTL8380_CHIP_INFO		0x08
#define RTL8380_MODEL_INFO		0x0c

#define RTL8390_MODEL_NAME		0x00
#define RTL8390_CHIP_INFO		0x04

#define RTL9300_MODEL_NAME		0x00
#define RTL9300_CHIP_INFO		0x04
#define RTL9310_MODEL_NAME		RTL9300_MODEL_NAME

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

static int realtek_chipinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct regmap *regmap;
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	int match_family;
	u32 model, chip_rev;
	bool is_engineering_sample;
	char model_letters[4];
	unsigned letter, max_letters;
	char revision;
	int err;

	regmap = device_node_to_regmap(dev->of_node);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

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

	switch (match_family) {
	case REALTEK_SOC_COMPATIBLE_8380:
		max_letters = 3;
		err = regmap_read(regmap, RTL8380_MODEL_NAME, &model);
		if (!err)
			err = regmap_read(regmap, RTL8380_MODEL_EXT_VERSION,
				&chip_rev);
		/*
		 * FIXME Correct detection of RTL8381M
		 * BIT(23) in register 0x1024 (RTL8380_MODE_DEFINE_CTL)
		 * indicates if the SoC is RTL8380M (1) or RTL8381M (0)
		 * The RTL8381M has only one QSGMII and two SGMII phys
		 */
		break;
	case REALTEK_SOC_COMPATIBLE_8390:
		max_letters = 2;
		err = regmap_read(regmap, RTL8390_MODEL_NAME, &model);
		chip_rev = (model >> 1) & 0x1f;
		is_engineering_sample = !!chip_rev;
		break;
	case REALTEK_SOC_COMPATIBLE_9300:
	case REALTEK_SOC_COMPATIBLE_9310:
		max_letters = 2;
		err = regmap_read(regmap, RTL9300_MODEL_NAME, &model);
		chip_rev = model & 0xf;
		is_engineering_sample = (model >> 4) & 0x1;
		break;
	default:
		dev_err(dev, "unknown SoC compatible string\n");
		return -EINVAL;
	}

	if (realtek_soc_family(model) != match_family) {
		dev_err(dev, "SoC family %04x does not match %04x\n",
			realtek_soc_family(model), match_family);
		return -EINVAL;
	}

	for (letter = 0; letter < max_letters; letter++)
		model_letters[letter] = realtek_5b_char(
			realtek_soc_5b_char(model, letter));
	model_letters[letter] = '\0';

	if (chip_rev)
		revision = realtek_5b_char(chip_rev);
	else
		revision = '0';

	of_property_read_string(NULL, "model", &soc_dev_attr->machine);
	soc_dev_attr->family = kasprintf(GFP_KERNEL, "RTL%04x",
		realtek_soc_family(model));
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%c%s",
		revision, is_engineering_sample ? "-ES" : "");
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "RTL%04x%s",
		realtek_soc_id(model), model_letters);

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
		.name = "realtek-chipid",
		.of_match_table = of_realtek_chipid_match
	},
	.probe = realtek_chipinfo_probe
};

static int __init realtek_chipid_init(void)
{
	return platform_driver_register(&realtek_chipid_driver);
}
subsys_initcall(realtek_chipid_init);

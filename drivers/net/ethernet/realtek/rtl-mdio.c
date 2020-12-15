// SPDX-License-Identifier: GPL-2.0-only

#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/mutex.h>
#include <linux/of_mdio.h>
#include <linux/of_device.h>

//#include "rtl83xx-eth.h"
// TODO: move to rtl83xx-eth.h

/* MII bus control */
#define RTL838X_SMI_ACCESS_PHY_CTRL_0				0xa1b8
#define RTL838X_SMI_ACCESS_PHY_CTRL_1				0xa1bc
#define   _RTL838X_SMIACCESSPHYCTRL1_REG_ADDR_4_0		20
#define   _RTL838X_SMIACCESSPHYCTRL1_PARK_PAGE_4_0		15
#define   _RTL838X_SMIACCESSPHYCTRL1_MAIN_PAGE_11_0		3
#define   _RTL838X_SMIACCESSPHYCTRL1_RWOP			BIT(2)
#define   _RTL838X_SMIACCESSPHYCTRL1_TYPE			BIT(1)
#define   _RTL838X_SMIACCESSPHYCTRL1_CMD			BIT(0)
#define RTL838X_SMI_ACCESS_PHY_CTRL_2				0xa1c0

#define RTL839X_PHYREG_ACCESS_CTRL				0x03dc
#define   _RTL839X_PHYREGACCESS_PARK_PAGE			23
#define   _RTL839X_PHYREGACCESS_MAIN_PAGE			10
#define   _RTL839X_PHYREGACCESS_REG				5
#define   _RTL839X_PHYREGACCESS_BROADCAST			BIT(4)
#define   _RTL839X_PHYREGACCESS_RWOP				BIT(3)
#define   _RTL839X_PHYREGACCESS_TYPE				BIT(2)
#define   _RTL839X_PHYREGACCESS_FAIL				BIT(1)
#define   _RTL839X_PHYREGACCESS_CMD				BIT(0)
#define RTL839X_PHYREG_CTRL					0x03e0
#define RTL839X_PHYREG_PORT_CTRL(port)				(0x03e4 + ((port >> 5) << 2))
#define RTL839X_PHYREG_DATA_CTRL				0x03f0


#define sw_r32(reg)		__raw_readl(reg)
#define sw_w32(val, reg)	__raw_writel(val, reg)
#define sw_w32_mask(clear, set, reg) sw_w32((sw_r32(reg) & ~(clear)) | (set), reg)


#define REG(x) (mdpriv->base + x)

DEFINE_MUTEX(smi_lock);

struct rtl_mdio_priv {
	void __iomem *base;
};


// TODO
/*
int old_rtl83xx_smi_wait_op(int timeout)
{
	do {
		timeout--;
		udelay(10);
	} while ((sw_r32(RTL838X_SMI_PHY_CTRL_1) & 0x1) && (timeout >= 0));
	if (timeout <= 0) {
		return -1;
	}
	return 0;
}
*/

int _rtl838x_smi_wait_op(struct rtl_mdio_priv *mdpriv, int timeout)
{
	while (timeout && (sw_r32(REG(RTL838X_SMI_ACCESS_PHY_CTRL_1)) & _RTL838X_SMIACCESSPHYCTRL1_CMD)) {
		timeout--;
		udelay(10);
	}

	return (!timeout);
}

static int rtl838x_mii_read(struct mii_bus *bus, int mii_id, int regnum)
{
	struct rtl_mdio_priv *mdpriv = bus->priv;
	u32 reg, park_page, v, page;

	mutex_lock(&smi_lock);

	if (_rtl838x_smi_wait_op(mdpriv, 10000))
		goto timeout;

	sw_w32(mii_id << 16, REG(RTL838X_SMI_ACCESS_PHY_CTRL_2));

	// TODO: fix up
	park_page = sw_r32(REG(RTL838X_SMI_ACCESS_PHY_CTRL_1)) & ((0x1f << 15) | 0x2);
	v = reg << 20 | page << 3;
	sw_w32(v | park_page, REG(RTL838X_SMI_ACCESS_PHY_CTRL_1));

	sw_w32_mask(0, 1, REG(RTL838X_SMI_ACCESS_PHY_CTRL_1));

	if (_rtl838x_smi_wait_op(mdpriv, 10000))
		goto timeout;

	reg = sw_r32(REG(RTL838X_SMI_ACCESS_PHY_CTRL_2)) & 0xffff;

	mutex_unlock(&smi_lock);

	pr_info("%s: id 0x%x reg 0x%x value 0x%.4x\n", __func__, mii_id,
		regnum, reg);
	return reg;

timeout:
	mutex_unlock(&smi_lock);
	return -EIO;
}

static int rtl838x_mii_write(struct mii_bus *bus, int mii_id, int regnum, u16 value)
{
	struct rtl_mdio_priv *mdpriv = bus->priv;
	u32 reg, park_page, v, page;

	pr_info("%s: id 0x%x reg 0x%x value %.4x\n", __func__, mii_id, regnum, value);
	mutex_lock(&smi_lock);

	if (_rtl838x_smi_wait_op(mdpriv, 10000))
		goto timeout;

	sw_w32(BIT(mii_id), REG(RTL838X_SMI_ACCESS_PHY_CTRL_0));
	mdelay(10);

	sw_w32_mask(0xffff0000, value << 16, REG(RTL838X_SMI_ACCESS_PHY_CTRL_2));

	park_page = sw_r32(REG(RTL838X_SMI_ACCESS_PHY_CTRL_1)) & ((0x1f << 15) | 0x2);
	v = reg << 20 | page << 3 | 0x4;
	sw_w32(v | park_page, REG(RTL838X_SMI_ACCESS_PHY_CTRL_1));
	sw_w32_mask(0, 1, REG(RTL838X_SMI_ACCESS_PHY_CTRL_1));

	if (_rtl838x_smi_wait_op(mdpriv, 10000))
		goto timeout;

	mutex_unlock(&smi_lock);

	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -EIO;
}

static int rtl839x_mii_read(struct mii_bus *bus, int mii_id, int regnum)
{
	struct rtl_mdio_priv *mdpriv = bus->priv;
	u32 reg;

	mutex_lock(&smi_lock);

	sw_w32(mii_id << 16, REG(RTL839X_PHYREG_DATA_CTRL));

	/* "Ext page" */
	sw_w32(0x1ff, REG(RTL839X_PHYREG_CTRL));

	sw_w32(regnum << 5 | BIT(_RTL839X_PHYREGACCESS_CMD), REG(RTL839X_PHYREG_ACCESS_CTRL));

	while (sw_r32(REG(RTL839X_PHYREG_ACCESS_CTRL)) & 0x1)
		cpu_relax();

	reg = sw_r32(REG(RTL839X_PHYREG_DATA_CTRL)) & 0xffff;

	mutex_unlock(&smi_lock);

	pr_info("%s: id 0x%x reg 0x%x value 0x%.4x\n", __func__, mii_id,
		regnum, reg);
	return reg;
}

static int rtl839x_mii_write(struct mii_bus *bus, int mii_id, int regnum, u16 value)
{
	struct rtl_mdio_priv *mdpriv = bus->priv;
	u32 reg;

	pr_info("%s: id 0x%x reg 0x%x value %.4x\n", __func__, mii_id, regnum, value);
	mutex_lock(&smi_lock);

	sw_w32(0, REG(RTL839X_PHYREG_PORT_CTRL(0)));
	sw_w32(0, REG(RTL839X_PHYREG_PORT_CTRL(0)) + 4);
	sw_w32(1 << (mii_id % 32), REG(RTL839X_PHYREG_PORT_CTRL(mii_id)));

	sw_w32(value << 16, REG(RTL839X_PHYREG_DATA_CTRL));

	/* "Ext page" */
	sw_w32(0x1ff, REG(RTL839X_PHYREG_CTRL));

	reg = regnum << 5 | BIT(_RTL839X_PHYREGACCESS_RWOP) | BIT(_RTL839X_PHYREGACCESS_CMD);
	sw_w32(reg, REG(RTL839X_PHYREG_ACCESS_CTRL));

	while (sw_r32(REG(RTL839X_PHYREG_ACCESS_CTRL)) & 0x1)
		cpu_relax();

	mutex_unlock(&smi_lock);

	return 0;
}

int realtek_rtl_mii_probe(struct platform_device *pdev)
{
	struct device_node *mii_np;
	struct mii_bus *bus;
	struct rtl_mdio_priv *mdpriv;
	u32 soc_id;

	pr_info("%s\n", __func__);
	mii_np = pdev->dev.of_node;

	bus = devm_mdiobus_alloc_size(&pdev->dev, sizeof(*mdpriv));
	if (!bus) {
		of_node_put(mii_np);
		return -ENOMEM;
	}
	mdpriv = bus->priv;

	mdpriv->base = (void *)0xbb000000;
	// mdpriv->base = devm_platform_ioremap_resource(pdev, 0);
	// if (IS_ERR(mdpriv->base))
	// 	return PTR_ERR(mdpriv->base);
	pr_info("%s base 0x%x", __func__, (u32)mdpriv->base);

	soc_id = ((unsigned int)of_device_get_match_data(&pdev->dev) * 0xfff0);

	bus->parent = &pdev->dev;
	bus->name = "Realtek RTL MII bus";
	snprintf(bus->id, MII_BUS_ID_SIZE, "%pOFn", mii_np);
	if (soc_id == 0x8380) {
		bus->read = rtl838x_mii_read;
		bus->write = rtl838x_mii_write;
	} else if (soc_id == 0x8390) {
		bus->read = rtl839x_mii_read;
		bus->write = rtl839x_mii_write;
	}
	return devm_of_mdiobus_register(&pdev->dev, bus, mii_np);
}


static const struct of_device_id realtek_rtl_mdio_of_ids[] = {
	{ .compatible = "realtek,rtl8382-mdio", .data = (void *)0x8382 },
	{ .compatible = "realtek,rtl8392-mdio", .data = (void *)0x8392 },
};
MODULE_DEVICE_TABLE(of, rtl83xx_eth_of_ids);

static struct platform_driver realtek_rtl_mdio_driver = {
	.driver = {
		.name = "realtek-rtl-mdio",
		.of_match_table = realtek_rtl_mdio_of_ids,
	},
	.probe = realtek_rtl_mii_probe,
};

module_platform_driver(realtek_rtl_mdio_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Bert Vermeulen <bert@biot.com>");

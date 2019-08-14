// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (C) 2018 Flowbird
 *  Martin Fuzzey  <martin.fuzzey@flowbird.group>
 */

#include <common.h>
#include <fdtdec.h>
#include <errno.h>
#include <dm.h>
#include <i2c.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/da9063_pmic.h>

static const struct pmic_child_info pmic_children_info[] = {
	{ .prefix = "ldo", .driver = DA9063_LDO_DRIVER },
	{ .prefix = "b", .driver = DA9063_BUCK_DRIVER },
	{ },
};

struct da9063_priv {
	u8	cur_page;

	/* We only support I2C for the moment so these are constant but,
	 * storing them here will simplify a future SPI implementation
	 */
	u8	page_mask;
	u8	offset_mask;
};

/*
 * The register map is non contiguous and attempts to read in the holes fail.
 * But "pmic dump" tries to dump the full register map.
 * So define the holes here so we can fix that.
 */
struct da9063_reg_hole {
	u16	first;
	u16	last;
};

static const struct da9063_reg_hole da9063_reg_holes[] = {
	DA9063_REG_HOLE_1,
	DA9063_REG_HOLE_2,
	DA9063_REG_HOLE_3,
	/* These aren't readable. I can't see why from the datasheet but
	 * attempts to read fail and the kernel marks them unreadable too,
	 */
	{DA9063_REG_OTP_COUNT, DA9063_REG_OTP_DATA},
};

static int da9063_reg_count(struct udevice *dev)
{
	return DA9063_NUM_OF_REGS;
}

static bool da9063_reg_valid(uint reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(da9063_reg_holes); i++) {
		const struct da9063_reg_hole *hole = &da9063_reg_holes[i];

		if (reg >= hole->first && reg <= hole->last)
			return false;
	}

	return true;
}

/* Select page for register
 * return offset or negative error code
 */
static int da9063_select_page(struct udevice *dev, uint reg)
{
	struct da9063_priv *priv = dev->priv;
	u8 page = (reg / 128) & priv->page_mask;
	uint offset = reg & priv->offset_mask;
	int ret;

	if (page == priv->cur_page)
		return offset;

	if (offset == DA9063_REG_PAGE_CON) /* Available on all pages */
		return offset;

	ret = dm_i2c_write(dev, DA9063_REG_PAGE_CON, &page, 1);
	if (ret < 0) {
		pr_err("error on device %p selecting page %u for reg %#x",
		       dev, page, reg);
		return ret;
	}

	priv->cur_page = page;

	return offset;
}

static int da9063_write(struct udevice *dev, uint reg, const uint8_t *buff,
			int len)
{
	int offset;

	offset = da9063_select_page(dev, reg);
	if (offset < 0)
		return offset;

	if (dm_i2c_write(dev, offset, buff, len)) {
		pr_err("write error to device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int da9063_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	int offset;

	if (!da9063_reg_valid(reg))
		return -ENODATA;

	offset = da9063_select_page(dev, reg);
	if (offset < 0)
		return offset;

	if (dm_i2c_read(dev, offset, buff, len)) {
		pr_err("read error from device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int da9063_bind(struct udevice *dev)
{
	ofnode regulators_node;
	int children;

	regulators_node = dev_read_subnode(dev, "regulators");
	if (!ofnode_valid(regulators_node)) {
		debug("%s: %s regulators subnode not found!", __func__,
		      dev->name);
		return -ENXIO;
	}

	debug("%s: '%s' - found regulators subnode\n", __func__, dev->name);

	children = pmic_bind_children(dev, regulators_node, pmic_children_info);
	if (!children)
		debug("%s: %s - no child found\n", __func__, dev->name);

	/* Always return success for this device */
	return 0;
}

static int da9063_probe(struct udevice *dev)
{
	struct da9063_priv *priv = dev->priv;

	/* For I2C we can access pages (0,1) and (2,3) without switching page */
	priv->page_mask = 0x2;
	priv->offset_mask = 0xff;

	return 0;
}

static struct dm_pmic_ops da9063_ops = {
	.reg_count = da9063_reg_count,
	.read = da9063_read,
	.write = da9063_write,
};

static const struct udevice_id da9063_ids[] = {
	{ .compatible = "dlg,da9063" },
	{ }
};

U_BOOT_DRIVER(pmic_da9063) = {
	.name = "da9063_pmic",
	.id = UCLASS_PMIC,
	.of_match = da9063_ids,
	.bind = da9063_bind,
	.probe = da9063_probe,
	.ops = &da9063_ops,
	.priv_auto_alloc_size = sizeof(struct da9063_priv),
};

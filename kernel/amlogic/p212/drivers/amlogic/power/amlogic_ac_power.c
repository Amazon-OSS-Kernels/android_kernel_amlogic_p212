/*
 * amlogic_ac_power.c
 *
 * Device power information
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Ruguang Chen (ruguangc@lab126.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int ac_get_property(struct power_supply *psy,
			   enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* ac_main initialization */
static struct power_supply ac_main = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = ac_props,
	.num_properties = ARRAY_SIZE(ac_props),
	.get_property = ac_get_property,
};

static int power_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("%s name = %s, id=%d\n", __func__, pdev->name, pdev->id);

	/* Integrate with Android Battery Service */
	ret = power_supply_register(dev, &ac_main);
	if (ret) {
		pr_err("%s: power_supply_register AC Fail !\n", __func__);
	}

	return ret;
}

static int power_remove(struct platform_device *dev)
{
	pr_info("%s: unregister drivers\n", __func__);
	power_supply_unregister(&ac_main);

	return 0;
}

static void power_shutdown(struct platform_device *dev)
{
	pr_info("%s: do nothing\n", __func__);
}

static struct platform_driver power_driver = {
	.probe = power_probe,
	.remove = power_remove,
	.shutdown = power_shutdown,
	.driver = {
		   .name = "aml_ac_power",
		},
};

static struct platform_device power_device = {
	.name = "aml_ac_power",
	.id = 0,
	.dev = {
		},
};

static int __init power_init(void)
{
	int ret;

	pr_info("%s: entering:\n", __func__);
	ret = platform_driver_register(&power_driver);
	if (ret) {
		pr_err("%s: Unable to register driver (%d)\n", __func__, ret);
		return ret;
	}

	ret = platform_device_register(&power_device);
	if (ret) {
		pr_err("%s: Unable to register device (%d)\n", __func__, ret);
		return ret;
	}

	pr_info("%s: DONE\n", __func__);
	return 0;
}

static void __exit power_exit(void)
{
	platform_driver_unregister(&power_driver);
}

late_initcall(power_init);
module_exit(power_exit);

MODULE_AUTHOR("Ruguang Chen");
MODULE_DESCRIPTION("AC Power Device Driver");
MODULE_LICENSE("GPL");
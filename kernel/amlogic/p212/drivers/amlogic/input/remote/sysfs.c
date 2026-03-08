/*
 * drivers/amlogic/input/remote/sysfs.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>

#include "remote_meson.h"
#include "remote_core.h"
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/dma-mapping.h>


static ssize_t protocol_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	return sprintf(buf, "protocol=%s (0x%x)\n",
		chip->proto_name, chip->protocol);
}

static ssize_t protocol_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int protocol;
	struct remote_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &protocol);
	if (ret != 0) {
		pr_err("input parameter error\n");
		return -EINVAL;
	}
	chip->protocol = protocol;
	chip->set_register_config(chip, chip->protocol);
	if ((chip->r_dev->rc_type >> 8) && !(chip->protocol >> 8)) {
		remote_raw_event_unregister(chip->r_dev); /*raw->no raw*/
		pr_info("remote_raw_event_unregister\n");
	} else if (!(chip->r_dev->rc_type >> 8) && (chip->protocol >> 8)) {
		remote_raw_init();
		remote_raw_event_register(chip->r_dev); /*no raw->raw*/
	}
	chip->r_dev->rc_type = chip->protocol;
	return count;
}


static ssize_t keymap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	int i, len;
	struct custom_table *ct = chip->cur_custom;
	if (!ct) {
		pr_err("please set keymap name first\n");
		return 0;
	}
	len = sprintf(buf, "custom_code=0x%x\n", ct->custom_code);
	len += sprintf(buf+len, "custom_name=%s\n", ct->custom_name);
	len += sprintf(buf+len, "map_size=%d\n" , ct->map_size);
	len += sprintf(buf+len, "keycode scancode\n");
	for (i = 0; i < ct->map_size; i++) {
		len += sprintf(buf+len, "%4d %4d\n", ct->codemap[i].map.keycode,
			ct->codemap[i].map.scancode);
	}
	return len;
}

static ssize_t keymap_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	int cnt;
	int ret;
	int value;
	struct custom_table *ct = chip->custom_tables;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0) {
		pr_err("keymap_store input err\n");
		return -EINVAL;
	}
	for (cnt = 0; cnt < chip->custom_num; cnt++, ct++) {
		if (ct->custom_code == value) {
			chip->cur_custom = ct;
			break;
		}
	}
	return count;
}

static ssize_t debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int debug_enable;

	debug_enable = remote_debug_get_enable();

	return sprintf(buf, "%d\n", debug_enable);
}

static ssize_t debug_enable_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int debug_enable;
	int ret;
	ret = kstrtoint(buf, 0, &debug_enable);
	if (ret != 0)
		return -EINVAL;
	remote_debug_set_enable(debug_enable);
	return count;
}

static ssize_t debug_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int debug_level;

	debug_level = remote_debug_get_level();
	return sprintf(buf, "%d\n", debug_level);
}

static ssize_t debug_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int debug_level;
	int ret;

	ret = kstrtoint(buf, 0, &debug_level);
	if (ret != 0)
		return -EINVAL;
	remote_debug_set_level(debug_level);
	return count;
}

int debug_log_printk(struct remote_dev *dev, const char *fmt)
{
	char *p;
	int len;

	len = strlen(fmt);
	if (dev->debug_current + len > dev->debug_buffer_size)
		dev->debug_current = 0;
	p = (char *)(dev->debug_buffer+dev->debug_current);
	strcpy(p, fmt);
	dev->debug_current += len;
	return 0;
}

static ssize_t debug_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->debug_buffer)
		return 0;
	return sprintf(buf, r_dev->debug_buffer);
}

static ssize_t debug_log_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->debug_buffer)
		return 0;
	if (buf[0] == 'c') {
		r_dev->debug_buffer[0] = 0;
		r_dev->debug_current = 0;
	}
	return count;
}

static ssize_t release_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	return sprintf(buf, "%lu\n", chip->r_dev->keyup_delay);
}

static ssize_t release_delay_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	int val;
	struct remote_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	chip->r_dev->keyup_delay = val;
	return count;
}

static ssize_t custom_maps_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct custom_table *ct = chip->custom_tables;
	int len = 0;
	int cnt = 0;

	len += sprintf(buf, "custom_num = %d\n", chip->custom_num);
	for (cnt = 0; cnt < chip->custom_num; cnt++, ct++) {
		len += sprintf(buf+len, "%d. 0x%x,%s\n",
			cnt, ct->custom_code, ct->custom_name);
	}
	return len;
}

static ssize_t start_cycle_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	int time_out;

	aml_write_cbus(0x2d, 0x200);
	aml_write_cbus(0x2e,0);

	ret = kstrtoint(buf, 0, &time_out);
	if (ret != 0)
		return -EINVAL;
	scpi_set_ir_data(0, time_out);
	return count;
}

static ssize_t stop_cycle_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);

	if (buf[0] == 's') {
			scpi_set_ir_data(1, 0);
	}
	return count;
}

static u64 remote_dma_mask = DMA_BIT_MASK(32);
static ssize_t get_cycle_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int len = 0, data = 0, cl_size = 0, i;
	unsigned char *cl_data;
	struct remote_chip *chip = dev_get_drvdata(dev);

	chip->cl_mem = kzalloc(CL_MEM_SIZE, GFP_KERNEL);
	if (!chip->cl_mem) {
		len += sprintf(buf, "kzalloc error\n");
		return len;
	}
	dev->dma_mask = &remote_dma_mask;
	chip->cl_mem_phy = (void*)dma_map_single(dev,
		chip->cl_mem, CL_MEM_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, (dma_addr_t)chip->cl_mem_phy)) {
		len += sprintf(buf, "dma_map error\n");
		goto err_map;
	}

	/*if fail return -EINVAL*/
	data = scpi_get_ir_data(1, chip->cl_mem_phy, CL_MEM_SIZE, &cl_size);
	if (cl_size > CL_MEM_SIZE)
		cl_size = CL_MEM_SIZE;
	dma_unmap_single( dev, (dma_addr_t)chip->cl_mem_phy,
		CL_MEM_SIZE, DMA_FROM_DEVICE);

	if (data == -EINVAL) {
		len += sprintf(buf, "%d\n", data);
		goto err_scpi;
	}

	cl_data = (unsigned char *)chip->cl_mem;
	for (i = 0 ; i < cl_size; i++) {
		len += sprintf(buf + len, "%3x", *cl_data);
		cl_data++;
		if (!((i + 1) & 0xf))
			len += sprintf(buf + len, "\n");
	}
	len += sprintf(buf + len, "\n");
	kfree(chip->cl_mem);
	return len;
err_scpi:
err_map:
	kfree(chip->cl_mem);
	return len;
}


DEVICE_ATTR_RW(release_delay);
DEVICE_ATTR_RW(protocol);
DEVICE_ATTR_RW(keymap);
DEVICE_ATTR_RW(debug_enable);
DEVICE_ATTR_RW(debug_level);
DEVICE_ATTR_RW(debug_log);
DEVICE_ATTR_RO(custom_maps);
DEVICE_ATTR_WO(start_cycle);
DEVICE_ATTR_WO(stop_cycle);
DEVICE_ATTR_RO(get_cycle);

/*
static DEVICE_ATTR(custom_maps, S_IRUGO,
	custom_maps_show, NULL);

static DEVICE_ATTR(release_delay, (S_IRUGO | S_IWUGO),
	release_delay_show, release_delay_store);

static DEVICE_ATTR(protocol, (S_IRUGO | S_IWUGO),
	protocol_show, protocol_store);

static DEVICE_ATTR(keymap, (S_IRUGO | S_IWUGO),
	keymap_show, keymap_store);

static DEVICE_ATTR(debug_enable, (S_IRUGO | S_IWUGO),
	debug_enable_show, debug_enable_store);

static DEVICE_ATTR(debug_level, (S_IRUGO | S_IWUGO),
	debug_level_show, debug_level_store);

static DEVICE_ATTR(debug_log, (S_IRUGO | S_IWUGO),
	debug_log_show, debug_log_store);
*/

static struct attribute *remote_attrs[] = {
	&dev_attr_protocol.attr,
	&dev_attr_custom_maps.attr,
	&dev_attr_keymap.attr,
	&dev_attr_release_delay.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_debug_level.attr,
	&dev_attr_debug_log.attr,
	&dev_attr_start_cycle.attr,
	&dev_attr_stop_cycle.attr,
	&dev_attr_get_cycle.attr,
	NULL,
};

ATTRIBUTE_GROUPS(remote);

static struct class remote_class = {
	.name		= "remote",
	.owner		= THIS_MODULE,
	.dev_groups = remote_groups,
};

int ir_sys_device_attribute_init(struct remote_chip *chip)
{
	struct device *dev;

	class_register(&remote_class);

	dev = device_create(&remote_class,  NULL,
					chip->chr_devno, chip, chip->dev_name);
	if (IS_ERR_OR_NULL(dev))
		return -1;
	return 0;
}
EXPORT_SYMBOL_GPL(ir_sys_device_attribute_init);

void ir_sys_device_attribute_sys(struct remote_chip *chip)
{
	device_destroy(&remote_class, chip->chr_devno);
}
EXPORT_SYMBOL_GPL(ir_sys_device_attribute_sys);




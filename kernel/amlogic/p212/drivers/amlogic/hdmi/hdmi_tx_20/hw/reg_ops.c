/*
 * drivers/amlogic/hdmi/hdmi_tx_20/hw/reg_ops.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include "mach_reg.h"
#include "hdmi_tx_reg.h"

static int dbg_en;

/*
 * RePacket HDMI related registers rd/wr
 */
struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

static struct reg_map reg_maps[] = {
	{ /* CBUS */
		.phy_addr = 0xc0800000,
		.size = 0xa00000,
	},
	{ /* RESET */
		.phy_addr = 0xc1104400,
		.size = 0x100,
	},
	{ /* RTI */
		.phy_addr = 0xc8100000,
		.size = 0x100000,
	},
	{ /* PERIPHS */
		.phy_addr = 0xc8834000,
		.size = 0x2000,
	},
	{ /* HDMITX NON-SECURE*/
		.phy_addr = 0xc883a000,
		.size = 0x2000,
	},
	{ /* HIU */
		.phy_addr = 0xc883c000,
		.size = 0x2000,
	},
	{ /* VPU */
		.phy_addr = 0xd0100000,
		.size = 0x40000,
	},
	{ /* HDMITX SECURE */
		.phy_addr = 0xda83a000,
		.size = 0x2000,
	},
};

static int in_reg_maps_idx(unsigned int addr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_maps); i++) {
		if ((addr >= reg_maps[i].phy_addr) &&
			(addr < (reg_maps[i].phy_addr + reg_maps[i].size))) {
			return i;
		}
	}

	return -1;
}

static int check_map_flag(unsigned int addr)
{
	int idx;

	idx = in_reg_maps_idx(addr);
	if ((idx != -1) && (reg_maps[idx].flag))
		return 1;
	else {
		pr_info("hdmitx20: not Mapped PHY 0x%x\n", addr);
		return 0;
	}
}

void init_reg_map(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_maps); i++) {
		reg_maps[i].p = ioremap(reg_maps[i].phy_addr, reg_maps[i].size);
		if (!reg_maps[i].p) {
			pr_info("hdmitx20: failed Mapped PHY: 0x%x\n",
				reg_maps[i].phy_addr);
		} else
			reg_maps[i].flag = 1;
	}
}

unsigned int hd_read_reg(unsigned int addr)
{
	int ret = 0;
	int idx = in_reg_maps_idx(addr);
	unsigned int val = 0;
	unsigned int type = (addr >> OFFSET);
	unsigned int reg = addr & ((1 << OFFSET) - 1);

	if ((idx != -1) && check_map_flag(addr)) {
		val = readl(reg_maps[idx].p + (addr - reg_maps[idx].phy_addr));
		goto end;
	}

	ret = aml_reg_read(type, reg, &val);

	if (ret < 0) {
		pr_info("Rd[0x%x] Error\n", addr);
		return val;
	}
end:
	if (dbg_en)
		pr_info("Rd[0x%x] 0x%x\n", addr, val);
	return val;
}

void hd_write_reg(unsigned int addr, unsigned int val)
{
	int ret = 0;
	int idx = in_reg_maps_idx(addr);
	unsigned int type = (addr >> OFFSET);
	unsigned int reg = addr & ((1 << OFFSET) - 1);

	if ((idx != -1) && check_map_flag(addr)) {
		writel(val, reg_maps[idx].p + (addr - reg_maps[idx].phy_addr));
		goto end;
	}

	ret = aml_reg_write(type, reg, val);

	if (ret < 0) {
		pr_info("Wr[0x%x] 0x%x Error\n", addr, val);
		return;
	}

end:
	if (dbg_en)
		pr_info("Wr[0x%x] 0x%x\n", addr, val);
}

void hd_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hd_read_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hd_write_reg(addr, data32);
}

#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

unsigned int hdmitx_rd_reg(unsigned int addr)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	unsigned int data;
	register long x0 asm("x0") = 0x82000018;
	register long x1 asm("x1") = (unsigned long)addr;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		"smc #0\n"
		: "+r"(x0) : "r"(x1)
	);
	data = (unsigned)(x0&0xffffffff);

	if (dbg_en)
		pr_info("%s rd[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
			addr, data);
	return data;
}

void hdmitx_wr_reg(unsigned int addr, unsigned int data)
{
	unsigned long offset = (addr & DWC_OFFSET_MASK) >> 24;
	register long x0 asm("x0") = 0x82000019;
	register long x1 asm("x1") = (unsigned long)addr;
	register long x2 asm("x2") = data;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		__asmeq("%2", "x2")
		"smc #0\n"
		: : "r"(x0), "r"(x1), "r"(x2)
	);

	if (dbg_en)
		pr_info("%s wr[0x%x] 0x%x\n", offset ? "DWC" : "TOP",
			addr, data);
}

void hdmitx_set_reg_bits(unsigned int addr, unsigned int value,
	unsigned int offset, unsigned int len)
{
	unsigned int data32 = 0;

	data32 = hdmitx_rd_reg(addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	hdmitx_wr_reg(addr, data32);
}

void hdmitx_poll_reg(unsigned int addr, unsigned int val, unsigned long timeout)
{
	unsigned long time = 0;

	time = jiffies;
	while ((!(hdmitx_rd_reg(addr) & val)) &&
		time_before(jiffies, time + timeout)) {
		mdelay(2);
	}
	if (time_after(jiffies, time + timeout))
		pr_info("hdmitx poll:0x%x  val:0x%x T1=%lu t=%lu T2=%lu timeout\n",
			addr, val, time, timeout, jiffies);
}

void hdmitx_rd_check_reg(unsigned int addr, unsigned int exp_data,
	unsigned int mask)
{
	unsigned long rd_data;
	rd_data = hdmitx_rd_reg(addr);
	if ((rd_data | mask) != (exp_data | mask)) {
		pr_info("HDMITX-DWC addr=0x%04x rd_data=0x%02x\n",
			(unsigned int)addr, (unsigned int)rd_data);
		pr_info("Error: HDMITX-DWC exp_data=0x%02x mask=0x%02x\n",
			(unsigned int)exp_data, (unsigned int)mask);
	}
}

MODULE_PARM_DESC(dbg_en, "\n debug_level\n");
module_param(dbg_en, int, 0664);

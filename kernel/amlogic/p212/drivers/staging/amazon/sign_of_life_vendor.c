/*
 * sign_of_life_vendor.c
 *
 * vendor platform implementation
 * Copyright 2017 Amazon.com, Inc. or its Affiliates. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/sign_of_life.h>


#define STICKY_REG_BASE	0xc8834400
#define STICKY_REG_OFFSET (0x70 <<2)
#define BOOT_REASON_REG_BASE 0xc8100000
#define BOOT_REASON_REG_BOFFSET 0x23c

#define VENDOR_WARM_BOOT_KERNEL_PANIC	(1U << 1)
#define VENDOR_WARM_BOOT_KERNEL_WDOG	(1U << 2)
#define VENDOR_WARM_BOOT_HW_WDOG	(1U << 3)
#define VENDOR_WARM_BOOT_SW		(1U << 4)
#define VENDOR_COLD_BOOT_USB		(1U << 5)
#define VENDOR_COLD_BOOT_POWER_KEY	(1U << 6)
#define VENDOR_COLD_BOOT_POWER_SUPPLY	(1U << 7)

#define VENDOR_SHUTDOWN_LONG_PWR_KEY_PRESS	(1U << 8)
#define VENDOR_SHUTDOWN_SW			(1U << 9)
#define VENDOR_SHUTDOWN_PWR_KEY			(1U << 10)
#define VENDOR_SHUTDOWN_SUDDEN_PWR_LOSS		(1U << 11)
#define VENDOR_SHUTDOWN_UKNOWN			(1U << 12)

#define VENDOR_THERMAL_SHUTDOWN_BATTERY		(1U << 16)
#define VENDOR_THERMAL_SHUTDOWN_PMIC		(1U << 17)
#define VENDOR_THERMAL_SHUTDOWN_SOC		(1U << 18)
#define VENDOR_THERMAL_SHUTDOWN_MODEM		(1U << 19)
#define VENDOR_THERMAL_SHUTDOWN_WIFI		(1U << 20)
#define VENDOR_THERMAL_SHUTDOWN_PCB		(1U << 21)

#define VENDOR_SPECIAL_MODE_LOW_BATTERY			(1U << 24)
#define VENDOR_SPECIAL_MODE_WARM_BOOT_USB_CONNECTED	(1U << 25)
#define VENDOR_SPECIAL_MODE_OTA				(1U << 26)
#define VENDOR_SPECIAL_MODE_FACTORY_RESET		(1U << 27)

#define VENDOR_BOOT_REASON_MASK		0x000000ff
#define VENDOR_SHUTDOWN_MASK		0x0000ff00
#define VENDOR_THERMAL_SHUTDOWN_MASK	0x00ff0000
#define VENDOR_SPECIAL_MODE_MASK	0xff000000


#define VENDOR_BOOTMODE_COLD_BOOT			0
#define VENDOR_BOOTMODE_NORMAL_BOOT			1
#define VENDOR_BOOTMODE_FACTORY_RESET_REBOOT		2
#define VENDOR_BOOTMODE_UPDATE_REBOOT			3
#define VENDOR_BOOTMODE_FASTBOOT_REBOOT			4
#define VENDOR_BOOTMODE_SUSPEND_REBOOT			5
#define VENDOR_BOOTMODE_HIBERNATE_REBOOT		6
#define VENDOR_BOOTMODE_BOOTLOADER_REBOOT		7
#define VENDOR_BOOTMODE_CRASH_REBOOT			11
#define VENDOR_BOOTMODE_KERNEL_PANIC			12
#define VENDOR_BOOTMODE_WATCHDOG_REBOOT			13


static void __iomem *vendor_reg = NULL;

static u32 vendor_reg_read(void)
{
	if (vendor_reg != NULL)
		return (readl(vendor_reg));
	return 0;
}

static void vendor_reg_write(u32 data)
{
	if (vendor_reg != NULL)
		writel(data, vendor_reg);
}


static int (vendor_read_boot_reason)(life_cycle_reason_t *boot_reason)
{
	u32 vendor_breason;

	vendor_breason = vendor_reg_read();

	printk(KERN_INFO"%s: boot_reason is 0x%x\n", __func__, (vendor_breason & VENDOR_BOOT_REASON_MASK));

	if (vendor_breason & VENDOR_WARM_BOOT_KERNEL_PANIC)
		*boot_reason = WARMBOOT_BY_KERNEL_PANIC;
	else if (vendor_breason & VENDOR_WARM_BOOT_KERNEL_WDOG)
		*boot_reason = WARMBOOT_BY_KERNEL_WATCHDOG;
	else if (vendor_breason & VENDOR_WARM_BOOT_HW_WDOG)
		*boot_reason = WARMBOOT_BY_HW_WATCHDOG;
	else if (vendor_breason & VENDOR_WARM_BOOT_SW)
		*boot_reason = WARMBOOT_BY_SW;
	else if (vendor_breason & VENDOR_COLD_BOOT_USB)
		*boot_reason = COLDBOOT_BY_USB;
	else if (vendor_breason & VENDOR_COLD_BOOT_POWER_KEY)
		*boot_reason = COLDBOOT_BY_POWER_KEY;
	else if (vendor_breason & VENDOR_COLD_BOOT_POWER_SUPPLY)
		*boot_reason = COLDBOOT_BY_POWER_SUPPLY;
	else {
		printk(KERN_ERR"Failed to read boot vendor boot reason\n");
		return -1;
	}

	return 0;
}

static int (vendor_write_boot_reason)(life_cycle_reason_t boot_reason)
{
	u32 vendor_breason;

	vendor_breason = vendor_reg_read();

	printk(KERN_INFO"%s: current 0x%x boot_reason 0x%x\n", __func__, vendor_breason, boot_reason);
	if (boot_reason == WARMBOOT_BY_KERNEL_PANIC)
		vendor_breason = vendor_breason | VENDOR_WARM_BOOT_KERNEL_PANIC;
	else if (boot_reason == WARMBOOT_BY_KERNEL_WATCHDOG)
		vendor_breason = vendor_breason | VENDOR_WARM_BOOT_KERNEL_WDOG;
	else if (boot_reason == WARMBOOT_BY_HW_WATCHDOG)
		vendor_breason = vendor_breason | VENDOR_WARM_BOOT_HW_WDOG;
	else if (boot_reason == WARMBOOT_BY_SW)
		vendor_breason = vendor_breason | VENDOR_WARM_BOOT_SW;
	else if (boot_reason == COLDBOOT_BY_USB)
		vendor_breason = vendor_breason | VENDOR_COLD_BOOT_USB;
	else if (boot_reason == COLDBOOT_BY_POWER_KEY)
		vendor_breason = vendor_breason | VENDOR_COLD_BOOT_POWER_KEY;
	else if (boot_reason == COLDBOOT_BY_POWER_SUPPLY)
		vendor_breason = vendor_breason | VENDOR_COLD_BOOT_POWER_SUPPLY;

	vendor_reg_write(vendor_breason);

	return 0;
}

static int (vendor_read_shutdown_reason)(life_cycle_reason_t *shutdown_reason)
{
	u32 vendor_shutdown_reason;

	vendor_shutdown_reason = vendor_reg_read();

	printk(KERN_INFO"%s: shutdown reason is 0x%x\n", __func__, (vendor_shutdown_reason & VENDOR_SHUTDOWN_MASK));
	if (vendor_shutdown_reason & VENDOR_SHUTDOWN_LONG_PWR_KEY_PRESS)
		*shutdown_reason = SHUTDOWN_BY_LONG_PWR_KEY_PRESS;
	else if (vendor_shutdown_reason & VENDOR_SHUTDOWN_SW)
		*shutdown_reason = SHUTDOWN_BY_SW;
	else if (vendor_shutdown_reason & VENDOR_SHUTDOWN_PWR_KEY)
		*shutdown_reason = SHUTDOWN_BY_PWR_KEY;
	else if (vendor_shutdown_reason & VENDOR_SHUTDOWN_SUDDEN_PWR_LOSS)
		*shutdown_reason = SHUTDOWN_BY_SUDDEN_POWER_LOSS;
	else if (vendor_shutdown_reason & VENDOR_SHUTDOWN_UKNOWN)
		*shutdown_reason = SHUTDOWN_BY_UNKNOWN_REASONS;
	else {
		printk(KERN_ERR"Failed to read boot vendor boot reason\n");
		return -1;
	}

	return 0;
}

static int (vendor_write_shutdown_reason)(life_cycle_reason_t shutdown_reason)
{
	u32 vendor_shutdown_reason;

	vendor_shutdown_reason = vendor_reg_read();

	printk(KERN_INFO"%s: shutdown_reason 0x%x\n", __func__, vendor_shutdown_reason);

	if (shutdown_reason == SHUTDOWN_BY_LONG_PWR_KEY_PRESS)
		vendor_shutdown_reason = vendor_shutdown_reason | VENDOR_SHUTDOWN_LONG_PWR_KEY_PRESS;
	else if (shutdown_reason == SHUTDOWN_BY_SW)
		vendor_shutdown_reason = vendor_shutdown_reason | VENDOR_SHUTDOWN_SW;
	else if (shutdown_reason == SHUTDOWN_BY_PWR_KEY)
		vendor_shutdown_reason = vendor_shutdown_reason | VENDOR_SHUTDOWN_PWR_KEY;
	else if (shutdown_reason == SHUTDOWN_BY_SUDDEN_POWER_LOSS)
		vendor_shutdown_reason = vendor_shutdown_reason | VENDOR_SHUTDOWN_SUDDEN_PWR_LOSS;
	else if (shutdown_reason == SHUTDOWN_BY_UNKNOWN_REASONS)
		vendor_shutdown_reason = vendor_shutdown_reason | VENDOR_SHUTDOWN_UKNOWN;

	vendor_reg_write(vendor_shutdown_reason);

	return 0;
}

static int (vendor_read_thermal_shutdown_reason)(life_cycle_reason_t *thermal_shutdown_reason)
{
	u32 vendor_thermal_shutdown_reason;

	vendor_thermal_shutdown_reason = vendor_reg_read();

	printk(KERN_INFO"%s: thermal shutdown reason 0x%x\n", __func__, (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_MASK));
	if (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_BATTERY)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_BATTERY;
	else if (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_PMIC)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_PMIC;
	else if (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_SOC)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_SOC;
	else if (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_MODEM)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_MODEM;
	else if (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_WIFI)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_WIFI;
	else if (vendor_thermal_shutdown_reason & VENDOR_THERMAL_SHUTDOWN_PCB)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_PCB;
	else {
		printk(KERN_ERR"Failed to read boot vendor boot reason\n");
		return -1;
	}

	return 0;
}

static int (vendor_write_thermal_shutdown_reason)(life_cycle_reason_t thermal_shutdown_reason)
{
	u32 vendor_thermal_shutdown_reason;

	vendor_thermal_shutdown_reason = vendor_reg_read();

	printk(KERN_INFO "%s: shutdown_reason 0x%0x\n", __func__, vendor_thermal_shutdown_reason);


	if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_BATTERY)
		vendor_thermal_shutdown_reason = vendor_thermal_shutdown_reason | VENDOR_THERMAL_SHUTDOWN_BATTERY;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_PMIC)
		vendor_thermal_shutdown_reason = vendor_thermal_shutdown_reason | VENDOR_THERMAL_SHUTDOWN_PMIC;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_SOC)
		vendor_thermal_shutdown_reason = vendor_thermal_shutdown_reason | VENDOR_THERMAL_SHUTDOWN_SOC;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_MODEM)
		vendor_thermal_shutdown_reason = vendor_thermal_shutdown_reason | VENDOR_THERMAL_SHUTDOWN_MODEM;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_WIFI)
		vendor_thermal_shutdown_reason = vendor_thermal_shutdown_reason | VENDOR_THERMAL_SHUTDOWN_WIFI;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_PCB)
		vendor_thermal_shutdown_reason = vendor_thermal_shutdown_reason | VENDOR_THERMAL_SHUTDOWN_PCB;

	vendor_reg_write(vendor_thermal_shutdown_reason);

	return 0;
}

static int (vendor_read_special_mode)(life_cycle_reason_t *special_mode)
{
	u32 vendor_smode;

	vendor_smode = vendor_reg_read();

	printk(KERN_ERR"%s: special mode is 0x%x\n", __func__, (vendor_smode & VENDOR_SPECIAL_MODE_MASK));
	if (vendor_smode & VENDOR_SPECIAL_MODE_LOW_BATTERY)
		*special_mode = LIFE_CYCLE_SMODE_LOW_BATTERY;
	else if (vendor_smode & VENDOR_SPECIAL_MODE_WARM_BOOT_USB_CONNECTED)
		*special_mode = LIFE_CYCLE_SMODE_WARM_BOOT_USB_CONNECTED;
	else if (vendor_smode & VENDOR_SPECIAL_MODE_OTA)
		*special_mode = LIFE_CYCLE_SMODE_OTA;
	else if (vendor_smode & VENDOR_SPECIAL_MODE_FACTORY_RESET)
		*special_mode = LIFE_CYCLE_SMODE_FACTORY_RESET;
	else {
		printk(KERN_ERR"Failed to read boot vendor boot reason\n");
		return -1;
	}
	return 0;
}

static int (vendor_write_special_mode)(life_cycle_reason_t special_mode)
{
	u32 vendor_smode;

	vendor_smode = vendor_reg_read();

	printk(KERN_INFO"%s: special_mode 0x%x\n", __func__, vendor_smode);

	if (special_mode == LIFE_CYCLE_SMODE_LOW_BATTERY)
		vendor_smode = vendor_smode | VENDOR_SPECIAL_MODE_LOW_BATTERY;
	else if (special_mode == LIFE_CYCLE_SMODE_WARM_BOOT_USB_CONNECTED)
		vendor_smode = vendor_smode | VENDOR_SPECIAL_MODE_WARM_BOOT_USB_CONNECTED;
	else if (special_mode == LIFE_CYCLE_SMODE_OTA)
		vendor_smode = vendor_smode | VENDOR_SPECIAL_MODE_OTA;
	else if (special_mode == LIFE_CYCLE_SMODE_FACTORY_RESET)
		vendor_smode = vendor_smode | VENDOR_SPECIAL_MODE_FACTORY_RESET;

	vendor_reg_write(vendor_smode);

	return 0;
}

int vendor_lcr_reset(void)
{
	vendor_reg_write(0);

	return 0;
}

int life_cycle_platform_init(sign_of_life_ops *sol_ops)
{
	void __iomem *virtual_reg = NULL;
	u32 rebootreason;

	printk(KERN_ERR "%s: Support vendor platform\n", __func__);
	sol_ops->read_boot_reason = vendor_read_boot_reason;
	sol_ops->write_boot_reason = vendor_write_boot_reason;
	sol_ops->read_shutdown_reason = vendor_read_shutdown_reason;
	sol_ops->write_shutdown_reason = vendor_write_shutdown_reason;
	sol_ops->read_thermal_shutdown_reason = vendor_read_thermal_shutdown_reason;
	sol_ops->write_thermal_shutdown_reason = vendor_write_thermal_shutdown_reason;
	sol_ops->read_special_mode = vendor_read_special_mode;
	sol_ops->write_special_mode = vendor_write_special_mode;
	sol_ops->lcr_reset = vendor_lcr_reset;

	virtual_reg = ioremap(STICKY_REG_BASE, 0x400);
	if (virtual_reg != NULL)
		vendor_reg = virtual_reg + STICKY_REG_OFFSET;
	else
		return -1;

	/*Check if there is boot/shutdown reason*/
	if (vendor_reg_read() == 0) {
		virtual_reg = ioremap(BOOT_REASON_REG_BASE, 0x400);
		if (virtual_reg != NULL) {
			virtual_reg += BOOT_REASON_REG_BOFFSET;
			rebootreason = ((readl(virtual_reg) >> 12) & 0xf);
			printk(KERN_INFO"Reboot reason register:0x%x \n", rebootreason);
			switch (rebootreason)
			{
				case VENDOR_BOOTMODE_COLD_BOOT:
				{
					sol_ops->write_boot_reason(COLDBOOT_BY_POWER_SUPPLY);
					break;
				}
				case VENDOR_BOOTMODE_NORMAL_BOOT:
				case VENDOR_BOOTMODE_BOOTLOADER_REBOOT:
				{
					sol_ops->write_boot_reason(WARMBOOT_BY_SW);
					break;
				}
				case VENDOR_BOOTMODE_FACTORY_RESET_REBOOT:
				{
					sol_ops->write_boot_reason(LIFE_CYCLE_SMODE_FACTORY_RESET);
					break;
				}
				case VENDOR_BOOTMODE_UPDATE_REBOOT:
				{
					sol_ops->write_boot_reason(LIFE_CYCLE_SMODE_OTA);
					break;
				}
				case VENDOR_BOOTMODE_KERNEL_PANIC:
				{
					sol_ops->write_boot_reason(WARMBOOT_BY_KERNEL_PANIC);
					break;
				}
				case VENDOR_BOOTMODE_WATCHDOG_REBOOT:
				{
					sol_ops->write_boot_reason(VENDOR_WARM_BOOT_HW_WDOG);
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}


	return 0;
}



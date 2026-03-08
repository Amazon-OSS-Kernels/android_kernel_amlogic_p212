/*
 * Thermal Framework Driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

/* This file was named thermal_framework.h */
#ifndef __LINUX_THERMAL_FRAMEWORK_H__
#define __LINUX_THERMAL_FRAMEWORK_H__

/**
 * struct virtual_thermal_dev_ops  - Structure for device operation call backs
 * @get_temp: A temp sensor call back to get the current temperature, 
 *            input: int - for channel number
 *                   long * - for temp value returns
 *            returns 0 if success
 *		temp is reported in milli degrees.
 */
struct virtual_thermal_dev_ops {
	int (*get_temp) (struct device *, int, unsigned long *);
};

/**
 * API to register a temperature sensor with the virtual thermal zone
 */
int virtual_sensor_dev_register(struct device *dev, struct virtual_thermal_dev_ops *dev_ops, int data);

#endif /* __LINUX_THERMAL_FRAMEWORK_H__ */

#ifndef ARCH__AML_COOLING_H__
#define ARCH__AML_COOLING_H__

#include <linux/amlogic/saradc.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/cpucore_cooling.h>
#include <linux/gpucore_cooling.h>
#include <linux/gpu_cooling.h>
#include <linux/thermal_core.h>
#include <linux/opp.h>
#include <linux/cpu.h>

enum aml_cool_dev_type {
	COOL_DEV_TYPE_CPU_FREQ = 0,
	COOL_DEV_TYPE_CPU_CORE,
	COOL_DEV_TYPE_GPU_FREQ,
	COOL_DEV_TYPE_GPU_CORE,
	COOL_DEV_TYPE_WIFI,
	COOL_DEV_TYPE_MAX
};

struct aml_cool_dev {
	int min_state;
	int coeff;
	int cluster_id;
	enum aml_cool_dev_type device_type;
	struct device_node *np;
	struct thermal_cooling_device *cooling_dev;
};

int aml_parse_register_cool_device(struct device_node *np, struct aml_cool_dev *cool);

#endif

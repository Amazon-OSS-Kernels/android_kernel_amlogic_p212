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

#include <linux/amlogic/aml_thermal_cooling.h>
#include <linux/amlogic/aml_thermal_hw.h>

#define NOT_WRITE_EFUSE		0x0
#define EFUEE_PRIVATE		0x4
#define EFUSE_OPS		0xa

extern struct thermal_cooling_device *wifi_cooling_register(struct device_node *np, int id);

static int get_cool_dev_type(const char *type)
{
	if (!strcmp(type, "cpufreq"))
		return COOL_DEV_TYPE_CPU_FREQ;
	if (!strcmp(type, "cpucore"))
		return COOL_DEV_TYPE_CPU_CORE;
	if (!strcmp(type, "gpufreq"))
		return COOL_DEV_TYPE_GPU_FREQ;
	if (!strcmp(type, "gpucore"))
		return COOL_DEV_TYPE_GPU_CORE;
	if (!strcmp(type, "wifi"))
		return COOL_DEV_TYPE_WIFI;
	return COOL_DEV_TYPE_MAX;
}

static int aml_register_cool_dev(struct aml_cool_dev *cool)
{
	int pp;
	int id = cool->cluster_id;

	switch (cool->device_type) {
	case COOL_DEV_TYPE_CPU_CORE:
		cool->cooling_dev = cpucore_cooling_register(cool->np,
							     cool->cluster_id);
		break;

	case COOL_DEV_TYPE_CPU_FREQ:
		cool->cooling_dev = of_cpufreq_power_cooling_register(cool->np,
							get_cpumask(id),
							cool->coeff,
							NULL);
		break;

	/* GPU is KO, just save these parameters */
	case COOL_DEV_TYPE_GPU_FREQ:
		if (of_property_read_u32(cool->np, "num_of_pp", &pp))
			pr_err("thermal: read num_of_pp failed\n");
		save_gpu_cool_para(cool->coeff, cool->np, pp);
		return 0;

	case COOL_DEV_TYPE_GPU_CORE:
		save_gpucore_thermal_para(cool->np);
		return 0;

	case COOL_DEV_TYPE_WIFI:
		cool->cooling_dev = wifi_cooling_register(cool->np, 0);
		return 0;

	default:
		pr_err("thermal: unknown cooling device type:%d\n", cool->device_type);
		return -EINVAL;
	}

	if (IS_ERR(cool->cooling_dev)) {
		pr_err("thermal: register cooling device type %d failed\n", cool->device_type);
		return -EINVAL;
	}

	return 0;
}

int aml_parse_register_cool_device(struct device_node *np, struct aml_cool_dev *cool)
{
	int temp, ret = 0;
	struct device_node *node, *child = NULL;
	const char *str;

	while ((child = of_get_next_child(np, child)) != NULL) {
		if (of_property_read_string(child, "device_type", &str))
			pr_err("thermal: read device_type failed\n");
		else
			cool->device_type = get_cool_dev_type(str);

		if (of_property_read_u32(child, "min_state", &temp))
			pr_err("thermal: read min_state failed\n");
		else
			cool->min_state = temp;

		if (cool->device_type == COOL_DEV_TYPE_CPU_FREQ ||
			cool->device_type == COOL_DEV_TYPE_CPU_CORE ||
			cool->device_type == COOL_DEV_TYPE_GPU_FREQ) {
			if (of_property_read_u32(child, "dyn_coeff", &temp))
				pr_err("thermal: read dyn_coeff failed\n");
			else
				cool->coeff = temp;

			if (of_property_read_u32(child, "cluster_id", &temp))
				pr_err("thermal: read cluster_id failed\n");
			else
				cool->cluster_id = temp;
		}

		if (of_property_read_string(child, "node_name", &str))
			pr_err("thermal: read node_name failed\n");
		else {
			node = of_find_node_by_name(NULL, str);
			if (!node)
				pr_err("thermal: can't find node\n");
			cool->np = node;
		}
		if (cool->np) {
			ret |= aml_register_cool_dev(cool);
			pr_info("thermal: register cooling device type %d ret %d\n", cool->device_type, ret);
		}
		cool++;

	}
	return ret;
}

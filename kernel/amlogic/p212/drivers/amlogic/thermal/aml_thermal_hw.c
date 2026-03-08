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
#include <linux/delay.h>
#include <linux/amlogic/aml_thermal_cooling.h>
#include <linux/amlogic/aml_thermal_hw.h>
#include <linux/virtual_sensor_thermal.h>

#define NOT_WRITE_EFUSE		0x0
#define EFUEE_PRIVATE		0x4
#define EFUSE_OPS		0xa

struct aml_thermal_sensor {
	int chip_trimmed;
	int cool_dev_num;
	int min_exist;
	struct cpumask mask[NUM_CLUSTERS];
	struct aml_cool_dev *cool_devs;
	struct thermal_zone_device    *tzd;
};

static struct aml_thermal_sensor soc_sensor;

int thermal_firmware_init(void)
{
	int ret;

	ret = scpi_get_sensor("aml_thermal");
	soc_sensor.chip_trimmed = ret < 0 ? 0 : 1;
	return ret;
}
EXPORT_SYMBOL(thermal_firmware_init);

#define CPU_TEMP_READ_ERR -1000

int get_cpu_temp(void)
{
	unsigned int val = 0;

	if (soc_sensor.chip_trimmed) {	/* only supported trimmed chips */
		if (scpi_get_sensor_value(0, &val) < 0)
			return CPU_TEMP_READ_ERR;
		return val;
	} else {
		return CPU_TEMP_READ_ERR;
	}
}
EXPORT_SYMBOL(get_cpu_temp);

struct cpumask * get_cpumask(int id)
{
	if (id >= NUM_CLUSTERS) return NULL;
	return &soc_sensor.mask[id];
}
EXPORT_SYMBOL(get_cpu_mask);

static int get_cur_temp(void *data, long *temp)
{
	int val, i;

	for (i=0; i<3; i++) {
		val = get_cpu_temp();
		pr_debug("%s, %d\n", __func__, val);
		if (val != CPU_TEMP_READ_ERR) break;
		msleep(10); /* contention with m3, so wait a while */
	}
	if (val == CPU_TEMP_READ_ERR) return -EINVAL;

	*temp = val * 1000;

	return 0;
}
static int get_cur_temp_for_virtual_sensor(void *data, int index, long *temp)
{
	int val;

	val = get_cpu_temp();
	pr_debug("%s, %d\n", __func__, val);
	if (val == CPU_TEMP_READ_ERR) return -EINVAL;

	*temp = val * 1000;

	return 0;
}

static struct aml_cool_dev *get_cool_dev_by_node(struct device_node *np)
{
	int i;
	struct aml_cool_dev *dev;

	if (!np)
		return NULL;
	for (i = 0; i < soc_sensor.cool_dev_num; i++) {
		dev = &soc_sensor.cool_devs[i];
		if (dev->np == np)
			return dev;
	}
	return NULL;
}

static bool ipa_instance(struct thermal_instance *ins)
{
	if (ins->tz && ins->tz->governor)
		if (!strcmp(ins->tz->governor->name, "power_allocator"))
			return true;

	return false;
}

int aml_thermal_min_update(struct thermal_cooling_device *cdev)
{
	struct gpufreq_cooling_device *gf_cdev;
	struct gpucore_cooling_device *gc_cdev;
	struct thermal_instance *ins;
	struct aml_cool_dev *cool;
	long min_state;
	int i;
	int cpu, c_id;

	cool = get_cool_dev_by_node(cdev->np);
	if (!cool)
		return -ENODEV;

	if (cool->cooling_dev == NULL)
		cool->cooling_dev = cdev;

	if (cool->min_state == 0)
		return 0;

	switch (cool->device_type) {
	case COOL_DEV_TYPE_CPU_CORE:
		/* TODO: cluster ID */
		cool->cooling_dev->ops->get_max_state(cdev, &min_state);
		min_state = min_state - cool->min_state;
		break;

	case COOL_DEV_TYPE_CPU_FREQ:
		for_each_possible_cpu(cpu) {
			if (mc_capable())
				c_id = topology_physical_package_id(cpu);
			else
				c_id = 0; /* force cluster 0 if no MC */
			if (c_id == cool->cluster_id)
				break;
		}
		min_state = cpufreq_cooling_get_level(cpu, cool->min_state);
		break;

	case COOL_DEV_TYPE_GPU_CORE:
		gc_cdev = (struct gpucore_cooling_device *)cdev->devdata;
		cdev->ops->get_max_state(cdev, &min_state);
		min_state = min_state - cool->min_state;
		break;

	case COOL_DEV_TYPE_GPU_FREQ:
		gf_cdev = (struct gpufreq_cooling_device *)cdev->devdata;
		min_state = gf_cdev->get_gpu_freq_level(cool->min_state);
		break;

	default:
		return -EINVAL;
	}

	for (i = 0; i < soc_sensor.tzd->trips; i++) {
		ins = get_thermal_instance(soc_sensor.tzd,
					   cdev, i);
		if (ins && ipa_instance(ins)){ /* only change for IPA governor */
			pr_info("%s changing upper from %d to %d\n", __func__, ins->upper, min_state);
			ins->upper = min_state;
		}
	}
	return 0;
}
EXPORT_SYMBOL(aml_thermal_min_update);

int set_cur_mode(struct thermal_zone_device *tzd, enum thermal_device_mode mode)
{
	int i, ret = 0;
	struct thermal_cooling_device *cdev;

	/*
	 * each cooling device should return to max state if thermal is disalbed
	 */
	if (mode != THERMAL_DEVICE_DISABLED)
		return 0;

	for (i = 0; i < soc_sensor.cool_dev_num; i++) {
		cdev = soc_sensor.cool_devs[i].cooling_dev;
		if (cdev)
			ret |= cdev->ops->set_cur_state(cdev, 0);
	}
	return ret;
}

static struct thermal_zone_of_device_ops aml_thermal_ops = {
	.get_temp = get_cur_temp,
	.set_mode = set_cur_mode,
};

static struct virtual_thermal_dev_ops aml_virtual_thermal_ops = {
        .get_temp = get_cur_temp_for_virtual_sensor,
};

static int aml_thermal_probe(struct platform_device *pdev)
{
	int cpu, i, c_id,ret;
	struct device_node *np, *child;
	struct aml_cool_dev *cool;


	memset(&soc_sensor, 0, sizeof(struct aml_thermal_sensor));
	if (!cpufreq_frequency_get_table(0)) {
		dev_info(&pdev->dev,
			"Frequency table not initialized. Deferring probe...\n");
		return -EPROBE_DEFER;
	}

	if (thermal_firmware_init() < 0) {
		dev_err(&pdev->dev, "chip is not trimmed, disable thermal\n");
		return -EINVAL;
	}

	for_each_possible_cpu(cpu) {
		if (mc_capable())
			c_id = topology_physical_package_id(cpu);
		else
			c_id = CLUSTER_BIG;	/* Always cluster 0 if no mc */
		if (c_id > NUM_CLUSTERS) {
			pr_err("Cluster id: %d > %d\n", c_id, NUM_CLUSTERS);
			return -EINVAL;
		}
		cpumask_set_cpu(cpu, &soc_sensor.mask[c_id]);
	}

	np = pdev->dev.of_node;
	child = of_get_child_by_name(np, "cooling_devices");
	if (child == NULL) {
		pr_err("thermal: can't found cooling_devices\n");
		return -EINVAL;
	}
	soc_sensor.cool_dev_num = of_get_child_count(child);
	i = sizeof(struct aml_cool_dev) * soc_sensor.cool_dev_num;
	soc_sensor.cool_devs = kzalloc(i, GFP_KERNEL);
	if (soc_sensor.cool_devs == NULL) {
		pr_err("thermal: alloc mem failed\n");
		return -ENOMEM;
	}

	if (aml_parse_register_cool_device(child, &soc_sensor.cool_devs[0]))
		return -EINVAL;

	soc_sensor.tzd = thermal_zone_of_sensor_register(&pdev->dev,
							  3,
							  &soc_sensor,
							  &aml_thermal_ops);

	if (IS_ERR(soc_sensor.tzd)) {
		dev_warn(&pdev->dev, "Error registering sensor: %p\n",
			 soc_sensor.tzd);
		return PTR_ERR(soc_sensor.tzd);
	}
	thermal_zone_device_update(soc_sensor.tzd);

	/* update min state for each device */
	for (i = 0; i < soc_sensor.cool_dev_num; i++) {
		cool = &soc_sensor.cool_devs[i];
		if (cool->cooling_dev)
			aml_thermal_min_update(cool->cooling_dev);
	}
	thermal_zone_device_update(soc_sensor.tzd);


	return 0;
}

static int aml_thermal_remove(struct platform_device *pdev)
{
	kfree(soc_sensor.cool_devs);
	return 0;
}

static struct of_device_id aml_thermal_of_match[] = {
	{ .compatible = "amlogic, aml-thermal" },
	{},
};

static struct platform_driver aml_thermal_platdrv = {
	.driver = {
		.name		= "aml-thermal",
		.owner		= THIS_MODULE,
		.of_match_table = aml_thermal_of_match,
	},
	.probe	= aml_thermal_probe,
	.remove	= aml_thermal_remove,
};


static int __init aml_thermal_platdrv_init(void)
{
	 return platform_driver_register(&(aml_thermal_platdrv));
}
device_initcall(aml_thermal_platdrv_init);

#ifndef ARCH__THERMAL_H__
#define ARCH__THERMAL_H__

enum cluster_type {
	CLUSTER_BIG = 0,
	CLUSTER_LITTLE,
	NUM_CLUSTERS
};

struct thermal_cooling_device;
extern int thermal_firmware_init(void);
extern int get_cpu_temp(void);
extern int aml_thermal_min_update(struct thermal_cooling_device *cdev);
extern struct cpumask * get_cpumask(int id);

#endif
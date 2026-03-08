#ifndef __DYNAMIC_BOOST_H__
#define __DYNAMIC_BOOST_H__

enum mode{
	PRIO_MAX_CORES,
	PRIO_MAX_CORES_MAX_FREQ,
	PRIO_RESET,
	/* Define the max priority for priority limit */
	PRIO_DEFAULT
};

enum control{
	OFF = -2,
	ON = -1
};

int set_dynamic_boost(int duration, int prio_mode);

#endif	/* __DYNAMIC_BOOST_H__ */

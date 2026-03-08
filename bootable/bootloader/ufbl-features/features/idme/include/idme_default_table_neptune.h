/*
 * idme_default_table_neptune.h
 *
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

/*!
 * @file idme_default_table_neptune.h
 * @brief This file contains idme default table values
 *  in the userstore partition
 *
 */

#ifndef __IDME_DEFAULT_TABLE_NEPTUNE_H__
#define __IDME_DEFAULT_TABLE_NEPTUNE_H__

/* If size field in idme_desc is updated, need to make sure
   the total size of all idme fields are  within CONFIG_IDME_SIZE
*/
const struct idme_init_values idme_default_values[] = {
	{ { "board_id", 16, 1, 0444 },
		/* Default Board ID value */
		"0"
	},
	{ { "serial", 16, 1, 0444 },
		/* Default DSN value */
		"0"
	},
	{ { "mac_addr", 16, 1, 0444 },
		/* Default 2G/4G wifi MAC address */
		"0"
	},
	{ { "mac_sec", 32, 1, 0440 },
		/* Default MAC secret */
		"0"
	},
	{ { "bt_mac_addr", 16, 1, 0444 },
		/* Default BT MAC address */
		"0"
	},
	{ { "bootmode", 4, 1, 0444 },
		/* Default Bootmode */
		"1"
	},
	{ { "postmode", 4, 1, 0444 },
		/* Default Postmode */
		"0"
	},
	{ { "bootcount", 8, 1, 0444 },
		/* Initial Bootcount */
		"0"
	},
	{ { "device_type_id", 32, 1, 0444 },
		/* Initial device type id */
		""
	},
	{ { "dev_flags", 8, 1, 0444 },
		/* device specific flag */
		"0"
	},
	{ { "fos_flags", 8, 1, 0444 },
		/* device specific flag */
		"0"
	},
	{ { "usr_flags", 8, 1, 0444 },
		/* device specific flag */
		"0"
	},
	{ { "manufacturing", 512, 1, 0444 },
		/* Manufacturer-specific data */
		""
	},
	{ { "unlock_code", 1024, 0, 0444 },
		/* Unlock code */
		""
	},
	{ { "locale", 8, 1, 0444 },
		/* device default locale */
		""
	},

	{ { "als_cal", 16, 1, 0440 },
		/* als calibration data */
		""
	},

	{ { "mic_all_cal", 64, 1, 0444 },
		/* microphone calibration data */
		""
	},

	{ { "scm_cal", 512, 1, 0444 },
		/* camera EEPROM data */
		""
	},

	{ { "suecreek_usb", 4, 1, 0444 },
		/* suecreek usb image loaded or not*/
		"0"
	},

	{ { "single_fault", 8, 1, 0444 },
		/* HW single fault detection */
		"0"
	},

	{ { "", 0, 0, 0 }, 0 },
};


#endif /* __IDME_DEFAULT_TABLE_NEPTUNE_H__ */

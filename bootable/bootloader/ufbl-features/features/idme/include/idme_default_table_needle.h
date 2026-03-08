/*
 * idme_default_table_needle.h
 *
 * Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

/*!
 * @file idme_default_table_needle.h
 * @brief This file contains idme default table values
 *  in the userstore partition
 *
 */

#ifndef __IDME_DEFAULT_TABLE_NEEDLE_H__

/* If size field in idme_desc is updated, need to make sure
   the total size of all idme fields are  within CONFIG_IDME_SIZE
   in include/idme.h. Also need to update external/MTK/idme.c */
const struct idme_init_values idme_default_values[] = {
	{ { "board_id", 16, 1, 0444 },
		/* Default Board ID value */
		"ffffff0000000000"
	},
	{ { "serial", 16, 1, 0444 },
		/* Default DSN value */
		"0"
	},
	{ { "mac_addr", 16, 1, 0444 },
		/* Default MAC address */
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
	{ { "bt_mfg", 128, 1, 0444 },
		/* Default BT MFG value */
		"0"
	},
	{ { "wifi_mfg", 1024, 1, 0444 },
		/* Default WIFI MFG value */
		"0"
	},
	{ { "product_name", 32, 1, 0444 },
		/* Product name, acos 2.4 */
		"0"
	},
	{ { "productid", 32, 1, 0444 },
		/* Default Primary Product ID */
		"0"
	},
	{ { "productid2", 32, 1, 0444 },
		/* Default Secondary Product ID */
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
	{ { "manufacturing", 512, 1, 0444 },
		/* Manufacturer-specific data */
		""
	},
	{ { "unlock_code", 1024, 1, 0444 },
		/* Unlock code */
		""
	},
	{ { "miccal.0", 16, 1, 0444 },
		/* MIC Calibration Data for stark */
		"0"
	},
	{ { "miccal.1", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "miccal.2", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "miccal.3", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "miccal.4", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "miccal.5", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "miccal.6", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "miccal.7", 16, 1, 0444 },
		/* MIC Calibration Data */
		"0"
	},
	{ { "alscal", 1024, 1, 0444 },
		/* Als Calibration Data for stark */
		""
	},
	{ { "device_type_id", 32, 1, 0444 },
		/* Initial device type id */
		"0"
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
	{ { "keys", 81920, 1, 0444 },
		/* for backup of keys, 80k */
		""
	},
	{ { "mfg.locale", 64, 1, 0444},
		/* mfg.locale, current values are en-US, de-DE, en-GB */
		"en-US"
	},
	{ { "", 0, 0, 0 }, 0 },
};


#endif /* __IDME_DEFAULT_TABLE_H__ */

/*
 * board_fdt_fixup.c
 *
 * Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include "amzn_secure_boot.h"
#include <common.h>
#include <malloc.h>
#include <fdt_support.h>
#include <libfdt.h>

#if defined(UFBL_FEATURE_UNLOCK)
extern int amzn_device_unlock_status;
#endif

extern bool secure_boot_enabled(void);

/*
 * read fos_flags from idme
 */
unsigned long get_fos_flags(void)
{
	unsigned long flags = 0;

#ifdef  CONFIG_IDME
	char fos_buf[16];
	int ret = 0;
	ret = idme_get_var_external("fos_flags", fos_buf, sizeof(fos_buf));

	if (ret < 0) {
		printf("get idme fos_flags Error\n");
		return 0;
	}
	flags =  simple_strtoul(fos_buf, NULL, 16);
#endif
	printf("fos_flags=%x\n", flags);
	return flags;
}


/*
 * Checks whether dm-verity is disabled
 * For locked production device , always return false
 * For unlocked/engineering device, check amazon fos_flags
 *      if bit7 is set, return true
 *      if bit7 is clear, return false
 */
/* TODO: Update code to use FOS_FLAGS_DM_VERITY_OFF inside idme.h */
#define	PLATFORM_FOS_FLAGS_DM_VERITY_OFF   (1 << 7)
bool amzn_dm_verity_is_off(int unlock_status)
{
	int lock_state;
	lock_state = ((unlock_status == 0)
		&& (amzn_target_device_type() != AMZN_ENGINEERING_DEVICE));

	if (lock_state) {
		/* Locked device: dm-verity is on and cannot be off */
		return false;
	} else if (get_fos_flags() & PLATFORM_FOS_FLAGS_DM_VERITY_OFF) {
		/*
		 * Unlocked/Engineering device with bit 7 set
		 * in fos_flags, dm-verity is off
		 */
		return true;
	} else {
		/* dm-verity is on otherwise */
		return false;
	}
}

int board_fixup_fdt(void *blob)
{
	int err, nodeoffset, len = 0;
	const char *str = NULL;
	char *new_str = NULL;
	size_t new_str_size = 0;

	err = fdt_check_header(blob);

	if (err < 0) {
		printf("%s: Invalid FDT blob: %s\n",
			__FUNCTION__, fdt_strerror(err));
		return err;
	}

	err = fdt_path_offset(blob, "/chosen");
	if (err < 0) {
		printf("%s: Cannot find /chosen node: %s\n",
			__FUNCTION__, fdt_strerror(err));
		return err;
	}
	nodeoffset = err;

	/* Retrieve the cmdline string */
	str = fdt_getprop_namelen(blob, nodeoffset,
			"bootargs", strlen("bootargs"), &len);

	if (!str) {
		printf("%s: Unable to locate bootargs property",
			__FUNCTION__);
		return -FDT_ERR_NOTFOUND;
	}

	/* Construct a new string adding our own fields */
	new_str_size = strlen(str) +
		strlen(" androidboot.secure_cpu=1 androidboot.prod=1" \
		       " androidboot.arb_enabled=1") + 1;
	new_str = malloc(new_str_size);

	if (!new_str) {
		printf("%s: Unable to allocate memory\n",
			__FUNCTION__);
		return -FDT_ERR_NOSPACE;
	}

	err = snprintf(new_str, new_str_size,
		"%s androidboot.secure_cpu=%d androidboot.prod=%d" \
		" androidboot.arb_enabled=%d", str,
		(secure_boot_enabled() == true) ? 1 : 0,
		(amzn_target_device_type() == AMZN_ENGINEERING_DEVICE) ? 0 : 1,
		run_command("query ARB", 0));

	if (err < 0) {
		printf("%s: snprintf error\n", __FUNCTION__);
	} else if (err >= new_str_size) {
		printf("%s: Truncated bootargs string\n", __FUNCTION__);
		free(new_str);
		return -FDT_ERR_TRUNCATED;
	}

#if defined(UFBL_FEATURE_UNLOCK)
#define MAX_UNLOCK_STR_SIZE 70
	char unlock_str[MAX_UNLOCK_STR_SIZE] = {0};

	err = snprintf(unlock_str, MAX_UNLOCK_STR_SIZE," androidboot.unlocked_kernel=%s androidboot.veritymode=%s ",
		(amzn_device_unlock_status == 1) ? "true" : "false",
		(amzn_dm_verity_is_off(amzn_device_unlock_status) == true) ? "disabled" : "eio");

        if (err < 0) {
                printf("%s: snprintf error\n", __FUNCTION__);
        } else if (err >= MAX_UNLOCK_STR_SIZE) {
                printf("%s: Truncated bootargs string\n", __FUNCTION__);
                free(new_str);
                return -FDT_ERR_TRUNCATED;
        }

	new_str_size += strlen(unlock_str) +1;
	new_str = realloc(new_str, new_str_size);
	if (!new_str) {
		printf("%s: Unable to allocate memory\n",
			__FUNCTION__);
		return -FDT_ERR_NOSPACE;
	}
	strncat(new_str, unlock_str, strlen(unlock_str) + 1);
#endif

	err = fdt_setprop(blob, nodeoffset,
		"bootargs", new_str, strlen(new_str) + 1);

	if (err < 0)
		printf("%s: fdt_setprop failed: %s",
			__FUNCTION__, fdt_strerror(err));

	free(new_str);

	return err;
}

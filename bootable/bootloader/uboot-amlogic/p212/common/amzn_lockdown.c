/* Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved. */

#include <common.h>

#if defined(UFBL_FEATURE_SECURE_BOOT)
#include "amzn_secure_boot.h"
#else
#error "UFBL_FEATURE_SECURE_BOOT is required"
#endif

#if defined(UFBL_FEATURE_UNLOCK)
#include "amzn_unlock.h"
#else
#error "UFBL_FEATURE_UNLOCK is required"
#endif

/*
 * At U-Boot prompt, we only allow fastboot and reset
 * commands unless the device is unlocked (via fastboot), OR
 * it is an engineering device
 */
static const char *whitelisted_commands[] = {
	"fastboot",
	"reset",
	"reboot"
};

static bool lockdown_commands = false;

void amzn_block_commands(void)
{
	lockdown_commands = true;
}

bool amzn_is_command_blocked(const char *cmd)
{
	int i = 0, found = 0;

	/* Are we in lock down? */
	if (lockdown_commands == false)
		return false;

	/* Is this an engineering device? */
	if (amzn_target_device_type() == AMZN_ENGINEERING_DEVICE)
		return false;

	/* Are we un-locked? */
	if (amzn_target_is_unlocked())
		return false;

	/* If command is on the white-list, allow */
	for (i = 0; i < ARRAY_SIZE(whitelisted_commands); i++)
		if (strcmp(whitelisted_commands[i], cmd) == 0)
			found = 1;

	/* Not on the white-list? Block */
	if (!found)
		return true;

	return false;
}

/*
 * Copyright (C) 2016 - 2019 Amazon.com Inc. or its affiliates.  All Rights Reserved.
*/

#ifdef SUPPORT_UBOOT

#include "bcb_platform.h"
#include "bootctrl.h"
#include "bcb.h"
#include "cmd_bcb.h"
#include "command.h"
#include "fastboot.h"
#include "ufbl_debug.h"

/*
 * The code here is used by both the command line bcb command and fastboot oem bcb.
 *
 * The command line command can be disabled via CONFIG_CMD_DISABLE_BCB - this is
 * in contrast to other u-boot commands which are normally explicitly enabled,
 * but this disable functionality was added after the command was introduced, so
 * a disable define is used to avoid potentially updating multiple platforms.
 *
 * The code guard here is because none of this code required if fastboot is
 * not enabled and the command is disabled
 */
#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT) || !(defined CONFIG_CMD_DISABLE_BCB)

#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT)
static void *fastboot_context = NULL;
#endif

static int do_bcb_info(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char response[FASTBOOT_RESPONSE_LEN];
	snprintf(response, sizeof(response), "Active slot: %d", ufbl_get_active_slot());
	puts(response);puts("\n");
#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT)
	if (fastboot_context)
		fastboot_info(fastboot_context, response);
#endif
	return 0;
}

static int do_bcb_read(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	boot_ctrl_t bctrl;
	int ret = -1;
	bool bcb_read = ufbl_get_boot_ctrl(&bctrl);
	if (bcb_read) {
		printf("Slot 0: [priority: %d, tries: %d, successful: %d]\nSlot 1: [priority: %d, tries: %d, successful: %d]\n",
				bctrl.slot_info[0].priority, bctrl.slot_info[0].tries_remaining, bctrl.slot_info[0].successful_boot,
				bctrl.slot_info[1].priority, bctrl.slot_info[1].tries_remaining, bctrl.slot_info[1].successful_boot);

#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT)
		if (fastboot_context) {
			char response[FASTBOOT_RESPONSE_LEN];
			/* Necessary to be terse to fit it in */
			snprintf(response, sizeof(response), "S0: P%d T%d S%d S1: P%d T%d S%d",
				bctrl.slot_info[0].priority, bctrl.slot_info[0].tries_remaining, bctrl.slot_info[0].successful_boot,
				bctrl.slot_info[1].priority, bctrl.slot_info[1].tries_remaining, bctrl.slot_info[1].successful_boot);
			fastboot_info(fastboot_context, response);
		}
#endif

		ret = 0;
	}

	return ret;
}

static int do_bcb_write(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	boot_ctrl_t bctrl;
	int ret = -1;

	if (argc != 7) {
		static const char reason[] = "Incorrect number of parameters";
		puts(reason);puts("\n");
#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT)
		if (fastboot_context)
			fastboot_fail(fastboot_context, reason);
#endif
		goto exit;
	}

	bool bcb_read = ufbl_get_boot_ctrl(&bctrl);
	if (bcb_read) {
		bctrl.slot_info[0].priority = simple_strtoul(argv[1], NULL, 10);
		bctrl.slot_info[0].tries_remaining = simple_strtoul(argv[2], NULL, 10);
		bctrl.slot_info[0].successful_boot = simple_strtoul(argv[3], NULL, 10);
		bctrl.slot_info[1].priority = simple_strtoul(argv[4], NULL, 10);
		bctrl.slot_info[1].tries_remaining = simple_strtoul(argv[5], NULL, 10);
		bctrl.slot_info[1].successful_boot = simple_strtoul(argv[6], NULL, 10);

		if (ufbl_set_boot_ctrl(&bctrl)) {
			ret = 0;
		}
	}
exit:
	return ret;
}

/**
 * @brief Swap active slot
 *
 * Modifies the priority values for the slots such that the active slot
 * evaluation will return the alternate slot.
 * Will always ensure the new active slot priority is non-zero, allowing
 * recovery when both slots are set to unbootable
 *
 * @param cmdtp Unused (standard function header for u-boot commands)
 * @param flag Unused (standard function header for u-boot commands)
 * @param argc Unused (standard function header for u-boot commands)
 * @param argv Unused (standard function header for u-boot commands)
 *
 * @return 0 on success, -1 on failure
 */
static int do_bcb_swap(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	boot_ctrl_t bctrl;
	int ret = -1;
	bool bcb_read = ufbl_get_boot_ctrl(&bctrl);
	if (bcb_read) {
		slot_metadata_t *old_slot;
		slot_metadata_t *new_slot;
		int curr_slot = ufbl_get_active_slot();
		if (curr_slot != 1)
			curr_slot = 0;
		old_slot = &bctrl.slot_info[curr_slot];
		new_slot = &bctrl.slot_info[(size_t)curr_slot ^ 1];

		new_slot->priority = BCB_DEFAULT_ACTIVE_SLOT_PRIORITY;
		if (old_slot->priority > BCB_DEFAULT_OTHER_SLOT_PRIORITY)
			old_slot->priority = BCB_DEFAULT_OTHER_SLOT_PRIORITY;
		if (ufbl_set_boot_ctrl(&bctrl)) {
			printf("Active slot: %d", ufbl_get_active_slot());
			ret = 0;
		}
	}

	return ret;
}

static int do_bcb_get_boot_part(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
#if 0
	const char *active_part_name = NULL;
	lbaint_t start_block;
	int blocksize;
	int active_slot = ufbl_get_active_slot();
	int ret = -1;

	active_part_name = platform_get_boot_partition_name(active_slot);
	if (active_part_name) {
		char response[FASTBOOT_RESPONSE_LEN];
		if (platform_get_partition_info(active_part_name, &start_block, &blocksize)) {
			snprintf(response, sizeof(response), "Active boot partition is %s @ " LBAF, active_part_name, start_block);
#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT)
			if (fastboot_context) {
				fastboot_info(fastboot_context, response);
				strncat(response, "\n", sizeof(response));
				dprintf(INFO, response);
			}
#endif
			ret = setenv_hex("bootpart", start_block);
		} else {
			dprintf(CRITICAL, "Failed to retrieve eMMC partition info\n");
		}
	} else {
		dprintf(CRITICAL, "Invalid active slot %d\n", active_slot);
	}

	return ret;
#endif
	return 0;
}

/**
 * @brief Set the active slot to the given value
 *
 * Set the active slot to a specific one.  The slot should either be specified
 * using 0/1 naming (to correspond to the BCB array entry 0 or 1), or using
 * textual descriptions provided in the "slot-suffixes" environment variable.
 * This environment variable is also  used by fastboot to determine if slots
 * are supported by the platform and hence allow --slot and --set-active
 * parameters to be specified in u-boot.  This function is called in the
 * fastboot --set-active case.
 *
 * Will always ensure the new active slot priority is non-zero, allowing
 * recovery when both slots are set to unbootable
 *
 * @param cmdtp Pointer to current entry in the command table
 * @param flag Unused (standard function header for u-boot commands)
 * @param argc Number of command arguments
 * @param argv Array of command arguments
 *
 * @return 0 on success, -1 on failure
 */
static int do_bcb_set_active(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
#if 0
	boot_ctrl_t bctrl;
	int ret = -1;
	int slot_val = -1;
	char *slot_suffixes = NULL;
	slot_metadata_t *act_slot = NULL;
	slot_metadata_t *other_slot = NULL;

	if (argc != cmdtp->maxargs) {
		fastboot_printf(CRITICAL, FASTBOOT_FAIL,
				"Incorrect number of parameters");
		goto exit;
	}

	if (!ufbl_get_boot_ctrl(&bctrl)) {
		fastboot_printf(CRITICAL, FASTBOOT_FAIL,
				"Failed to read slot data");
		goto exit;
	}

	if (strncmp(argv[1], "0", 1) == 0) {
		slot_val = 0;
	} else if (strncmp(argv[1], "1", 1) == 0) {
		slot_val = 1;
	} else if ((slot_suffixes = getenv("slot-suffixes")) != NULL) {
		/* slot_suffixes is a comma separated list of suffixes.  This is
		 * arbitrary, but we need a way of relating it to our numeric
		 * values.  This code assumes the slots are listed in order -
		 * eg _a,_b maps to 0,1.  If they are listed in reverse order
		 * this code will get things wrong, so don't do that. */
		char *sep = strchr(slot_suffixes, ',');
		if (sep) {
			if (strncmp(argv[1], slot_suffixes, sep - slot_suffixes) == 0) {
				/* Matches first slot */
				slot_val = 0;
			} else if (strncmp(argv[1], sep + 1, sizeof(argv[1])) == 0) {
				slot_val = 1;
			}
		}
	}
	if (slot_val < 0) {
		fastboot_printf(CRITICAL, FASTBOOT_FAIL,
				"Failed to find slot %s\n", argv[1]);
		goto exit;
	}

	act_slot = &bctrl.slot_info[slot_val];
	other_slot = &bctrl.slot_info[(size_t)slot_val ^ 1];
	if (act_slot->priority <= other_slot->priority) {
		if (act_slot->priority == 0) {
			act_slot->successful_boot = 0;
		}
		act_slot->priority = BCB_DEFAULT_ACTIVE_SLOT_PRIORITY;
		if (other_slot->priority > BCB_DEFAULT_OTHER_SLOT_PRIORITY)
			other_slot->priority = BCB_DEFAULT_OTHER_SLOT_PRIORITY;
		if (ufbl_set_boot_ctrl(&bctrl)) {
			ret = 0;
		}
	} else {
		/* Correct slot already set, no action - return success */
		ret = 0;
	}

exit:
	return ret;
#endif
	return 0;
}


/***************************************************/
static cmd_tbl_t cmd_bcb[] = {
	U_BOOT_CMD_MKENT(info, 1, 0, do_bcb_info, "", ""),
	U_BOOT_CMD_MKENT(read, 1, 0, do_bcb_read, "", ""),
	U_BOOT_CMD_MKENT(write, 7, 0, do_bcb_write, "", ""),
	U_BOOT_CMD_MKENT(swap, 1, 0, do_bcb_swap, "", ""),
	U_BOOT_CMD_MKENT(bootpart, 1, 0, do_bcb_get_boot_part, "", ""),
};

static int do_bcb_uboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	cp = find_cmd_tbl(argv[1], cmd_bcb, ARRAY_SIZE(cmd_bcb));

	/* Drop the bcb command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;

	return cp->cmd(cmdtp, flag, argc, argv);
}

#ifndef CONFIG_CMD_DISABLE_BCB
U_BOOT_CMD(
		bcb, 8, 0, do_bcb_uboot,
		"bcb support",
		"info       :  Get information about the bcb\n"
		"bcb read       :  Read bcb data\n"
		"bcb write      :  Write bcb data priority0 tries0 successful0 priority1 tries1 successful1\n"
		"bcb swap       :  Swap the current partition in the bcb (swap priority0 / priority1)\n"
		"bcb set_active :  Set the active slot to the specified value (either 0/1 or slots defined in slot-suffixes env var)\n"
		"bcb bootpart   :  Get the block number of the partition to boot the kernel from (based on current active slot)\n"
);
#endif

#if (defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT)
int fastboot_bcb(char response[FASTBOOT_RESPONSE_LEN - 4], char *cmd)
{
#if 0
	char *argv[9];	/* Enough for any BCB subcommand */
	int argc = 0;
	char *ptr = cmd;
	int inword = 0;
	int ret;
	while (*ptr)	/* Caller guarantees null terminated string */
	{
		if (inword) {
			/* Space is the end of the word */
			if (*ptr == ' ') {
				*ptr = 0;
				inword = 0;
			}
		} else {
			if (*ptr != ' ') {
				argv[argc++] = ptr;
				inword = 1;
			}
		}
		++ptr;

		if (argc >= (sizeof(argv)) / sizeof(*argv)) {
			return -1;
		}
	}

	fastboot_context = response;

	ret = do_bcb_uboot(0, 0, argc, argv);

	fastboot_context = NULL;

	return ret;
#endif
	return 0;
}
#endif

#endif  //(defined CONFIG_FASTBOOT_UDP) || (defined CONFIG_USB_FUNCTION_FASTBOOT) || !(defined CONFIG_CMD_DISABLE_BCB)
#endif //SUPPORT_UBOOT

/*
 * Copyright (C) 2016 - 2019 Amazon.com, Inc. or its Affiliates. All rights reserved.
 */

#if !defined(SUPPORT_UBOOT) && !defined(SUPPORT_MTK_PRELOADER)
    #include <string.h>
    #ifdef CONFIG_BCB_RANDOMISE_DEFAULT
        #include <rand.h>
    #endif
    #include <debug.h>
#else
    #include "ufbl_debug.h"
#endif
#include "bcb.h"
#include "bcb_platform.h"
#include "bootctrl.h"

static bool ufbl_read_misc(char *misc_buf, int size)
{
    int retry = BCB_MAX_RETRY_LIMIT;
    while(retry --) {
        if(true == platform_read_partition("misc", misc_buf, size))
            return true;
    }

    return false;
}

static bool ufbl_write_misc(char *misc_buf, int size)
{
    int retry = BCB_MAX_RETRY_LIMIT;
    while(retry --) {
        if(true == platform_write_partition("misc", misc_buf, size))
            return true;
    }
    return false;
}

static void create_default_metadata(boot_ctrl_t *bctrl, int default_active_slot)
{
    dprintf(INFO, "create_default_metadata, default_active_slot = %d\n", default_active_slot);

    bctrl->magic = BOOTCTRL_MAGIC;
    bctrl->version = BOOT_CONTROL_VERSION;

    if(default_active_slot == 0) {
        bctrl->slot_info[0].priority = BCB_DEFAULT_ACTIVE_SLOT_PRIORITY;
        bctrl->slot_info[1].priority = BCB_DEFAULT_OTHER_SLOT_PRIORITY;
    }
    else {
        bctrl->slot_info[0].priority = BCB_DEFAULT_OTHER_SLOT_PRIORITY;
        bctrl->slot_info[1].priority = BCB_DEFAULT_ACTIVE_SLOT_PRIORITY;
    }

    bctrl->slot_info[0].tries_remaining = BCB_MAX_RETRY_LIMIT;
    bctrl->slot_info[0].successful_boot = 0;

    bctrl->slot_info[1].tries_remaining = BCB_MAX_RETRY_LIMIT;
    bctrl->slot_info[1].successful_boot = 0;
}

bool ufbl_get_boot_ctrl(boot_ctrl_t *bctrl)
{
    bool ret = false;
    char misc_buf[OFFSETOF_SLOT_SUFFIX + sizeof(boot_ctrl_t)];
    boot_ctrl_t *bctrl_read = (boot_ctrl_t *)&misc_buf[OFFSETOF_SLOT_SUFFIX];

    if(platform_support_pingpong() && bctrl) {
        if(ufbl_read_misc(misc_buf, sizeof(misc_buf))) {
            memcpy(bctrl, bctrl_read, sizeof(boot_ctrl_t));
            ret = true;
        }
    }

    return ret;
}

bool ufbl_set_boot_ctrl(const boot_ctrl_t *bctrl)
{
    bool ret = false;
    char misc_buf[OFFSETOF_SLOT_SUFFIX + sizeof(boot_ctrl_t)];
    boot_ctrl_t *bctrl_write = (boot_ctrl_t *)&misc_buf[OFFSETOF_SLOT_SUFFIX];

    if(platform_support_pingpong() && bctrl) {
        memcpy(bctrl_write, bctrl, sizeof(boot_ctrl_t));
        if(ufbl_write_misc(misc_buf, sizeof(misc_buf))) {
            ret = true;
        }
    }

    return ret;
}

static void ufbl_print_boot_ctrl(const boot_ctrl_t *bctrl)
{
    dprintf(CRITICAL, "============================================================\n");
    dprintf(CRITICAL, "Boot Ctrl - Magic                    : 0x%x\n", bctrl->magic);
    dprintf(CRITICAL, "Boot Ctrl - Version                  : %d\n", bctrl->version);
    dprintf(CRITICAL, "Boot Ctrl - Recovery Tries Remaining : %d\n", bctrl->recovery_tries_remaining);
    dprintf(CRITICAL, "Boot Ctrl - Slot 0 Priority          : %d\n", bctrl->slot_info[0].priority);
    dprintf(CRITICAL, "Boot Ctrl - Slot 0 Tries Remaining   : %d\n", bctrl->slot_info[0].tries_remaining);
    dprintf(CRITICAL, "Boot Ctrl - Slot 0 Successful Boot   : %d\n", bctrl->slot_info[0].successful_boot);
    dprintf(CRITICAL, "Boot Ctrl - Slot 1 Priority          : %d\n", bctrl->slot_info[1].priority);
    dprintf(CRITICAL, "Boot Ctrl - Slot 1 Tries Remaining   : %d\n", bctrl->slot_info[1].tries_remaining);
    dprintf(CRITICAL, "Boot Ctrl - Slot 1 Successful Boot   : %d\n", bctrl->slot_info[1].successful_boot);
    dprintf(CRITICAL, "============================================================\n");
}

/**
 * Returns true whether the active (highest priority) slot has failed to boot
 * and false otherwise.
 */
static bool ufbl_last_boot_failed_pingpong(const boot_ctrl_t *bctrl)
{
    return (bctrl->slot_info[0].priority > bctrl->slot_info[1].priority) ?
        (!bctrl->slot_info[0].successful_boot) :
        (!bctrl->slot_info[1].successful_boot);
 }

static int ufbl_get_active_slot_internal(unsigned long long flags)
{
    char misc_buf[OFFSETOF_SLOT_SUFFIX + sizeof(boot_ctrl_t)];
    boot_ctrl_t *bctrl = (boot_ctrl_t *)&misc_buf[OFFSETOF_SLOT_SUFFIX];
    int active_slot = -1;
    bool write_back = false;
    int default_slot = 0;
#ifdef CONFIG_BCB_RANDOMISE_DEFAULT
    default_slot = rand() % 2;
#endif

    if(false == platform_support_pingpong())
        return active_slot;

    memset(misc_buf, 0, sizeof(misc_buf));

    // read BCB
    if(false == ufbl_read_misc(misc_buf, sizeof(misc_buf))) {
        memset(misc_buf, 0, sizeof(misc_buf));

        create_default_metadata(bctrl, default_slot);
        if (flags & BCB_FLAGS_CREATE_ON_CORRUPT_BCB) {
            write_back = true;
        }
    }
    else {
        dprintf(INFO, "read misc: %06x %08x %08x\n", OFFSETOF_SLOT_SUFFIX, *(int *)bctrl, *((int *)bctrl + 1));
    }

    if (flags & BCB_FLAGS_PRINT_BCB) {
        ufbl_print_boot_ctrl(bctrl);
    }

    // sanity check
    if(bctrl->magic != BOOTCTRL_MAGIC) {
        dprintf(CRITICAL, "metadata is not initialized or corrupted.  Recreating default metadata.\n");
        memset(misc_buf, 0, sizeof(misc_buf));
        create_default_metadata(bctrl, default_slot);
        if (flags & BCB_FLAGS_CREATE_ON_CORRUPT_BCB) {
            write_back = true;
        }
    }

    if(bctrl->slot_info[0].priority > bctrl->slot_info[1].priority)
        active_slot = 0;
    else if(bctrl->slot_info[0].priority < bctrl->slot_info[1].priority)
        active_slot = 1;
    else {
        if(bctrl->slot_info[0].priority > 0) {
            memset(misc_buf, 0, sizeof(misc_buf));
            create_default_metadata(bctrl, 0);
            if (flags & BCB_FLAGS_CREATE_ON_CORRUPT_BCB) {
                write_back = true;
            }
        }
        else {
            if (flags & BCB_FLAGS_HANG_ON_RETRY_LIMIT) {
                // OMG
                while(1);
            }
            return -1;
        }
    }

    if(active_slot >= 0 && (flags & BCB_FLAGS_DECREMENT_TRIES_REMAINING)) {
        if(platform_last_boot_failed_pingpong() || ((flags & BCB_FLAGS_LAST_BOOT_FAILED_PINGPONG) && ufbl_last_boot_failed_pingpong(bctrl))) {
            if(bctrl->slot_info[active_slot].tries_remaining > 0) {
                bctrl->slot_info[active_slot].tries_remaining --;
            }
            else {
                bctrl->slot_info[active_slot].priority = 0;
                bctrl->slot_info[active_slot].successful_boot = 0;
                active_slot = (active_slot == 0) ? 1 : 0;
                if(bctrl->slot_info[active_slot].priority == 0) {
                    if (flags & BCB_FLAGS_HANG_ON_RETRY_LIMIT) {
                        // OMG
                        while(1);
                    }
                    return -1;
                }
            }
            write_back = true;
        }
    }

    if(write_back) {
        dprintf(CRITICAL, "write misc: %06x %08x %08x\n", OFFSETOF_SLOT_SUFFIX, *(int *)bctrl, *((int *)bctrl + 1));

        if(true == ufbl_write_misc(misc_buf, sizeof(misc_buf))) {
            // for debug purpose
            memset(misc_buf, 0, sizeof(misc_buf));
            ufbl_read_misc(misc_buf, sizeof(misc_buf));
            dprintf(SPEW, "readback misc: %06x %08x %08x\n", OFFSETOF_SLOT_SUFFIX, *(int *)bctrl, *((int *)bctrl + 1));
        }
        else {
            // ignore error and continue to boot
        }
    }

    dprintf(INFO, "active_slot is %d\n", active_slot);

    return active_slot;
}

int ufbl_get_active_slot(void)
{
    return ufbl_get_active_slot_internal(BCB_FLAGS_NONE);
}

int ufbl_get_active_slot_ex(unsigned long long flags)
{
    return ufbl_get_active_slot_internal(flags);
}

int ufbl_prevent_slot_rollback(void)
{
    boot_ctrl_t bctrl;
    int ret, alt_slot;

    ret = ufbl_get_active_slot_internal(BCB_FLAGS_NONE);

    // Give more verbose output for calls from non-AB configured devices.
    if (ret != 0 && ret != 1) {
        dprintf(CRITICAL, "Non-AB device.  Rollback feaure does not exist.\n");
        return BCB_ERROR_PREVENT_ROLLBACK_NOT_AB;
    }

    if (!ufbl_get_boot_ctrl(&bctrl)) {
        dprintf(CRITICAL, "ERROR: Failed to get boot_ctrl object.\n");
        return BCB_ERROR_PREVENT_ROLLBACK_GET_BOOT_CTRL;
    }

    // prevent rollback to previous slot.
    alt_slot = !ret;

    bctrl.slot_info[alt_slot].successful_boot = 0;
    bctrl.slot_info[alt_slot].priority = 0;
    bctrl.slot_info[alt_slot].tries_remaining = 0;

    if (!ufbl_set_boot_ctrl(&bctrl)) {
        dprintf(CRITICAL, "ERROR: Failed setting boot_ctrl object.\n");
        return BCB_ERROR_PREVENT_ROLLBACK_SET_BOOT_CTRL;
    }

    dprintf(CRITICAL, "Alternate slot %d marked unbootable.\n", alt_slot);
    return 0;
}


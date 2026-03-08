/*
 * Copyright (C) 2016 - 2019 Amazon.com Inc. or its affiliates.  All Rights Reserved.
 */

#ifndef __UFBL_BOOT_H
#define __UFBL_BOOT_H

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

// BCB flag bitmasks - designed to be OR-ed together

// Do not perform any special operations when trying to get the active slot.
#define BCB_FLAGS_NONE                         (0x00000000ull)

// Re-create and re-initialize BCB to default values if reading corrupt or
// invalid data from the misc partition
#define BCB_FLAGS_CREATE_ON_CORRUPT_BCB        (0x00000001ull)

// If reading the BCB on a slot that has failed to boot, decrement 'tries
// remaining' by one.  If 'tries remaining' is zero, we revert back to the
// previous succussfully booted slot.  There is a total of
// BCB_MAX_RETRY_LIMIT tries.
#define BCB_FLAGS_DECREMENT_TRIES_REMAINING    (0x00000002ull)

// When reading the BCB, print important debug information, useful for developer
// debugging and validation
#define BCB_FLAGS_PRINT_BCB                    (0x00000004ull)

// Use internal BCB to determine whether the active slot can boot.  This flag is
// only applicable if platform_last_boot_failed_pingpong() returns false and
// is most likely needed if the caller is using BCB_FLAGS_DECREMENT_TRIES_REMAINING
#define BCB_FLAGS_LAST_BOOT_FAILED_PINGPONG    (0x00000008ull)

// When 'tries remaining' is zero and no slots left to boot, hangs with an
// infinite loop.  If flag is not specified, an error is returned.
#define BCB_FLAGS_HANG_ON_RETRY_LIMIT          (0x00000010ull)


/* Gets the active slot.  This function is safe to call multiple times. Returns
   either 0 or 1 which corresponds to slot '_a' and '_b' respectively or -1 if
   an error occurred.*/
int ufbl_get_active_slot(void);

/* Gets the active slot, but uses flags (defined above) to determine special
   behavior.  Returns either 0 or 1 which corresponds to slot '_a' and '_b'
   respectively or -1 if an error occurred.*/
int ufbl_get_active_slot_ex(unsigned long long flags);

/* Clear the alternate slot "successful" status on AB OTA configured devices
 * to prevent the the possibility of a rollback.  This is needed for anti-
 * rollback cases to prevent an boot slot rollback event which would "brick"
 * the device is such a way that is not debuggable.  Note that preventing
 * AB OTA rollbacks could also lead to a bricked device, but can still produce
 * a device bootup state that is still possibly debuggable unlike an anti-rollback
 * bricked state.
 */
int ufbl_prevent_slot_rollback(void);

#define BCB_ERROR_PREVENT_ROLLBACK_NOT_AB          (-1)
#define BCB_ERROR_PREVENT_ROLLBACK_GET_BOOT_CTRL   (-2)
#define BCB_ERROR_PREVENT_ROLLBACK_SET_BOOT_CTRL   (-3)

#define BCB_MAX_SLOT_PRIORITY 15
#define BCB_MAX_RETRY_LIMIT   3
#define BCB_DEFAULT_ACTIVE_SLOT_PRIORITY BCB_MAX_SLOT_PRIORITY
#define BCB_DEFAULT_OTHER_SLOT_PRIORITY (BCB_MAX_SLOT_PRIORITY - 1)
/* The code requires active slot priority > other slot priority.  Verify this */
STATIC_ASSERT(BCB_DEFAULT_ACTIVE_SLOT_PRIORITY > BCB_DEFAULT_OTHER_SLOT_PRIORITY, check_slot_priority);
#endif


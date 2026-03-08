/*
 * Copyright (C) 2016 - 2019 Amazon.com Inc. or its affiliates.  All Rights Reserved.
 */

#ifndef FEATURES_BCB_BCB_PLATFORM_H_
#define FEATURES_BCB_BCB_PLATFORM_H_

#if defined(SUPPORT_UBOOT)
#if !(defined __STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
typedef enum
{
    false = ( 1 == 0 ),
    true = ( ! false )
} bool;
#else
#include <stdbool.h>
#endif

#include <common.h>

#elif defined(SUPPORT_MTK_PRELOADER)
#include <typedefs.h> /* for bool support */
#endif // SUPPORT_UBOOT

bool platform_read_partition(const char *partition, char *misc_buf, int size);
bool platform_write_partition(const char *partition, char *misc_buf, int size);
bool platform_support_pingpong(void);
bool platform_last_boot_failed_pingpong(void);

/* Platform helper functions for cmd_bcb */
#if defined(SUPPORT_UBOOT) && (!(defined CONFIG_SPL_BUILD) || defined CONFIG_SPL_LIBDISK_SUPPORT)
bool platform_get_partition_info(const char *partition, lbaint_t *start_block, int *blocksize);
#endif
const char* platform_get_boot_partition_name(int active_slot);

#endif /* FEATURES_BCB_BCB_PLATFORM_H_ */

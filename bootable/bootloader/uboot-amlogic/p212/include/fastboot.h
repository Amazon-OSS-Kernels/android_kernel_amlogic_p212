/*
 * (C) Copyright 2016
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * fastboot header file
 */
#ifndef _FASTBOOT_H_
#define _FASTBOOT_H_

void fastboot_info(const char *reason);
void fastboot_register(const char *prefix,
                       void (*handle)(const char *arg, void *data, unsigned sz), unsigned char security_enabled);

#endif	/* _FASTBOOT_H_ */

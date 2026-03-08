
/*
 * common/cmd_cpuinfo.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <common.h>
#include <asm/io.h>
#include <command.h>
#include <asm/arch/secure_apb.h>
#include <asm/arch/mailbox.h>
#include <asm/cpu_id.h>


static int do_cpuinfo(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int family_id = 0xff;
	int revision = 0xf;
	family_id = get_cpu_id().family_id;
	revision = get_cpu_id().chip_rev;
	switch(family_id) {
		case MESON_CPU_MAJOR_ID_M6 :
			printf("family_id:MESON_CPU_MAJOR_ID_M6\n");
			break;
		case MESON_CPU_MAJOR_ID_M6TV :
			printf("family_id:MESON_CPU_MAJOR_ID_M6TV\n");
			break;
		case MESON_CPU_MAJOR_ID_M6TVL :
			printf("family_id:MESON_CPU_MAJOR_ID_M6TVL\n");
			break;
		case MESON_CPU_MAJOR_ID_M8 :
			printf("family_id:define MESON_CPU_MAJOR_ID_M8\n");
			break;
		case MESON_CPU_MAJOR_ID_MTVD :
			printf("family_id:MESON_CPU_MAJOR_ID_MTVD\n");
			break;
		case MESON_CPU_MAJOR_ID_M8B :
			printf("family_id:MESON_CPU_MAJOR_ID_M8B\n");
			break;
		case MESON_CPU_MAJOR_ID_MG9TV :
			printf("family_id:MESON_CPU_MAJOR_ID_MG9TV\n");
			break;
		case MESON_CPU_MAJOR_ID_M8M2 :
			printf("family_id:MESON_CPU_MAJOR_ID_M8M2\n");
			break;
		case MESON_CPU_MAJOR_ID_GXBB :
			printf("family_id:MESON_CPU_MAJOR_ID_GXBB\n");
			break;
		case MESON_CPU_MAJOR_ID_GXL :
			printf("family_id:MESON_CPU_MAJOR_ID_GXL\n");
			break;
		case MESON_CPU_MAJOR_ID_GXM :
			printf("family_id:MESON_CPU_MAJOR_ID_GXM\n");
			break;
		case MESON_CPU_MAJOR_ID_TXL :
			printf("family_id:MESON_CPU_MAJOR_ID_TXL\n");
			break;
		default:
			printf("family_id:UNKOWN\n");
			break;
	}

	if (revision == 0xA) {
		printf("revision:0xA\n");
	} else if (revision == 0xB) {
		printf("revision:0xB\n");
	} else if (revision == 0xC) {
		printf("revision:0xC\n");
	} else if (revision == 0xD) {
		printf("revision:0xD\n");
	} else {
		printf("revision:UNKOWN\n");
	}
	return 0;
}

U_BOOT_CMD(
	cpuinfo,	1,	1,	do_cpuinfo,
	"family_id info",
	"get family_id info"
);

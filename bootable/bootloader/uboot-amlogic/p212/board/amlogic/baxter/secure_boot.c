/*
 * secure_boot.c
 *
 * Copyright 2011-2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <asm/io.h>
#include <common.h>
#include "amzn_secure_boot.h"
#if defined(UFBL_FEATURE_UNLOCK)
#include <amzn_unlock.h>
#include <u-boot/sha256.h>
#endif

const char *amzn_target_device_name(void)
{
	return "needle";
}

#define SECURE_BOOT_STATUS_REG  (0xc8100228)
#define GPIOCLK_0_PULLUP_ENABLE_REG 	(0xc883452C)
#define GPIOCLK_0_PULLUP_REG 		(0xc88344F4)
#define GPIOCLK_0_INPUT_REG		(0xc883445C)

bool secure_boot_enabled(void)
{
	unsigned int reg_value;
	reg_value = *((volatile unsigned int *)SECURE_BOOT_STATUS_REG);
	/* 4th bit indicates secure boot status */
	return ((reg_value & (1 << 4)) ? true : false);
}

int amzn_target_device_type(void)
{
	unsigned int reg_value;
	unsigned int pullEnable;

	if (false == secure_boot_enabled())
		return AMZN_ENGINEERING_DEVICE; //taking care of proto

	/* Is anti-rollback enabled? */
	if (query_efuse_status("ARB") == 1)
		return AMZN_PRODUCTION_DEVICE;

	/*set pullup enable*/
	pullEnable = __arch_getl(GPIOCLK_0_PULLUP_ENABLE_REG);
	__arch_putl((pullEnable | (1 << 28)), GPIOCLK_0_PULLUP_ENABLE_REG);
	/*pullup*/
	reg_value = __arch_getl(GPIOCLK_0_PULLUP_REG);
	__arch_putl((reg_value | (1 << 28)), GPIOCLK_0_PULLUP_REG);
	/*read*/
	reg_value = __arch_getl(GPIOCLK_0_INPUT_REG);
	//printf("amzn_target_device_type: reg_value=0x%x\n", reg_value);
	/* Restore pullEnable */
	__arch_putl(pullEnable, GPIOCLK_0_PULLUP_ENABLE_REG);

	if (reg_value & (1 << 28))
		return AMZN_PRODUCTION_DEVICE;
	else
		return AMZN_ENGINEERING_DEVICE;
}

#if defined(UFBL_FEATURE_UNLOCK)

#define CHIPID_UPPER (6)
#define CHIPID_LOWER (7)
#define CHIPID_BUF_SIZE (16)
#define HASH_BUF_SIZE (32)

int amzn_get_unlock_code(unsigned char *code, unsigned int *len)
{
	sha256_context ctx;
	uint8_t buff[CHIPID_BUF_SIZE] = {0};
	uint8_t hash[HASH_BUF_SIZE] = {0};

	if (!code || !len || *len < (16 + 1))
		return -1;

	if (get_chip_id(&buff[0], sizeof(buff)))
		return -1;
	/**
	* To sync with Amazon serial number from kernel's /proc/cpuinfo,
	* the unlock_code is low 64 bit of sha256(SoC Chipid 128bits).
	*/
	sha256_starts(&ctx);
	sha256_update(&ctx, &buff[0], sizeof(buff));
	sha256_finish(&ctx, &hash[0]);
	u32 *hashcode = (u32 *) &hash[0];
	snprintf(code, CHIPID_BUF_SIZE, "%08x%08x",be32_to_cpu(hashcode[CHIPID_UPPER]),
			be32_to_cpu(hashcode[CHIPID_LOWER]));

	*len = 16;
	return 0;
}

const unsigned char *amzn_get_unlock_key(unsigned int *key_len)
{
	//unlock_needle.pub
	static const unsigned char unlock_key[] =
	"\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01"
	"\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01"
	"\x00\xbb\x4d\xf1\xab\x6f\x85\x07\x07\xd5\x08\x93\x84\x44\xd6\xf8"
	"\x4e\x4a\xad\x0b\x2a\xc0\xe3\x3a\x27\x30\x32\x7c\x5b\xef\xa3\x7d"
	"\x76\x3f\x26\x17\x01\xf3\x05\x81\x4b\xb4\xfd\xcb\x48\xe3\x85\x1a"
	"\xfd\xf2\xf4\x25\xb0\x7e\x30\x50\x63\x58\x52\xdf\x6d\x6e\x28\xf8"
	"\x8a\xb8\xd1\x78\x21\x3c\x22\x19\x34\x44\x50\x7e\xf6\x05\x2b\xb5"
	"\xcf\xdf\xc9\xc2\x99\xb0\x6e\x5d\x09\x07\xe1\x68\x5a\x60\x0b\x58"
	"\x95\xab\xb2\xb1\x32\x23\xd7\x0c\xd5\x85\x4c\xcb\x75\xd0\xca\xdf"
	"\x3c\xc9\xf7\x33\x40\x1c\x20\xe5\x3e\x72\xda\x69\x24\x80\x29\xd2"
	"\x33\xfc\x7d\x03\x83\xb6\xca\x53\xf7\x3a\xab\x0d\x1e\xe4\x6b\x8c"
	"\x53\x95\x27\x1e\x59\x12\xf2\xfc\x2c\x70\x73\xb1\x22\x86\xf6\x1f"
	"\x81\x57\x27\x12\x6e\x2d\xb6\xd9\x80\xe3\xea\x18\x42\x48\x23\x67"
	"\x88\xf8\x9a\xba\x3e\x0a\xd2\x49\xc8\x99\x98\xde\xa2\x03\x33\xe4"
	"\xc8\xd5\xad\x29\x84\x21\x28\xf7\xc1\xd4\x20\x6f\xea\xbc\x19\x11"
	"\x8e\x20\xc2\x30\xee\x42\xc1\x71\x55\x02\x3e\xbc\xa7\x48\x08\x84"
	"\x18\x76\x42\x01\x7b\x91\xcc\x0e\xa7\x91\xa7\xb5\xfd\xaa\xe1\xb8"
	"\x3b\xb7\x72\xe5\x1d\xe7\x6a\x00\xab\x0b\x2c\xdf\xb0\x07\x71\x16"
	"\x37\x02\x03\x01\x00\x01"
	;

	const int unlock_key_size = sizeof(unlock_key);
	if (!key_len)
		return NULL;

	*key_len = unlock_key_size;

	return unlock_key;
}
#endif

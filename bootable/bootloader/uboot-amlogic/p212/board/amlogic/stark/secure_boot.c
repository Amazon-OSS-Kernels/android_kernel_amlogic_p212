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
	return "stark";
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
	//stark_unlock.pub
	static const unsigned char unlock_key[] =
	"\x30\x82\x01\x22\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01"
	"\x01\x05\x00\x03\x82\x01\x0F\x00\x30\x82\x01\x0A\x02\x82\x01\x01"
	"\x00\xAC\x15\x8C\x3F\xC1\xA8\xDC\xC0\x7A\x45\xB9\xAC\xC3\x7E\xDC"
	"\x1D\x3F\x27\xD4\x71\x44\xAB\x9B\x1F\xF1\x3F\x20\x88\xAB\xD0\xC6"
	"\x38\x75\x82\x22\x85\x43\x8F\xAC\x8D\xB0\x0D\x7F\x62\xB1\xE0\xBF"
	"\xBC\x8D\x9F\x2C\x02\x9A\x6B\x66\xA0\x54\xAE\x66\x0C\x3D\x3D\x15"
	"\x42\x9C\x51\x21\xC7\x6A\xDE\xB2\xBF\x93\x12\xCE\x6F\xD2\x50\x3E"
	"\xD7\x56\x44\xAF\x4A\xDA\x52\xE3\x76\xC8\x17\x36\x33\xA9\x03\x81"
	"\x6F\x20\x20\xCE\x0C\xD1\xC3\x01\x73\xDD\x43\x4C\xC2\x0F\x9C\x56"
	"\xE4\x83\xAA\x40\x12\x09\x12\x1B\x05\xD9\x57\xB8\x2C\x2A\xB3\x3F"
	"\xFC\x0C\xDB\x0F\x66\xAC\x64\xE0\xF8\x28\x04\x95\x74\x57\xBB\xAA"
	"\xB7\xF3\x5F\x4C\x09\x21\xE9\x18\x3A\xFA\x74\x3F\x75\xA8\xEE\x98"
	"\x5E\xA1\x1C\x62\xC2\x34\xFB\x63\xBC\xA4\x7A\x4B\x70\x41\xF4\xF6"
	"\x65\x2E\x93\x84\x22\xB3\x5D\x58\x57\x20\x31\xFF\xAA\x68\x24\x6A"
	"\x42\xF1\x9D\x60\x8F\xBB\x9C\x5D\xAF\x4C\xB9\x12\xDB\x5E\x1F\xC3"
	"\x51\xA0\x00\xA0\xED\xB6\x60\x74\x29\xEE\x48\xE7\xEC\x9E\xB4\x55"
	"\xBB\x34\xA5\xCC\xE3\xBB\xE2\xB1\xBC\x88\x4C\x7B\x2C\x12\xEC\x4F"
	"\xB2\x4B\x34\xE0\x35\x3E\x7C\x82\xC5\xD3\xF9\xCB\x83\xF3\x31\x89"
	"\xD9\x02\x03\x01\x00\x01"
	;

	const int unlock_key_size = sizeof(unlock_key);
	if (!key_len)
		return NULL;

	*key_len = unlock_key_size;

	return unlock_key;
}
#endif

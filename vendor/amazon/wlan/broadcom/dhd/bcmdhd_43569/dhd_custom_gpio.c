/*
 * Customer code to add GPIO control during WLAN start/stop
 *
 * Copyright (C) 1999-2016, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: dhd_custom_gpio.c 586390 2015-09-15 10:34:19Z $
 */

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmutils.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_linux.h>

#include <wlioctl.h>
#if defined(WL_WIRELESS_EXT)
#include <wl_iw.h>
#endif

#define WL_ERROR(x) printf x
#define WL_TRACE(x)


#if defined(OOB_INTR_ONLY) || defined(BCMSPI_ANDROID)

#if defined(BCMLXSDMMC)
extern int sdioh_mmc_irq(int irq);
#endif /* (BCMLXSDMMC)  */

#if defined(PLATFORM_MPS)
#include <mach/gpio.h>
#endif

/* Customer specific Host GPIO defintion  */
static int dhd_oob_gpio_num = -1;

module_param(dhd_oob_gpio_num, int, 0644);
MODULE_PARM_DESC(dhd_oob_gpio_num, "DHD oob gpio number");

/* This function will return:
 *  1) return :  Host gpio interrupt number per customer platform
 *  2) irq_flags_ptr : Type of Host interrupt as Level or Edge
 *
 *  NOTE :
 *  Customer should check his platform definitions
 *  and his Host Interrupt spec
 *  to figure out the proper setting for his platform.
 *  Broadcom provides just reference settings as example.
 *
 */
int dhd_customer_oob_irq_map(void *adapter, unsigned long *irq_flags_ptr)
{
	int  host_oob_irq = 0;

#if defined(CUSTOM_OOB_GPIO_NUM)
	if (dhd_oob_gpio_num < 0) {
		dhd_oob_gpio_num = CUSTOM_OOB_GPIO_NUM;
	}
#endif /* CUSTOMER_OOB_GPIO_NUM */

	if (dhd_oob_gpio_num < 0) {
		WL_ERROR(("%s: ERROR customer specific Host GPIO is NOT defined \n",
		__FUNCTION__));
		return (dhd_oob_gpio_num);
	}

	WL_ERROR(("%s: customer specific Host GPIO number is (%d)\n",
	         __FUNCTION__, dhd_oob_gpio_num));

#if defined(PLATFORM_MPS)
	gpio_request(dhd_oob_gpio_num, "oob irq");
	host_oob_irq = gpio_to_irq(dhd_oob_gpio_num);
	gpio_direction_input(dhd_oob_gpio_num);
#endif 

	return (host_oob_irq);
}
#endif /* defined(OOB_INTR_ONLY) || defined(BCMSPI_ANDROID) */

/* Customer function to control hw specific wlan gpios */
int
dhd_customer_gpio_wlan_ctrl(void *adapter, int onoff)
{
	int err = 0;

	return err;
}

#ifdef GET_CUSTOM_MAC_ENABLE
/* Function to get custom MAC address */
int
dhd_custom_get_mac_address(void *adapter, unsigned char *buf)
{
	int ret = 0;

	WL_TRACE(("%s Enter\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;

	/* Customer access to MAC address stored outside of DHD driver */
#if (0 || 0) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	ret = wifi_platform_get_mac_addr(adapter, buf);
#endif

#ifdef EXAMPLE_GET_MAC
	/* EXAMPLE code */
	{
		struct ether_addr ea_example = {{0x00, 0x11, 0x22, 0x33, 0x44, 0xFF}};
		bcopy((char *)&ea_example, buf, sizeof(struct ether_addr));
	}
#endif /* EXAMPLE_GET_MAC */

	return ret;
}
#endif /* GET_CUSTOM_MAC_ENABLE */

#if !defined(WL_WIRELESS_EXT)
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];	/* ISO 3166-1 country abbreviation */
	char custom_locale[WLC_CNTRY_BUF_SZ];	/* Custom firmware locale */
	int32 custom_locale_rev;		/* Custom local revisin default -1 */
};
#endif /* WL_WIRELESS_EXT */

/* Customized Locale table : OPTIONAL feature */
/* Table should be filled out based on custom platform regulatory requirement */
#ifdef NEEDLE_SUPPORT_COUNTRY_CODE
#define DEFAULT_WWS_CCODE	"XZ"
#define DEFAULT_WWS_REV		981
const struct cntry_locales_custom translate_custom_table[] = {
	{"",   "XZ", 981}, /* Universal if Country code is unknown or empty */
	{"US", "US", 886},
	{"US1", "US", 844},
	{"US2", "US", 886},
	{"CA", "US", 886},
	{"CA1", "US", 886},
	{"CA2", "US", 886},
	{"JP", "JP", 942},
	{"JP1", "JP", 942},
	{"JP2", "JP", 942},
	{"AT", "AT", 996},
	{"AT1", "AT", 996},
	{"AT2", "AT", 996},
	{"IN", "IN", 986},
	{"IN1", "IN", 986},
	{"IN2", "IN", 986},
	{"EU", "DE", 990},
	{"EU1", "DE", 967},
	{"EU2", "DE", 990},
	{"BE", "DE", 990},
	{"BE1", "DE", 967},
	{"BE2", "DE", 990},
	{"BG", "DE", 990},
	{"BG1", "DE", 967},
	{"BG2", "DE", 990},
	{"CY", "DE", 990},
	{"CY1", "DE", 967},
	{"CY2", "DE", 990},
	{"CZ", "DE", 990},
	{"CZ1", "DE", 967},
	{"CZ2", "DE", 990},
	{"DK", "DE", 990},
	{"DK1", "DE", 967},
	{"DK2", "DE", 990},
	{"EE", "DE", 990},
	{"EE1", "DE", 967},
	{"EE2", "DE", 990},
	{"FI", "DE", 990},
	{"FI1", "DE", 967},
	{"FI2", "DE", 990},
	{"FR", "DE", 967},
	{"FR1", "DE", 967},
	{"FR2", "DE", 967},
	{"DE", "DE", 990},
	{"DE1", "DE", 967},
	{"DE2", "DE", 990},
	{"GR", "DE", 990},
	{"GR1", "DE", 967},
	{"GR2", "DE", 990},
	{"HU", "DE", 990},
	{"HU1", "DE", 967},
	{"HU2", "DE", 990},
	{"IE", "DE", 990},
	{"IE1", "DE", 967},
	{"IE2", "DE", 990},
	{"IT", "DE", 990},
	{"IT1", "DE", 967},
	{"IT2", "DE", 990},
	{"LV", "DE", 990},
	{"LV1", "DE", 967},
	{"LV2", "DE", 990},
	{"LI", "DE", 990},
	{"LI1", "DE", 967},
	{"LI2", "DE", 990},
	{"LT", "DE", 990},
	{"LT1", "DE", 967},
	{"LT2", "DE", 990},
	{"LU", "DE", 990},
	{"LU1", "DE", 967},
	{"LU2", "DE", 990},
	{"MT", "DE", 990},
	{"MT1", "DE", 967},
	{"MT2", "DE", 990},
	{"NL", "DE", 990},
	{"NL1", "DE", 967},
	{"NL2", "DE", 990},
	{"PL", "DE", 990},
	{"PL1", "DE", 967},
	{"PL2", "DE", 990},
	{"PT", "DE", 990},
	{"PT1", "DE", 967},
	{"PT2", "DE", 990},
	{"RO", "DE", 990},
	{"RO1", "DE", 967},
	{"RO2", "DE", 990},
	{"SK", "DE", 990},
	{"SK1", "DE", 967},
	{"SK2", "DE", 990},
	{"SI", "DE", 990},
	{"SI1", "DE", 967},
	{"SI2", "DE", 990},
	{"ES", "DE", 990},
	{"ES1", "DE", 967},
	{"ES2", "DE", 990},
	{"SE", "DE", 990},
	{"SE1", "DE", 967},
	{"SE2", "DE", 990},
	{"GB", "GB", 897},
	{"GB1", "GB", 891},
	{"GB2", "GB", 897},
	{"WS", "XZ", 982},
	{"WS1", "XZ", 981},
	{"WS2", "XZ", 982} /*Pass WS if no support for country is not present*/
};
#elif defined STARK_SUPPORT_COUNTRY_CODE
#define DEFAULT_WWS_CCODE	"XZ"
#define DEFAULT_WWS_REV		983
const struct cntry_locales_custom translate_custom_table[] = {
	{"",   "XZ", 983}, /* Universal if Country code is unknown or empty */
	{"US", "Q2", 895},
	{"CA", "Q2", 895},
	{"JP", "JP", 941},
	{"AT", "AT", 997},
	{"IN", "IN", 986},
	{"EU", "DE", 971},
	{"BE", "DE", 971},
	{"BG", "DE", 971},
	{"CY", "DE", 971},
	{"CZ", "DE", 971},
	{"DK", "DE", 971},
	{"EE", "DE", 971},
	{"FI", "DE", 971},
	{"FR", "DE", 971},
	{"DE", "DE", 971},
	{"GR", "DE", 971},
	{"HU", "DE", 971},
	{"IE", "DE", 971},
	{"IT", "DE", 971},
	{"LV", "DE", 971},
	{"LI", "DE", 971},
	{"LT", "DE", 971},
	{"LU", "DE", 971},
	{"MT", "DE", 971},
	{"NL", "DE", 971},
	{"PL", "DE", 971},
	{"PT", "DE", 971},
	{"RO", "DE", 971},
	{"SK", "DE", 971},
	{"SI", "DE", 971},
	{"ES", "DE", 971},
	{"SE", "DE", 971},
	{"GB", "GB", 894},
	{"WS", "XZ", 983} /*Pass WS if no support for country is not present*/
};
#else
#define DEFAULT_WWS_CCODE	"XZ"
#define DEFAULT_WWS_REV		991
const struct cntry_locales_custom translate_custom_table[] = {
	{"",   "XZ", 991}, /* Universal if Country code is unknown or empty */
	{"US", "US", 936},
	{"CA", "US", 936},
	{"JP", "JP", 966},
	{"AT", "AT", 999},
	{"IN", "IN", 996},
	{"EU", "DE", 990},
	{"BE", "DE", 990},
	{"BG", "DE", 990},
	{"CY", "DE", 990},
	{"CZ", "DE", 990},
	{"DK", "DE", 990},
	{"EE", "DE", 990},
	{"FI", "DE", 990},
	{"FR", "DE", 990},
	{"DE", "DE", 990},
	{"GR", "DE", 990},
	{"HU", "DE", 990},
	{"IE", "DE", 990},
	{"IT", "DE", 990},
	{"LV", "DE", 990},
	{"LI", "DE", 990},
	{"LT", "DE", 990},
	{"LU", "DE", 990},
	{"MT", "DE", 990},
	{"NL", "DE", 990},
	{"PL", "DE", 990},
	{"PT", "DE", 990},
	{"RO", "DE", 990},
	{"SK", "DE", 990},
	{"SI", "DE", 990},
	{"ES", "DE", 990},
	{"SE", "DE", 990},
	{"GB", "GB", 997},
	{"WS", "XZ", 991} /*Pass WS if no support for country is not present*/
};
#endif

/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
#ifdef CUSTOM_COUNTRY_CODE
void get_customized_country_code(void *adapter, char *country_iso_code,
  wl_country_t *cspec, u32 flags)
#else
void get_customized_country_code(void *adapter, char *country_iso_code, wl_country_t *cspec)
#endif /* CUSTOM_COUNTRY_CODE */
{
#if defined(OEM_ANDROID)
#if 0 && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))

	struct cntry_locales_custom *cloc_ptr;

	if (!cspec)
		return;
#ifdef CUSTOM_COUNTRY_CODE
	cloc_ptr = wifi_platform_get_country_code(adapter, country_iso_code,
	           flags);
#else
	cloc_ptr = wifi_platform_get_country_code(adapter, country_iso_code);
#endif /* CUSTOM_COUNTRY_CODE */
	if (cloc_ptr) {
		strlcpy(cspec->ccode, cloc_ptr->custom_locale, WLC_CNTRY_BUF_SZ);
		cspec->rev = cloc_ptr->custom_locale_rev;
	}
	return;
#else
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
			memcpy(cspec->ccode,
				translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
			cspec->rev = translate_custom_table[i].custom_locale_rev;
			return;
		}
	}

	/* if no country code matched return first universal code from translate_custom_table */
	memcpy(cspec->ccode, translate_custom_table[0].custom_locale, WLC_CNTRY_BUF_SZ);
	cspec->rev = translate_custom_table[0].custom_locale_rev;
	return;
#endif
#endif /* OEM_ANDROID */
}

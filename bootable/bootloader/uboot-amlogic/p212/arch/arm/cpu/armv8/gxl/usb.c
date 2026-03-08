
/*
 * arch/arm/cpu/armv8/gxl/usb.c
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

//#include <common.h>
//#include <asm/cache.h>
#include <asm/arch/usb.h>
#include <asm/arch/romboot.h>

#define   P_AO_RTC_ALT_CLK_CNTL0                               (volatile uint32_t *)(0xc8100000 + (0x25 << 2))
#define   P_AO_RTI_PWR_CNTL_REG0                               (volatile uint32_t *)(0xc8100000 + (0x04 << 2))

void set_usb_phy_config_device(struct amlogic_usb_config * cfg)
{
	const int time_dly = 500;
	struct u2p_aml_regs  * u2p_aml_regs = (struct u2p_aml_regs * )((ulong)cfg->usb_phy2_base_addr+PHY_REGISTER_SIZE);
	struct usb_aml_regs * usb_aml_regs = (struct usb_aml_regs * )(ulong)cfg->usb_phy3_base_addr;

	union u2p_r0_t u2p_r0;
	union usb_r0_t usb_r0;
	union usb_r4_t usb_r4;

	writel((1 << 2),P_RESET1_REGISTER);

	*P_AO_RTC_ALT_CLK_CNTL0 |= (1<<31)|(1<<30);
	*P_AO_RTI_PWR_CNTL_REG0 |= (4<<10);

	u2p_r0.d32 = u2p_aml_regs->u2p_r0;
	u2p_r0.b.fsel = 2;

	u2p_r0.b.por = 1;
	u2p_r0.b.dppulldown = 0;
	u2p_r0.b.dmpulldown = 0;
	u2p_aml_regs->u2p_r0 = u2p_r0.d32;

	u2p_r0.d32 = u2p_aml_regs->u2p_r0;
	u2p_r0.b.por = 0;
	u2p_aml_regs->u2p_r0 = u2p_r0.d32;

	usb_r0.d32 = usb_aml_regs->usb_r0;
	usb_r0.b.u2d_act = 1;
	usb_aml_regs->usb_r0 = usb_r0.d32;

	usb_r4.d32 = usb_aml_regs->usb_r4;
	usb_r4.b.p21_SLEEPM0 = 1;
	usb_aml_regs->usb_r4 = usb_r4.d32;

	udelay(time_dly);
	return;
}

void amlogic_usb2_phy_init(struct u2p_aml_regs *phy, int port_number)
{
	int time_dly = 500;
	int i;
	struct u2p_aml_regs *u2p_aml_reg;
	union u2p_r0_t reg0;

	for (i=0; i<port_number; i++) {
		u2p_aml_reg = (struct u2p_aml_regs *)((ulong)phy + i * PHY_REGISTER_SIZE);

		reg0.d32 = u2p_aml_reg->u2p_r0;
		reg0.b.por = 1;
		reg0.b.dmpulldown = 1;
		reg0.b.dppulldown = 1;

		u2p_aml_reg->u2p_r0 = reg0.d32;

		udelay(time_dly);

		reg0.d32 = u2p_aml_reg->u2p_r0;
		reg0.b.por = 0;
		u2p_aml_reg->u2p_r0 = reg0.d32;
	}

	return;
}

void amlogic_usb3_phy_init(struct usb_aml_regs *phy, int port_number)
{
	union usb_r1_t r1 = {.d32 = 0};
	struct usb_aml_regs *usb_aml_reg;
	int i;

	if (port_number == 0) {
		for (i = 0; i < 1; i++) {
			usb_aml_reg = (struct usb_aml_regs *)((ulong)phy+i*PHY_REGISTER_SIZE);

			r1.d32 = usb_aml_reg->usb_r1;
			r1.b.u3h_fladj_30mhz_reg = 0x20;
			usb_aml_reg->usb_r1 = r1.d32;
		}
	}

	return;
}

void amlogic_usb2_phy_exit(struct u2p_aml_regs *phy, int port_number)
{
	return;
}

void amlogic_usb3_phy_exit(struct usb_aml_regs *phy, int port_number)
{
	return;
}

const char * bc_name[]={
	"UNKNOWN (Disconnect)",
	"SDP (PC)",
	"DCP (Charger)",
	"CDP (PC with Charger)",
};
const char * idpin_name[]={
	"Host mode",
	"Device mode",
};
#define T_DCD_TIMEOUT	10
#define T_VDPSRC_ON		40
#define T_VDMSRC_EN	(20 + 5)
#define T_VDMSRC_DIS	(20 + 5)
#define T_VDMSRC_ON	40
static void usb_bc_detect(struct amlogic_usb_config * usb_cfg)
{
	int timeout_det;
	struct u2p_aml_regs *u2p_aml_reg;
	struct usb_aml_regs *usb_aml_reg;
	int bc_mode = BC_MODE_UNKNOWN;
	union u2p_r0_t reg0;
	union u2p_r1_t reg1;
	union u2p_r2_t reg2;
	union usb_r5_t r5;
	int devicemode;

	u2p_aml_reg = (struct u2p_aml_regs *)((ulong)usb_cfg->usb_phy2_base_addr+PHY_REGISTER_SIZE);
	usb_aml_reg = (struct usb_aml_regs *)((ulong)usb_cfg->usb_phy3_base_addr);
	/*detect ID pin*/
	reg0.d32 = u2p_aml_reg->u2p_r0;
	r5.d32 = usb_aml_reg->usb_r5;
	reg0.b.idpullup = 1;
	r5.b.iddig_en0 = 1;
	r5.b.iddig_en1 = 1;
	u2p_aml_reg->u2p_r0 = reg0.d32;
	usb_aml_reg->usb_r5 = r5.d32;
	mdelay(1);
	r5.d32 = usb_aml_reg->usb_r5;
	devicemode = r5.b.iddig_curr;
	
	/*detect BC mode*/
	reg2.d32 = u2p_aml_reg->u2p_r2;
	reg1.d32 = u2p_aml_reg->u2p_r1;

	if ( reg2.b.device_sess_vld ) {
		/* Turn on VDPSRC */
		reg1.b.chrgsel = 0;
		reg1.b.vdatdetenb = 1;
		reg1.b.vdatsrcenb = 1;
		reg1.b.dcd_enable = 0;

		u2p_aml_reg->u2p_r1 = reg1.d32;

		/* SDP and CDP/DCP distinguish */
		timeout_det = T_VDMSRC_EN;
		while ( timeout_det-- ) {
			reg2.d32 = u2p_aml_reg->u2p_r2;
			if ( reg2.b.chg_det )
				break;
			mdelay(1);
		};

		if ( reg2.b.chg_det ) {
			mdelay(T_VDPSRC_ON);
			/* Turn off VDPSRC */
			reg1.d32 = u2p_aml_reg->u2p_r1;
			reg1.b.vdatdetenb = 0;
			reg1.b.vdatsrcenb = 0;
			u2p_aml_reg->u2p_r1 = reg1.d32;

			/* Wait VDMSRC_DIS */
			timeout_det = T_VDMSRC_DIS;
			while ( timeout_det-- ) {
				reg2.d32 = u2p_aml_reg->u2p_r2;
				if (!reg2.b.chg_det)
					break;
				mdelay(1);
			};

			if ( timeout_det <= 0 )
				printf("Time out for VDMSRC_DIS!");

			/* Turn on VDMSRC */
			reg1.d32 = u2p_aml_reg->u2p_r1;
			reg1.b.chrgsel = 1;
			reg1.b.vdatdetenb = 1;
			reg1.b.vdatsrcenb = 1;
			u2p_aml_reg->u2p_r1 = reg1.d32;

			mdelay(T_VDMSRC_ON);

			reg2.d32 = u2p_aml_reg->u2p_r2;
			if ( reg2.b.chg_det )
				bc_mode = BC_MODE_DCP;
			else
				bc_mode = BC_MODE_CDP;
		} else {
			bc_mode = BC_MODE_SDP;
		}
		reg1.d32 = u2p_aml_reg->u2p_r1;
		reg1.b.vdatdetenb = 0;
		reg1.b.vdatsrcenb = 0;
		reg1.b.dcd_enable = 0;
		u2p_aml_reg->u2p_r1 = reg1.d32;
	}

	if ((devicemode == 1) && (bc_mode == BC_MODE_UNKNOWN))
		devicemode = 0;
	printf("detect usb battery charger mode: %s\n",bc_name[bc_mode]);
	printf("detect ID pin : %s\n",idpin_name[devicemode]);
	bc_mode |= (devicemode<<7);
	usb_cfg->battery_charging_det_cb(bc_mode);

}
static struct amlogic_usb_config * g_usb_cfg[BOARD_USB_MODE_MAX][USB_PHY_PORT_MAX];

struct amlogic_usb_config * board_usb_start(int mode,int index)
{
	if (mode < 0 || mode >= BOARD_USB_MODE_MAX||!g_usb_cfg[mode][index]) {
		printf("There is no this type usb port seting,Please check HW and SW!\n");
		return 0;
	}
	printf("USB3.0 XHCI init start\n");
	writel((1 << 2),P_RESET1_REGISTER);
	if (mode == BOARD_USB_MODE_CHARGER) {
		if (g_usb_cfg[mode][index]->set_vbus_power) {
			g_usb_cfg[mode][index]->set_vbus_power(0);
			mdelay(10);
		}
		if (g_usb_cfg[mode][index]->battery_charging_det_cb) {
			set_usb_phy_config_device(g_usb_cfg[mode][index]);
			usb_bc_detect(g_usb_cfg[mode][index]);
		} else {
			printf("There is no charge usb port seting,Please check HW and SW!\n");
		}
	} else if(mode == BOARD_USB_MODE_HOST ) {
		if (g_usb_cfg[mode][index]->set_vbus_power) {
			g_usb_cfg[mode][index]->set_vbus_power(1);
		}
	}

	return g_usb_cfg[mode][index];
}

int board_usb_stop(int mode,int index)
{
	printf("board_usb_stop cfg: %d\n",mode);

	return 0;
}

int usb_index = 0;
void board_usb_init(struct amlogic_usb_config * usb_cfg,int mode)
{
	if (mode < 0 || mode >= BOARD_USB_MODE_MAX || !usb_cfg)
		return ;

	if (mode == BOARD_USB_MODE_HOST) {
		if (usb_index >= USB_PHY_PORT_MAX)
			return;
		g_usb_cfg[mode][usb_index] = usb_cfg;
		usb_index++;
	} else
		g_usb_cfg[mode][0] = usb_cfg;
	printf("register usb cfg[%d][%d] = %p\n",mode,(mode==BOARD_USB_MODE_HOST)?usb_index:0,usb_cfg);
}

int get_usb_count(void)
{
    return  usb_index;
}

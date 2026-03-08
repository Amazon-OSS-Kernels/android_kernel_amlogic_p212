
/*
 * arch/arm/cpu/armv8/txl/usb.c
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

void amlogic_usb2_phy_init(struct u2p_aml_regs *phy, int port_number)
{
	int time_dly = 500;
	int i;
	struct u2p_aml_regs *u2p_aml_reg;
	union u2p_r0_t reg0;

	*P_RESET1_REGISTER = (1<<2);

	udelay(time_dly);

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

static struct amlogic_usb_config * g_usb_cfg[BOARD_USB_MODE_MAX][USB_PHY_PORT_MAX];

struct amlogic_usb_config * board_usb_start(int mode,int index)
{
	printf("USB3.0 XHCI init start\n");

	if (mode < 0 || mode >= BOARD_USB_MODE_MAX||!g_usb_cfg[mode][index])
		return 0;

	writel((1 << 2),P_RESET1_REGISTER);

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

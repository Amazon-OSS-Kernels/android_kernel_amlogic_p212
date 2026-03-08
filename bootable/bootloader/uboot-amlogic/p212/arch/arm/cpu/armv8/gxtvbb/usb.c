
/*
 * arch/arm/cpu/armv8/gxtvbb/usb.c
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

struct usb_aml_regs *usb_aml_reg;
void amlogic_usb2_phy_init(struct u2p_aml_regs *phy, int port_number)
{
	int time_dly = 500;
	int i;
	struct u2p_aml_regs *u2p_aml_reg;
	union u2p_r0_t reg0;


	for (i=0; i<4; i++)
	{
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

void cr_bus_addr (u32 addr)
{
	union usb_r2_t usb_r2 = {.d32 = 0};
	union usb_r6_t usb_r6 = {.d32 = 0};

	// prepare addr
	usb_r2.b.p30_cr_data_in = addr;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// cap addr rising edge
	usb_r2.b.p30_cr_cap_addr = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;
	usb_r2.b.p30_cr_cap_addr = 1;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 1
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 0);

	// clear cap addr
	usb_r2.b.p30_cr_cap_addr = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 0
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 1);
}

int cr_bus_read (u32 addr)
{
	int data;
	union usb_r2_t usb_r2 = {.d32 = 0};
	union usb_r6_t usb_r6 = {.d32 = 0};

	cr_bus_addr ( addr );

	// read rising edge
	usb_r2.b.p30_cr_read = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;
	usb_r2.b.p30_cr_read = 1;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 1
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 0);

	// save data
	data = usb_r6.b.p30_cr_data_out;

	// clear read
	usb_r2.b.p30_cr_read = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 0
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 1);

	return data;
}

void cr_bus_write (u32 addr, u32 data)
{
	union usb_r2_t usb_r2 = {.d32 = 0};
	union usb_r6_t usb_r6 = {.d32 = 0};

	cr_bus_addr ( addr );

	// prepare data
	usb_r2.b.p30_cr_data_in = data;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// cap data rising edge
	usb_r2.b.p30_cr_cap_data = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;
	usb_r2.b.p30_cr_cap_data = 1;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 1
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 0);

	// clear cap data
	usb_r2.b.p30_cr_cap_data = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 0
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 1);

	// write rising edge
	usb_r2.b.p30_cr_write = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;
	usb_r2.b.p30_cr_write = 1;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 1
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 0);

	// clear write
	usb_r2.b.p30_cr_write = 0;
	usb_aml_reg->usb_r2 = usb_r2.d32;

	// wait ack 0
	do {
		usb_r6.d32 = usb_aml_reg->usb_r6;
	} while (usb_r6.b.p30_cr_ack == 1);
}


void amlogic_usb3_phy_init(struct usb_aml_regs *phy, int port_number)
{
	union usb_r0_t r0 = {.d32 = 0};
	union usb_r1_t r1 = {.d32 = 0};
	union usb_r2_t r2 = {.d32 = 0};
	union usb_r3_t r3 = {.d32 = 0};
	int i;
	u32 data = 0;

	for (i=0;i<1;i++)
	{
		usb_aml_reg = (struct usb_aml_regs *)((ulong)phy+i*PHY_REGISTER_SIZE);

		usb_aml_reg->usb_r3 = (1<<13) | (0x68<<24);
		udelay(2);
		r0.d32 = usb_aml_reg->usb_r0;
		r0.b.p30_phy_reset = 1;
		usb_aml_reg->usb_r0 = r0.d32;
		udelay(2);
		r0.b.p30_phy_reset = 0;
		r0.b.p30_tx_vboost_lvl = 0x4;
		usb_aml_reg->usb_r0 = r0.d32;


		/*
		* WORKAROUND: There is SSPHY suspend bug due to which USB enumerates
		* in HS mode instead of SS mode. Workaround it by asserting
		* LANE0.TX_ALT_BLOCK.EN_ALT_BUS to enable TX to use alt bus mode
		*/
		data = cr_bus_read(0x102d);
		data |= (1 << 7);
		cr_bus_write(0x102D, data);

		data = cr_bus_read(0x1010);
		data &= ~0xff0;
		data |= 0x20;
		cr_bus_write(0x1010, data);

		/*
		* Fix RX Equalization setting as follows
		* LANE0.RX_OVRD_IN_HI. RX_EQ_EN set to 0
		* LANE0.RX_OVRD_IN_HI.RX_EQ_EN_OVRD set to 1
		* LANE0.RX_OVRD_IN_HI.RX_EQ set to 3
		* LANE0.RX_OVRD_IN_HI.RX_EQ_OVRD set to 1
		*/
		data = cr_bus_read(0x1006);
		data &= ~(1 << 6);
		data |= (1 << 7);
		data &= ~(0x7 << 8);
		data |= (0x3 << 8);
		data |= (0x1 << 11);
		cr_bus_write(0x1006, data);

		/*
		* Set EQ and TX launch amplitudes as follows
		* LANE0.TX_OVRD_DRV_LO.PREEMPH set to 22
		* LANE0.TX_OVRD_DRV_LO.AMPLITUDE set to 127
		 * LANE0.TX_OVRD_DRV_LO.EN set to 1.
		*/
		data = cr_bus_read(0x1002);
		data &= ~0x3f80;
		data |= (0x16 << 7);
		data &= ~0x7f;
		data |= (0x7f | (1 << 14));
		cr_bus_write(0x1002, data);

		/*
		* TX_FULL_SWING  to 127
		*/
		data = cr_bus_read(0x30);
		data &= ~(0xf << 4);
		cr_bus_write(0x30, data);

		/*
		* TX_FULL_SWING  to 127
		*/
		r1.d32 = usb_aml_reg->usb_r1;
		r1.b.p30_pcs_tx_swing_full = 127;
		r1.b.u3h_fladj_30mhz_reg = 0x20;
		usb_aml_reg->usb_r1 = r1.d32;
		udelay(2);

		/*
		* TX_DEEMPH_3_5DB  to 22
		*/
		r2.d32 = usb_aml_reg->usb_r2;
		r2.b.p30_pcs_tx_deemph_3p5db = 22;
		usb_aml_reg->usb_r2 = r2.d32;

		udelay(2);
		/*
		* LOS_BIAS	to 0x5
		* LOS_LEVEL to 0x9
		*/
		r3.d32 = usb_aml_reg->usb_r3;
		r3.b.p30_los_bias = 0x5;
		r3.b.p30_los_level = 0x9;
		r3.b.p30_ssc_en = 1;
		r3.b.p30_ssc_range = 2;
		usb_aml_reg->usb_r3 = r3.d32;
	}

}

void amlogic_usb2_phy_exit(struct u2p_aml_regs *phy, int port_number)
{
	return;
}

void amlogic_usb3_phy_exit(struct usb_aml_regs *phy, int port_number)
{
	return;
}

static amlogic_usb_config_t * g_usb_cfg[BOARD_USB_MODE_MAX][USB_PHY_PORT_MAX];

amlogic_usb_config_t * board_usb_start(int mode,int index)
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
void board_usb_init(amlogic_usb_config_t * usb_cfg,int mode)
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


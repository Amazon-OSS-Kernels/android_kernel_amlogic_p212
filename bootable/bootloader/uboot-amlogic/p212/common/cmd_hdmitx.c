/*
 * (C) Copyright 2012
 * Amlogic. Inc. zongdong.jiao@amlogic.com
 *
 * This file is used to prefetch/varify/compare HDCP keys
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <amlogic/hdmi.h>

static int do_hpd_detect(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	int st = hdmitx_device.HWOp.get_hpd_state();
	printf("hpd_state=%c\n", st ? '1' : '0');
	setenv("hpd_state", st ? "1": "0");

	return st;
}

static unsigned char edid_raw_buf[256] = {0};
static void dump_edid_raw_8bytes(unsigned char *buf)
{
	int i = 0;
	for (i = 0; i < 8; i++)
		printf("%02x ", buf[i]);
	printf("\n");
}

static void dump_full_edid(const unsigned char *buf)
{
	int i;
	int blk_no = buf[126] + 1;

	if (blk_no > 4)
		blk_no = 4;
	printf("Dump EDID Rawdata\n");
	for (i = 0; i < blk_no * EDID_BLK_SIZE; i++) {
		printk("%02x", buf[i]);
		if (((i+1) & 0x1f) == 0)    /* print 32bytes a line */
			printk("\n");
	}
}

static int do_edid(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	unsigned int tmp_addr = 0;
	unsigned char edid_addr = 0;
	unsigned char st = 0;

	memset(edid_raw_buf, 0, ARRAY_SIZE(edid_raw_buf));
	if (argc < 2)
		return cmd_usage(cmdtp);
	if (strcmp(argv[1], "read") == 0) {
		tmp_addr = simple_strtoul(argv[2], NULL, 16);
		if (tmp_addr > 0xff)
			return cmd_usage(cmdtp);
		edid_addr = tmp_addr;
		/* read edid raw data */
		/* current only support read 1 byte edid data */
		st = hdmitx_device.HWOp.read_edid(
			&edid_raw_buf[edid_addr & 0xf8], edid_addr & 0xf8, 8);
		printf("edid[0x%02x]: 0x%02x\n", edid_addr,
			edid_raw_buf[edid_addr]);
		if (0) /* Debug only */
			dump_edid_raw_8bytes(&edid_raw_buf[edid_addr & 0xf8]);
		if (!st)
			printf("edid read failed\n");
	}
	return st;
}

static int do_rx_det(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	unsigned char edid_addr = 0xf8;     // Fixed Address
	unsigned char st = 0;

	memset(edid_raw_buf, 0, ARRAY_SIZE(edid_raw_buf));

	// read edid raw data
	// current only support read 1 byte edid data
	st = hdmitx_device.HWOp.read_edid(&edid_raw_buf[edid_addr & 0xf8], edid_addr & 0xf8, 8);
	if (1)      // Debug only
		dump_edid_raw_8bytes(&edid_raw_buf[edid_addr & 0xf8]);
	if (st) {
#if 0
		// set fake value for debug
		edid_raw_buf[250] = 0xfb;
		edid_raw_buf[251] = 0x0c;
		edid_raw_buf[252] = 0x01;
#endif
		if ((edid_raw_buf[250] == 0xfb) & (edid_raw_buf[251] == 0x0c)) {
			printf("RX is FBC\n");

			// set outputmode ENV
			switch (edid_raw_buf[252] & 0x0f) {
			case 0x0:
				run_command("setenv outputmode 1080p50hz", 0);
				break;
			case 0x1:
				run_command("setenv outputmode 2160p50hz420", 0);
				break;
			case 0x2:
				run_command("setenv outputmode 1080p50hz44410bit", 0);
				break;
			case 0x3:
				run_command("setenv outputmode 2160p50hz42010bit", 0);
				break;
			case 0x4:
				run_command("setenv outputmode 2160p50hz42210bit", 0);
				break;
			case 0x5:
				run_command("setenv outputmode 2160p50hz", 0);
				break;
			default:
				run_command("setenv outputmode 1080p50hz", 0);
				break;
			}

			// set RX 3D Info
			switch ((edid_raw_buf[252] >> 4) & 0x0f) {
			case 0x00:
				run_command("setenv rx_3d_info 0", 0);
				break;
			case 0x01:
				run_command("setenv rx_3d_info 1", 0);
				break;
			case 0x02:
				run_command("setenv rx_3d_info 2", 0);
				break;
			case 0x03:
				run_command("setenv rx_3d_info 3", 0);
				break;
			case 0x04:
				run_command("setenv rx_3d_info 4", 0);
				break;
			default:
				break;
			}

			switch (edid_raw_buf[253]) {
			case 0x1:
				// TODO
				break;
			case 0x2:
				// TODO
				break;
			default:
				break;
			}
		}
	} else
		printf("edid read failed\n");

	return st;
}

static int do_output(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (argc < 1)
		return cmd_usage(cmdtp);

	if (strcmp(argv[1], "list") == 0)
		hdmitx_device.HWOp.list_support_modes();
	else if (strcmp(argv[1], "bist") == 0) {
		unsigned int mode;
		mode = simple_strtoul(argv[2], NULL, 16);
		hdmitx_device.HWOp.test_bist(mode);
	} else { /* "output" */
		hdmitx_device.vic = hdmi_get_fmt_vic(argv[1]);
		hdmitx_device.para = hdmi_get_fmt_paras(hdmitx_device.vic);
		if (hdmitx_device.vic == HDMI_unkown) {
			/* Not find VIC */
			printf("Not find '%s' mapped VIC\n", argv[1]);
			return CMD_RET_FAILURE;
		} else
			printf("set hdmitx VIC = %d\n", hdmitx_device.vic);
		if (getenv("colorattribute"))
			hdmi_parse_attr(hdmitx_device.para, getenv("colorattribute"));
		if (strstr(argv[1], "hz420") != NULL)
			hdmitx_device.para->cs = HDMI_COLOR_FORMAT_420;
		/* For RGB444 or YCbCr444 under 6Gbps mode, no deepcolor */
		/* Only 4k50/60 has 420 modes */
		switch (hdmitx_device.vic) {
		case HDMI_3840x2160p50_16x9:
		case HDMI_3840x2160p60_16x9:
		case HDMI_4096x2160p50_256x135:
		case HDMI_4096x2160p60_256x135:
		case HDMI_3840x2160p50_64x27:
		case HDMI_3840x2160p60_64x27:
		case HDMI_3840x2160p50_16x9_Y420:
		case HDMI_3840x2160p60_16x9_Y420:
		case HDMI_4096x2160p50_256x135_Y420:
		case HDMI_4096x2160p60_256x135_Y420:
		case HDMI_3840x2160p50_64x27_Y420:
		case HDMI_3840x2160p60_64x27_Y420:
			if ((hdmitx_device.para->cs == HDMI_COLOR_FORMAT_RGB) ||
			    (hdmitx_device.para->cs == HDMI_COLOR_FORMAT_444)) {
				if (hdmitx_device.para->cd != HDMI_COLOR_DEPTH_24B) {
					printf("vic %d cs %d has no cd %d\n",
						hdmitx_device.vic,
						hdmitx_device.para->cs,
						hdmitx_device.para->cd);
					hdmitx_device.para->cd = HDMI_COLOR_DEPTH_24B;
					printf("set cd as %d\n", HDMI_COLOR_DEPTH_24B);
				}
			}
			if (hdmitx_device.para->cs == HDMI_COLOR_FORMAT_420)
				hdmitx_device.vic |= HDMITX_VIC420_OFFSET;
			break;
		default:
			if (hdmitx_device.para->cs == HDMI_COLOR_FORMAT_420) {
				printf("vic %d has no cs %d\n", hdmitx_device.vic,
					hdmitx_device.para->cs);
				hdmitx_device.para->cs = HDMI_COLOR_FORMAT_444;
				printf("set cs as %d\n", HDMI_COLOR_FORMAT_444);
			}
			break;
		}
		hdmi_tx_set(&hdmitx_device);
	}
	return CMD_RET_SUCCESS;
}

static int do_blank(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (argc < 1)
		return cmd_usage(cmdtp);

	if (strcmp(argv[1], "1") == 0)
		hdmitx_device.HWOp.output_blank(1);
	if (strcmp(argv[1], "0") == 0)
		hdmitx_device.HWOp.output_blank(0);

	return CMD_RET_SUCCESS;
}

static int do_off(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	hdmitx_device.vic = HDMI_unkown;
	hdmitx_device.HWOp.turn_off();
	printf("turn off hdmitx\n");
	return 1;
}

static int do_dump(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	hdmitx_device.HWOp.dump_regs();
	return 1;
}

static int do_info(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	struct hdmi_format_para *para = hdev->para;

	printf("%s %d\n", para->ext_name, hdev->vic);
	printf("cd%d cs%d cr%d\n", para->cd, para->cs, para->cr);
	printf("frac_rate: %d\n", hdev->frac_rate_policy);
	return 1;
}

static int do_get_preferred_mode(cmd_tbl_t * cmdtp, int flag, int argc,
	char * const argv[])
{
	struct hdmitx_dev *hdev = &hdmitx_device;
	unsigned int byte_num = 0;
	unsigned char *edid = hdev->rawedid;
	unsigned char blk_no = 1;
	struct hdmi_format_para *para;
	char pref_mode[16];
	char color_attr[16];
	char width[16];
	char height[16];

	memset(edid, 0, EDID_BLK_SIZE * EDID_BLK_NO);
	memset(pref_mode, 0, sizeof(pref_mode));
	memset(color_attr, 0, sizeof(color_attr));
	memset(width, 0, sizeof(width));
	memset(height, 0, sizeof(height));

	/* If sink is not detected there is a still a good chance it supports proper modes */
	/* 720p is chosen as a safe compromise: supported by most sinks and looks good enough */
	if (!hdmitx_device.HWOp.get_hpd_state()) {
                para = hdmi_get_fmt_paras(HDMI_1280x720p60_16x9);
                snprintf(pref_mode, sizeof(pref_mode), "%s", para->sname);
                snprintf(color_attr, sizeof(color_attr), "%s", "rgb,8bit");
		printk("no sink, fallback to %s[%d]\n", para->sname, HDMI_1280x720p60_16x9);
		goto bypass_edid_read;
	}

	/* Read complete EDID data sequentially */
	while (byte_num < 128 * blk_no) {
		hdmitx_device.HWOp.read_edid(&edid[byte_num], byte_num & 0x7f, byte_num / 128);
		if (byte_num == 120) {
			blk_no = edid[126] + 1;
			if (blk_no > 4)
				blk_no = 4; /* MAX Read Blocks 4 */
		}
		byte_num += 8;
	}

	if (hdmi_edid_parsing(hdev->rawedid, &hdev->RXCap) == 0) {
		dump_full_edid(hdev->rawedid);
	}
	para = hdmi_get_fmt_paras(hdev->RXCap.preferred_mode);

	/* Avoid 60hz interlaced mode */
	if (para && !strcmp("1080i60hz", para->sname))
		para = hdmi_get_fmt_paras(HDMI_1280x720p60_16x9);

	/* Avoid 50hz interlaced mode */
	if (para && !strcmp("1080i50hz", para->sname))
		para = hdmi_get_fmt_paras(HDMI_1280x720p50_16x9);

	/* Avoid 4K60hz mode */
	if (para && !strcmp("2160p60hz", para->sname))
		para = hdmi_get_fmt_paras(HDMI_3840x2160p60_16x9_Y420);

	/* Avoid 4K50hz mode */
	if (para && !strcmp("2160p50hz", para->sname))
		para = hdmi_get_fmt_paras(HDMI_3840x2160p50_16x9_Y420);

	if (para) { /* preferred mode detected successfully */
		snprintf(pref_mode, sizeof(pref_mode), "%s", para->sname);
		if (hdev->RXCap.pref_colorspace & (1 << 5))
			snprintf(color_attr, sizeof(color_attr), "%s", "444,8bit");
		else if (hdev->RXCap.pref_colorspace & (1 << 4))
			snprintf(color_attr, sizeof(color_attr), "%s", "422,8bit");
		else
			snprintf(color_attr, sizeof(color_attr), "%s", "rgb,8bit");
	} else { /* fall back to 480p as the mode supported by all sinks */
		hdev->RXCap.preferred_mode = HDMI_720x480p60_16x9;
		para = hdmi_get_fmt_paras(HDMI_720x480p60_16x9);
		snprintf(pref_mode, sizeof(pref_mode), "%s", para->sname);
		snprintf(color_attr, sizeof(color_attr), "%s", "rgb,8bit");
	}
	printk("sink preferred mode is %s[%d]\n", para->sname, hdev->RXCap.preferred_mode);

bypass_edid_read:

#ifdef CONFIG_USE_NATIVE_IMG_SIZE
	/* Configure FB and display to native resolution for 1080p and below */
	if (para->timing.h_active <= 1920 && para->timing.v_active <= 1080) {
		snprintf(width,  sizeof(width), "%d", para->timing.h_active);
		snprintf(height, sizeof(height), "%d", para->timing.v_active);
		setenv("display_width" , width);
		setenv("display_height", height);
		setenv("fb_width" , width);
		setenv("fb_height", height);
	}
#endif

	setenv("hdmimode", pref_mode);
	setenv("outputmode", pref_mode);
	setenv("colorattribute", color_attr);

	return 0;
}

static cmd_tbl_t cmd_hdmi_sub[] = {
	U_BOOT_CMD_MKENT(hpd, 1, 1, do_hpd_detect, "", ""),
	U_BOOT_CMD_MKENT(edid, 3, 1, do_edid, "", ""),
	U_BOOT_CMD_MKENT(rx_det, 1, 1, do_rx_det, "", ""),
	U_BOOT_CMD_MKENT(output, 3, 1, do_output, "", ""),
	U_BOOT_CMD_MKENT(blank, 3, 1, do_blank, "", ""),
	U_BOOT_CMD_MKENT(off, 1, 1, do_off, "", ""),
	U_BOOT_CMD_MKENT(dump, 1, 1, do_dump, "", ""),
	U_BOOT_CMD_MKENT(info, 1, 1, do_info, "", ""),
	U_BOOT_CMD_MKENT(get_preferred_mode, 1, 1, do_get_preferred_mode, "", ""),
};

static int do_hdmitx(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2)
		return cmd_usage(cmdtp);

	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_hdmi_sub[0], ARRAY_SIZE(cmd_hdmi_sub));

	if (c)
		return  c->cmd(cmdtp, flag, argc, argv);
	else
		return cmd_usage(cmdtp);
}

U_BOOT_CMD(hdmitx, CONFIG_SYS_MAXARGS, 0, do_hdmitx,
	   "HDMITX sub-system",
	"hdmitx hpd\n"
	"    Detect hdmi rx plug-in\n"
	"hdmitx get_preferred_mode\n"
	"    Read full edid data, parse edid, and get preferred mode\n"
#if 0
	"hdmitx edid read ADDRESS\n"
	"    Read hdmi rx edid from ADDRESS(0x00~0xff)\n"
#endif
	"hdmitx output [list | FORMAT | bist MODE]\n"
	"    list: list support formats\n"
	"    FORMAT can be 720p60/50hz, 1080i60/50hz, 1080p60hz, etc\n"
	"       extend with 8bits/10bits, y444/y422/y420/rgb\n"
	"       such as 2160p60hz,10bits,y420\n"
	"    bist MODE: 1 Color bar  2 Line  3 Dots  0 default\n"
	"hdmitx blank [0|1]\n"
	"    1: output blank  0: output normal\n"
	"hdmitx off\n"
	"    Turn off hdmitx output\n"
	"hdmitx info\n"
	"    current mode info\n"
	"hdmitx rx_det\n"
	"    Auto detect if RX is FBC and set outputmode\n"
);

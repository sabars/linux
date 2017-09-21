/*
 * module different macro
 *
 * Copyright (C) 2008 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MODULE_DIFF_H__
#define __MODULE_DIFF_H__

#include "./../module_comm/module_comm.h"
#include "./../host_comm/owl_device.h"

#define CAMERA_MODULE_NAME		"GC0312"
#define CAMERA_MODULE_PID		0xb310
#define MODULE_I2C_REAL_ADDRESS		(0x42>>1)
#define MODULE_I2C_REG_ADDRESS		(0x42>>1)
#define I2C_REGS_WIDTH			1
#define I2C_DATA_WIDTH			1

#define PIDH				0xf0	/* Product ID Number H byte */
#define PIDL				0xf1	/* Product ID Number L byte */

#define OUTTO_SENSO_CLOCK		24000000

#define DEFAULT_VSYNC_ACTIVE_LEVEL	V4L2_MBUS_VSYNC_ACTIVE_HIGH
#define DEFAULT_HSYNC_ACTIVE_LEVEL	V4L2_MBUS_HSYNC_ACTIVE_HIGH
#define DEFAULT_PCLK_SAMPLE_EDGE        V4L2_MBUS_PCLK_SAMPLE_RISING
#define DEFAULT_POWER_LINE_FREQUENCY	V4L2_CID_POWER_LINE_FREQUENCY_50HZ

#define MODULE_DEFAULT_WIDTH	        WIDTH_VGA
#define MODULE_DEFAULT_HEIGHT	        HEIGHT_VGA
#define MODULE_MAX_WIDTH	        WIDTH_VGA
#define MODULE_MAX_HEIGHT	        HEIGHT_VGA

#define AHEAD_LINE_NUM			15
#define DROP_NUM_CAPTURE		0
#define DROP_NUM_PREVIEW		0

/*Every sensor must set this value*/
#define USE_AS_FRONT 1
#define USE_AS_REAR 0

static unsigned int frame_rate_vga[] = { 30, };

/*
SI interface params setting
*/
static struct host_module_setting_t module_setting = {
	.hs_pol = 1,		/*0: active low 1:active high */
	.vs_pol = 0,		/*0: active low 1:active high */
	.clk_edge = 0,		/*0: rasing edge 1:falling edge */
	/*0: BG/GR, U0Y0V0Y1, 1: GR/BG, V0Y0U0Y1,
	*2: GB/RG, Y0U0Y1V0, 3: RG/GB, Y0V0Y1U0 */
	.color_seq = COLOR_SEQ_UYVY,
};

struct module_info camera_module_info = {
	.flags = 0
	    | SENSOR_FLAG_10BIT
	    | SENSOR_FLAG_YUV | SENSOR_FLAG_DVP | SENSOR_FLAG_CHANNEL0,
	.module_cfg = &module_setting,
};

/*
 * supported color format list.
 * see definition in
 * http://thread.gmane.org/gmane.linux.drivers.video-input-infrastructure/
 * 12830/focus=13394
 * YUYV8_2X8_LE == YUYV with LE packing
 * YUYV8_2X8_BE == UYVY with LE packing
 * YVYU8_2X8_LE == YVYU with LE packing
 * YVYU8_2X8_BE == VYUY with LE packing
 */
static const struct module_color_format module_cfmts[] = {
	{
	 .code = V4L2_MBUS_FMT_UYVY8_2X8,
	 .colorspace = V4L2_COLORSPACE_JPEG,
	 },

};

static const struct regval_list module_init_regs[] = {
	{1, 0xfe, 0xf0},
	{1, 0xfe, 0xf0},
	{1, 0xfe, 0x00},
	{1, 0xfc, 0x0e},
	{1, 0xfc, 0x0e},
	{1, 0xf2, 0x07},
	{1, 0xf3, 0x00},	/* output_disable */
	{1, 0xf7, 0x1b},
	{1, 0xf8, 0x04},
	{1, 0xf9, 0x0e},
	{1, 0xfa, 0x11},

	/********************/
	/**** CISCTL reg******/
	/********************/
	{1, 0x00, 0x2f},
	{1, 0x01, 0x0f},	/*06 */
	{1, 0x02, 0x04},
	{1, 0x03, 0x03},
	{1, 0x04, 0x50},
	{1, 0x09, 0x00},
	{1, 0x0a, 0x00},
	{1, 0x0b, 0x00},
	{1, 0x0c, 0x04},
	{1, 0x0d, 0x01},
	{1, 0x0e, 0xe8},
	{1, 0x0f, 0x02},
	{1, 0x10, 0x88},
	{1, 0x16, 0x00},
	{1, 0x17, 0x14},
	{1, 0x18, 0x1a},
	{1, 0x19, 0x14},
	{1, 0x1b, 0x48},
	{1, 0x1e, 0x6b},
	{1, 0x1f, 0x28},
	{1, 0x20, 0x89},
	{1, 0x21, 0x49},
	{1, 0x22, 0xb0},
	{1, 0x23, 0x04},
	{1, 0x24, 0x16},
	{1, 0x34, 0x20},

	/********************/
	/******   BLK	*********/
	/********************/
	{1, 0x26, 0x23},
	{1, 0x28, 0xff},
	{1, 0x29, 0x00},
	{1, 0x33, 0x10},
	{1, 0x37, 0x20},
	{1, 0x38, 0x10},
	{1, 0x47, 0x80},
	{1, 0x4e, 0x66},
	{1, 0xa8, 0x02},
	{1, 0xa9, 0x80},

	/********************/
	/*******ISP reg ******/
	/*******************/
	{1, 0x40, 0xff},
	{1, 0x41, 0x21},
	{1, 0x42, 0xcf},
	{1, 0x44, 0x00},
	{1, 0x45, 0xa8},
	{1, 0x46, 0x02},	/*sync */
	{1, 0x4a, 0x11},
	{1, 0x4b, 0x01},
	{1, 0x4c, 0x20},
	{1, 0x4d, 0x05},
	{1, 0x4f, 0x01},
	{1, 0x50, 0x01},
	{1, 0x55, 0x01},
	{1, 0x56, 0xe0},
	{1, 0x57, 0x02},
	{1, 0x58, 0x80},

	/********************/
	/********GAIN********/
	/********************/
	{1, 0x70, 0x70},
	{1, 0x5a, 0x84},
	{1, 0x5b, 0xc9},
	{1, 0x5c, 0xed},
	{1, 0x77, 0x74},
	{1, 0x78, 0x40},
	{1, 0x79, 0x5f},

	/********************/
	/*****  DNDD*********/
	/********************/
	{1, 0x82, 0x14},
	{1, 0x83, 0x0b},
	{1, 0x89, 0xf0},

	/********************/
	/*******EEINTP ******/
	/*******************/
	{1, 0x8f, 0xaa},
	{1, 0x90, 0x8c},
	{1, 0x91, 0x90},
	{1, 0x92, 0x03},
	{1, 0x93, 0x03},
	{1, 0x94, 0x05},
	{1, 0x95, 0x65},
	{1, 0x96, 0xf0},

	/*******************/
	/******ASDE********/
	/******************/
	{1, 0xfe, 0x00},
	{1, 0x9a, 0x20},
	{1, 0x9b, 0x80},
	{1, 0x9c, 0x40},
	{1, 0x9d, 0x80},

	{1, 0xa1, 0x30},
	{1, 0xa2, 0x32},
	{1, 0xa4, 0x30},
	{1, 0xa5, 0x30},
	{1, 0xaa, 0x10},
	{1, 0xac, 0x22},

	/**********************/
	/*******GAMMA ********/
	/*********************/
	{1, 0xfe, 0x00},	/*default */
	{1, 0xbf, 0x08},
	{1, 0xc0, 0x16},
	{1, 0xc1, 0x28},
	{1, 0xc2, 0x41},
	{1, 0xc3, 0x5a},
	{1, 0xc4, 0x6c},
	{1, 0xc5, 0x7a},
	{1, 0xc6, 0x96},
	{1, 0xc7, 0xac},
	{1, 0xc8, 0xbc},
	{1, 0xc9, 0xc9},
	{1, 0xca, 0xd3},
	{1, 0xcb, 0xdd},
	{1, 0xcc, 0xe5},
	{1, 0xcd, 0xf1},
	{1, 0xce, 0xfa},
	{1, 0xcf, 0xff},
	/*******************/
	/*****   YCP ********/
	/******************/
	{1, 0xd0, 0x40},
	{1, 0xd1, 0x34},
	{1, 0xd2, 0x34},
	{1, 0xd3, 0x40},
	{1, 0xd6, 0xf2},
	{1, 0xd7, 0x1b},
	{1, 0xd8, 0x18},
	{1, 0xdd, 0x03},

	/********************/
	/******** AEC********/
	/*******************/
	{1, 0xfe, 0x01},
	{1, 0x05, 0x30},
	{1, 0x06, 0x75},
	{1, 0x07, 0x40},
	{1, 0x08, 0xb0},
	{1, 0x0a, 0xc5},
	{1, 0x0b, 0x11},
	{1, 0x0c, 0x00},
	{1, 0x12, 0x52},
	{1, 0x13, 0x38},
	{1, 0x18, 0x95},
	{1, 0x19, 0x96},
	{1, 0x1f, 0x20},
	{1, 0x20, 0xc0},	/*80*/
	{1, 0x3e, 0x40},
	{1, 0x3f, 0x57},
	{1, 0x40, 0x7d},
	{1, 0x03, 0x60},
	{1, 0x44, 0x02},

	/********************/
	/*******AWB	********/
	/******************/
	{1, 0x1c, 0x91},
	{1, 0x21, 0x15},
	{1, 0x50, 0x80},
	{1, 0x56, 0x04},
	{1, 0x59, 0x08},
	{1, 0x5b, 0x02},
	{1, 0x61, 0x8d},
	{1, 0x62, 0xa7},
	{1, 0x63, 0xd0},
	{1, 0x65, 0x06},
	{1, 0x66, 0x06},
	{1, 0x67, 0x84},
	{1, 0x69, 0x08},
	{1, 0x6a, 0x25},	/*50 */
	{1, 0x6b, 0x01},
	{1, 0x6c, 0x00},
	{1, 0x6d, 0x02},
	{1, 0x6e, 0xf0},
	{1, 0x6f, 0x80},
	{1, 0x76, 0x80},
	{1, 0x78, 0xaf},
	{1, 0x79, 0x75},
	{1, 0x7a, 0x40},
	{1, 0x7b, 0x50},
	{1, 0x7c, 0x0c},

	{1, 0xa4, 0xb9},
	{1, 0xa5, 0xa0},
	{1, 0x90, 0xc9},
	{1, 0x91, 0xbe},

	{1, 0xa6, 0xb8},
	{1, 0xa7, 0x95},
	{1, 0x92, 0xe6},
	{1, 0x93, 0xca},

	{1, 0xa9, 0xbc},
	{1, 0xaa, 0x95},
	{1, 0x95, 0x23},
	{1, 0x96, 0xe7},

	{1, 0xab, 0x9d},
	{1, 0xac, 0x80},
	{1, 0x97, 0x43},
	{1, 0x98, 0x24},

	{1, 0xae, 0xb7},
	{1, 0xaf, 0x9e},
	{1, 0x9a, 0x43},
	{1, 0x9b, 0x24},

	{1, 0xb0, 0xc8},
	{1, 0xb1, 0x97},
	{1, 0x9c, 0xc4},
	{1, 0x9d, 0x44},

	{1, 0xb3, 0xb7},
	{1, 0xb4, 0x7f},
	{1, 0x9f, 0xc7},
	{1, 0xa0, 0xc8},

	{1, 0xb5, 0x00},
	{1, 0xb6, 0x00},
	{1, 0xa1, 0x00},
	{1, 0xa2, 0x00},

	{1, 0x86, 0x60},
	{1, 0x87, 0x08},
	{1, 0x88, 0x00},
	{1, 0x89, 0x00},
	{1, 0x8b, 0xde},
	{1, 0x8c, 0x80},
	{1, 0x8d, 0x00},
	{1, 0x8e, 0x00},

	{1, 0x94, 0x55},
	{1, 0x99, 0xa6},
	{1, 0x9e, 0xaa},
	{1, 0xa3, 0x0a},
	{1, 0x8a, 0x0a},
	{1, 0xa8, 0x55},
	{1, 0xad, 0x55},
	{1, 0xb2, 0x55},
	{1, 0xb7, 0x05},
	{1, 0x8f, 0x05},

	{1, 0xb8, 0xcc},
	{1, 0xb9, 0x9a},
	/*******************/
	/******** CC	*******/
	/*****************/
	{1, 0xfe, 0x01},

	{1, 0xd0, 0x38},	/*skin red */
	{1, 0xd1, 0x00},
	{1, 0xd2, 0x02},
	{1, 0xd3, 0x04},
	{1, 0xd4, 0x38},
	{1, 0xd5, 0x12},

	{1, 0xd6, 0x30},
	{1, 0xd7, 0x00},
	{1, 0xd8, 0x0a},
	{1, 0xd9, 0x16},
	{1, 0xda, 0x39},
	{1, 0xdb, 0xf8},

	/******************/
	/******  LSC	******/
	/****************/
	{1, 0xfe, 0x01},
	{1, 0xc1, 0x3c},
	{1, 0xc2, 0x50},
	{1, 0xc3, 0x00},
	{1, 0xc4, 0x40},
	{1, 0xc5, 0x30},
	{1, 0xc6, 0x30},
	{1, 0xc7, 0x10},
	{1, 0xc8, 0x00},
	{1, 0xc9, 0x00},
	{1, 0xdc, 0x20},
	{1, 0xdd, 0x10},
	{1, 0xdf, 0x00},
	{1, 0xde, 0x00},

	/***********************/
	/******Histogram	*******/
	/*********************/
	{1, 0x01, 0x10},
	{1, 0x0b, 0x31},
	{1, 0x0e, 0x50},
	{1, 0x0f, 0x0f},
	{1, 0x10, 0x6e},
	{1, 0x12, 0xa0},
	{1, 0x15, 0x60},
	{1, 0x16, 0x60},
	{1, 0x17, 0xe0},

	/******************************/
	/******Measure Window**********/
	/*****************************/
	{1, 0xcc, 0x0c},
	{1, 0xcd, 0x10},
	{1, 0xce, 0xa0},
	{1, 0xcf, 0xe6},

	/*****************/
	/*****dark sun****/
	/***************/
	{1, 0x45, 0xf7},
	{1, 0x46, 0xff},
	{1, 0x47, 0x15},
	{1, 0x48, 0x03},
	{1, 0x4f, 0x60},

	/****banding****/
	{1, 0xfe, 0x00},
	{1, 0x05, 0x02},
	{1, 0x06, 0xd1}, /*HB*/ {1, 0x07, 0x00},
	{1, 0x08, 0x22}, /*VB*/ {1, 0xfe, 0x01},
	{1, 0x25, 0x00},	/*anti-flicker step [11:8] */
	{1, 0x26, 0x6a},	/*anti-flicker step [7:0] */

	{1, 0x27, 0x02},	/*exp level 0  20fps */
	{1, 0x28, 0x12},
	{1, 0x29, 0x03},	/*exp level 1  12.50fps */
	{1, 0x2a, 0x50},
	{1, 0x2b, 0x05},	/*7.14fps */
	{1, 0x2c, 0xcc},
	{1, 0x2d, 0x07},	/*exp level 3  5.55fps */
	{1, 0x2e, 0x74},
	{1, 0x3c, 0x20},
	{1, 0xfe, 0x00},

	/******************/
	/******  DVP	******/
	/****************/
	{1, 0xfe, 0x03},
	{1, 0x01, 0x00},
	{1, 0x02, 0x00},
	{1, 0x10, 0x00},
	{1, 0x15, 0x00},
	{1, 0xfe, 0x00},
	/****OUTPUT***/
	{1, 0xf3, 0xff},	/* output_enable */
	ENDMARKER,
};

	/* 640*480: VGA */
static const struct regval_list module_vga_regs[] = {
	{1, 0xfe, 0x00},
	{1, 0x18, 0x1a},
	{1, 0x50, 0x01},	/*crop enable */
	{1, 0x55, 0x01},	/*crop window height */
	{1, 0x56, 0xe0},
	{1, 0x57, 0x02},	/*crop window width */
	{1, 0x58, 0x80},

	{1, 0xfe, 0x01},
	{1, 0xc1, 0x3c},
	{1, 0xc2, 0x50},
	{1, 0xcc, 0x0c},
	{1, 0xcd, 0x10},
	{1, 0xce, 0xa0},
	{1, 0xcf, 0xe6},
	{1, 0xfe, 0x00},
	ENDMARKER,
};

/*
 * window size list
 */
/*S VGA */
static struct camera_module_win_size module_win_vga = {
	.name = "VGA",
	.width = WIDTH_VGA,
	.height = HEIGHT_VGA,
	.win_regs = module_vga_regs,
	.frame_rate_array = frame_rate_vga,
	.capture_only = 0,
};

static struct camera_module_win_size *module_win_list[] = {
	&module_win_vga,
};

static struct v4l2_ctl_cmd_info v4l2_ctl_array[] = {
	{
	 .id = V4L2_CID_GAIN,
	 .min = 256,
	 .max = 0XFFFF,
	 .step = 1,
	 .def = 2560,
	 },
	{
	 .id = V4L2_CID_AUTO_WHITE_BALANCE,
	 .min = 0,
	 .max = 1,
	 .step = 1,
	 .def = 1,
	 },
	{
	 .id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
	 .min = 0,
	 .max = 3,
	 .step = 1,
	 .def = 1,
	 },
	{
	 .id = V4L2_CID_SENSOR_ID,
	 .min = 0,
	 .max = 0xffffff,
	 .step = 1,
	 .def = 0,
	 },
};

static struct v4l2_ctl_cmd_info_menu v4l2_ctl_array_menu[] = {
	{
	 .id = V4L2_CID_COLORFX,
	 .max = 3,
	 .mask = 0x0,
	 .def = 0,},
	{
	 .id = V4L2_CID_EXPOSURE_AUTO,
	 .max = 1,
	 .mask = 0x0,
	 .def = 1,},
};

#endif				/* __MODULE_DIFF_H__ */
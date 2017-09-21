/*
 * OWL eDP(Embedded Display Port)  controller.
 *
 * Copyright (c) 2015 Actions Semi Co., Ltd.
 *
 * Author: Lipeng<lipeng@actions-semi.com>
 *
 * Change log:
 *	2015/8/8: Created by Lipeng.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __EDP_EDPC_H_
#define __EDP_EDPC_H_

#define EDP_LNK_LANE_COUNT		(0x0004)
#define EDP_LNK_ENHANCED		(0x0008)
#define EDP_LNK_TRAP			(0x000c)
#define EDP_LNK_QUAL_PAT		(0x0010)
#define EDP_LNK_SCR_CTRL		(0x0014)
#define EDP_LNK_DSPR_CTRL		(0x0018)
#define EDP_LNK_SCR_RST			(0x001c)
#define EDP_LNK_PANEL_SRF		(0x0020)
#define EDP_CORE_TX_EN			(0x0080)
#define EDP_CORE_MSTREAM_EN		(0x0084)
#define EDP_CORE_SSTREAM_EN		(0x0088)
#define EDP_CORE_FSCR_RST		(0x00C0)
#define EDP_CORE_USER_CFG		(0x00C4)
#define EDP_CORE_CAPS			(0x00F8)
#define EDP_CORE_ID			(0x00FC)
#define EDP_AUX_COMD			(0x0100)
#define EDP_AUX_WR_FIFO			(0x0104)
#define EDP_AUX_ADDR			(0x0108)
#define EDP_AUX_CLK_DIV			(0x010C)
#define EDP_AUX_STATE			(0x0130)
#define EDP_AUX_RPLY_DAT		(0x0134)
#define EDP_AUX_RPLY_CODE		(0x0138)
#define EDP_AUX_RPLY_COUNT		(0x013C)
#define EDP_AUX_INT_STAT		(0x0140)
#define EDP_AUX_INT_MASK		(0x0144)
#define EDP_AUX_RPLY_DAT_CNT		(0x0148)
#define EDP_AUX_STATUS			(0x014C)
#define EDP_AUX_RCLK_WIDTH		(0x0150)
#define EDP_MSTREAM_HTOTAL		(0x0180)
#define EDP_MSTREAM_VTOTAL		(0x0184)
#define EDP_MSTREAM_POLARITY		(0x0188)
#define EDP_MSTREAM_HSWIDTH		(0x018C)
#define EDP_MSTREAM_VSWIDTH		(0x0190)
#define EDP_MSTREAM_HRES		(0x0194)
#define EDP_MSTREAM_VRES		(0x0198)
#define EDP_MSTREAM_HSTART		(0x019C)
#define EDP_MSTREAM_VSTART		(0x01A0)
#define EDP_MSTREAM_MISC0		(0x01A4)
#define EDP_MSTREAM_MISC1		(0x01A8)
#define EDP_M_VID			(0x01AC)
#define EDP_MTRANSFER_UNIT		(0x01B0)
#define EDP_N_VID			(0x01B4)
#define EDP_USER_PIXEL_WIDTH		(0x01B8)
#define EDP_USER_DATA_COUNT		(0x01BC)
#define EDP_MSTREAM_INTERLACED		(0x01C0)
#define EDP_USER_SYNC_POLARITY		(0x01C4)
#define EDP_PHY_RESET			(0x0200)
#define EDP_PHY_PREEM_L0		(0x0204)
#define EDP_PHY_PREEM_L1		(0x0208)
#define EDP_PHY_PREEM_L2		(0x020C)
#define EDP_PHY_PREEM_L3		(0x0210)
#define EDP_PHY_VSW_L0			(0x0214)
#define EDP_PHY_VSW_L1			(0x0218)
#define EDP_PHY_VSW_L2			(0x021C)
#define EDP_PHY_VSW_L3			(0x0220)
#define EDP_PHY_VSW_AUX			(0x0224)
#define EDP_PHY_PWR_DOWN		(0x0228)
#define EDP_PHY_CAL_CONFIG		(0x022C)
#define EDP_PHY_CAL_CTRL		(0x0230)
#define EDP_PHY_CTRL			(0x0234)
#define EDP_SDB_LANE_SELECT		(0x0300)
#define EDP_SDB_WRITE_INDEX		(0x0304)
#define EDP_SDB_DATA_COUNT		(0x0308)
#define EDP_SDB_DATA			(0x030C)
#define EDP_SDB_READY			(0x0310)
#define EDP_SDB_BUSY			(0x0314)
#define EDP_RGB_CTL			(0x0500)
#define EDP_RGB_STATUS			(0x0504)
#define EDP_RGB_COLOR			(0x0508)
#define EDP_DEBUG			(0x050C)

#endif	/* __EDP_EDPC_H_ */


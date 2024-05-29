// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Renesas R-Car MIPI CSI-2 Receiver
 *
 * Copyright (C) 2018 Renesas Electronics Corp.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/sys_soc.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>

#include "rcar-vin.h"
#include "snps-csi2camera.h"

struct rcar_csi2;

/* Register offsets and bits */

/* Control Timing Select */
#define TREF_REG			0x00
#define TREF_TREF			BIT(0)

/* Software Reset */
#define SRST_REG			0x04
#define SRST_SRST			BIT(0)

/* PHY Operation Control */
#define PHYCNT_REG			0x08
#define PHYCNT_SHUTDOWNZ		BIT(17)
#define PHYCNT_RSTZ			BIT(16)
#define PHYCNT_ENABLECLK		BIT(4)
#define PHYCNT_ENABLE_3			BIT(3)
#define PHYCNT_ENABLE_2			BIT(2)
#define PHYCNT_ENABLE_1			BIT(1)
#define PHYCNT_ENABLE_0			BIT(0)

/* Checksum Control */
#define CHKSUM_REG			0x0c
#define CHKSUM_ECC_EN			BIT(1)
#define CHKSUM_CRC_EN			BIT(0)

/*
 * Channel Data Type Select
 * VCDT[0-15]:  Channel 0 VCDT[16-31]:  Channel 1
 * VCDT2[0-15]: Channel 2 VCDT2[16-31]: Channel 3
 */
#define VCDT_REG			0x10
#define VCDT2_REG			0x14
#define VCDT_VCDTN_EN			BIT(15)
#define VCDT_SEL_VC(n)			(((n) & 0x3) << 8)
#define VCDT_SEL_DTN_ON			BIT(6)
#define VCDT_SEL_DT(n)			(((n) & 0x3f) << 0)

/* Frame Data Type Select */
#define FRDT_REG			0x18

/* Field Detection Control */
#define FLD_REG				0x1c
#define FLD_FLD_NUM(n)			(((n) & 0xff) << 16)
#define FLD_DET_SEL(n)			(((n) & 0x3) << 4)
#define FLD_FLD_EN4			BIT(3)
#define FLD_FLD_EN3			BIT(2)
#define FLD_FLD_EN2			BIT(1)
#define FLD_FLD_EN			BIT(0)

/* Automatic Standby Control */
#define ASTBY_REG			0x20

/* Long Data Type Setting 0 */
#define LNGDT0_REG			0x28

/* Long Data Type Setting 1 */
#define LNGDT1_REG			0x2c

/* Interrupt Enable */
#define INTEN_REG			0x30
#define INTEN_INT_AFIFO_OF		BIT(27)
#define INTEN_INT_ERRSOTHS		BIT(4)
#define INTEN_INT_ERRSOTSYNCHS		BIT(3)

/* Interrupt Source Mask */
#define INTCLOSE_REG			0x34

/* Interrupt Status Monitor */
#define INTSTATE_REG			0x38
#define INTSTATE_INT_ULPS_START		BIT(7)
#define INTSTATE_INT_ULPS_END		BIT(6)

/* Interrupt Error Status Monitor */
#define INTERRSTATE_REG			0x3c

/* Short Packet Data */
#define SHPDAT_REG			0x40

/* Short Packet Count */
#define SHPCNT_REG			0x44

/* LINK Operation Control */
#define LINKCNT_REG			0x48
#define LINKCNT_MONITOR_EN		BIT(31)
#define LINKCNT_REG_MONI_PACT_EN	BIT(25)
#define LINKCNT_ICLK_NONSTOP		BIT(24)

/* Lane Swap */
#define LSWAP_REG			0x4c
#define LSWAP_L3SEL(n)			(((n) & 0x3) << 6)
#define LSWAP_L2SEL(n)			(((n) & 0x3) << 4)
#define LSWAP_L1SEL(n)			(((n) & 0x3) << 2)
#define LSWAP_L0SEL(n)			(((n) & 0x3) << 0)

/* PHY Test Interface Write Register */
#define PHTW_REG			0x50
#define PHTW_DWEN			BIT(24)
#define PHTW_TESTDIN_DATA(n)		(((n & 0xff)) << 16)
#define PHTW_CWEN			BIT(8)
#define PHTW_TESTDIN_CODE(n)		((n & 0xff))

/* V4H Registers */
#define N_LANES			0x0004

#define CSI2_RESETN		0x0008

#define PHY_SHUTDOWNZ	0x0040

#define PHY_MODE		0x001c

#define DPHY_RSTZ		0x0044

#define FLDC			0x0804

#define FLDD			0x0808

#define IDIC			0x0810

#define OVR1					0x0848
#define OVR1_forcerxmode_3		BIT(12)
#define OVR1_forcerxmode_2		BIT(11)
#define OVR1_forcerxmode_1		BIT(10)
#define OVR1_forcerxmode_0		BIT(9)
#define OVR1_forcerxmode_dck	BIT(8)

#define PHY_EN			0x2000
#define PHY_ENABLE_3	BIT(7)
#define PHY_ENABLE_2	BIT(6)
#define PHY_ENABLE_1	BIT(5)
#define PHY_ENABLE_0	BIT(4)
#define PHY_ENABLE_DCK	BIT(0)

#define FRXM					0x2004
#define FRXM_FORCERXMODE_DCK	BIT(4)
#define FRXM_FORCERXMODE_3		BIT(3)
#define FRXM_FORCERXMODE_2		BIT(2)
#define FRXM_FORCERXMODE_1		BIT(1)
#define FRXM_FORCERXMODE_0		BIT(0)

/* V4M registers */
#define V4M_PHYPLL		0x02050
#define V4M_CSI0CLKFCPR	0x02054
#define V4M_CSI0CLKFREQRANGE(n)		(((n) & 0xff) << 16)
#define V4M_PHTW		0x02060
#define V4M_PHTW_DIN_DATA_PP(n)		((n) & 0xff)
#define V4M_PHTW_DIN_DATA_D(n)		(((n) & 0xf00) >> 16)
#define V4M_PHTR		0x02064
#define PHTR_TESTDOUT_CODE(n)		(((n) & 0xff) << 16)
#define V4M_PHTC		0x02068

#define ST_PHYST		0x2814
#define ST_PHY_READY	BIT(31)
#define ST_STOPSTATE_DCK	BIT(7)
#define ST_STOPSTATE_3	BIT(3)
#define ST_STOPSTATE_2	BIT(2)
#define ST_STOPSTATE_1	BIT(1)
#define ST_STOPSTATE_0	BIT(0)

/* V4H PPI registers */
#define PPI_STARTUP_RW_COMMON_DPHY(n)		(0x21800 + (n *2 ))	/* n = 0 - 9 */
#define PPI_STARTUP_RW_COMMON_STARTUP_1_1	0x21822
#define PPI_CALIBCTRL_RW_COMMON_BG_0		0x2184C
#define PPI_RW_LPDCOCAL_TIMEBASE			0x21C02
#define PPI_RW_LPDCOCAL_NREF				0x21C04
#define PPI_RW_LPDCOCAL_NREF_RANGE			0x21C06
#define PPI_RW_LPDCOCAL_TWAIT_CONFIG		0x21C0A
#define PPI_RW_LPDCOCAL_VT_CONFIG			0x21C0C
#define PPI_RW_LPDCOCAL_COARSE_CFG			0x21C10
#define PPI_RW_DDLCAL_CFG(n)				(0x21C40 + (n * 2))	/* n = 0 - 7 */
#define PPI_RW_COMMON_CFG					0x21C6C
#define PPI_RW_TERMCAL_CFG_0				0x21C80
#define PPI_RW_OFFSETCAL_CFG_0				0x21CA0

/* V4H CORE registers */
#define CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2(n)	(0x22040 + (n * 2))	/* n = 0 - 15 */
#define CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2(n)	(0x22440 + (n * 2))	/* n = 0 - 15 */
#define CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2(n)	(0x22840 + (n * 2))	/* n = 0 - 15 */
#define CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2(n)	(0x22C40 + (n * 2))	/* n = 0 - 15 */
#define CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2(n)	(0x23040 + (n * 2))	/* n = 0 - 15 */

#define CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(n)		(0x23840 + (n * 2))	/* n = 0 - 11 */

#define CORE_DIG_RW_COMMON(n)					(0x23880 + (n * 2))	/* n = 0 - 15 */

#define CORE_DIG_ANACTRL_RW_COMMON_ANACTRL(n)	(0x239E0 + (n * 2))	/* n = 0 - 3 */

#define CORE_DIG_COMMON_RW_DESKEW_FINE_MEM		0x23FE0

#define CORE_DIG_CLANE_0_RW_CFG_0				0x2A000
#define CORE_DIG_CLANE_1_RW_CFG_0				0x2A400
#define CORE_DIG_CLANE_2_RW_CFG_0				0x2A800

#define CORE_DIG_CLANE_0_RW_HS_TX_6				0x2A20C
#define CORE_DIG_CLANE_1_RW_HS_TX_6				0x2A60C
#define CORE_DIG_CLANE_2_RW_HS_TX_6				0x2AA0C

#define CORE_DIG_DLANE_0_RW_CFG(n)				(0x26000 + (n * 2))	/* n = 0 - 2 */

#define CORE_DIG_DLANE_0_RW_LP(n)				(0x26080 + (n * 2))	/* n = 0 - 1 */

#define CORE_DIG_DLANE_0_RW_HS_RX(n)			(0x26100 + (n * 2))	/* n = 0 - 8 */

#define CORE_DIG_DLANE_1_RW_CFG(n)				(0x26400 + (n * 2))	/* n = 0 - 2 */

#define CORE_DIG_DLANE_1_RW_LP(n)				(0x26480 + (n * 2))	/* n = 0 - 1 */

#define CORE_DIG_DLANE_1_RW_HS_RX(n)			(0x26500 + (n * 2))	/* n = 0 - 8 */

#define CORE_DIG_DLANE_2_RW_CFG(n)				(0x26800 + (n * 2))	/* n = 0 - 2 */

#define CORE_DIG_DLANE_2_RW_LP(n)				(0x268A0 + (n * 2))	/* n = 0 - 1 */

#define CORE_DIG_DLANE_2_RW_HS_RX(n)			(0x26900 + (n * 2))	/* n = 0 - 8 */

#define CORE_DIG_DLANE_3_RW_CFG(n)				(0x26C00 + (n * 2))	/* n = 0 - 2 */

#define CORE_DIG_DLANE_3_RW_LP(n)				(0x26CA0 + (n * 2))	/* n = 0 - 1 */

#define CORE_DIG_DLANE_3_RW_HS_RX(n)			(0x26D00 + (n * 2))	/* n = 0 - 8 */

#define CORE_DIG_DLANE_CLK_RW_CFG(n)			(0x27000 + (n * 2))	/* n = 0 - 2 */

#define CORE_DIG_DLANE_CLK_RW_LP(n)				(0x27080 + (n * 2))	/* n = 0 - 1 */

#define CORE_DIG_DLANE_CLK_RW_HS_RX(n)			(0x27100 + (n * 2))	/* n = 0 - 8 */

/* C-PHY */
#define CORE_DIG_RW_TRIO0(n)					(0x22100 + (n * 2))
#define CORE_DIG_RW_TRIO1(n)					(0x22500 + (n * 2))
#define CORE_DIG_RW_TRIO2(n)					(0x22900 + (n * 2))

#define CORE_DIG_CLANE_0_RW_LP_0				0x2A080
#define CORE_DIG_CLANE_0_RW_HS_RX(n)			(0x2A100 + (n * 2)) /* n = 0 ~ 6 */

#define CORE_DIG_CLANE_1_RW_LP_0				0x2A480
#define CORE_DIG_CLANE_1_RW_HS_RX(n)			(0x2A500 + (n * 2)) /* n = 0 ~ 6 */

#define CORE_DIG_CLANE_2_RW_LP_0				0x2A880
#define CORE_DIG_CLANE_2_RW_HS_RX(n)			(0x2A900 + (n * 2)) /* n = 0 ~ 6 */

#define CSI1300		1

#define CSI2_CPHY_SETTING(ms, rx2, t0, t1, t2, a29, a27) \
	.msps = (ms), \
	.rw_hs_rx_2 = (rx2), \
	.rw_trio_0 = (t0), \
	.rw_trio_1 = (t1), \
	.rw_trio_2 = (t2), \
	.afe_lane0_29 = (a29), \
	.afe_lane0_27 = (a27)

/** SNPS CSI-2 v4.0 **/
/* MAIN registers */
#define TO_HSRX_CFG							0x24
#define TO_HSRX_CFG_BIT						GENMASK(31, 0)

#define PWR_UP								0xc
#define PWR_UP_BIT							BIT(0)

/* PHY registers */
#define PHY_MODE_CFG						0x100
#define PPI_WIDTH							GENMASK(17, 16)
#define PHY_L3_DISABLE						BIT(11)
#define PHY_L2_DISABLE						BIT(10)
#define PHY_L1_DISABLE						BIT(9)
#define PHY_L0_DISABLE						BIT(8)
#define PHY_MODE_BIT						BIT(0)

#define PHY_DESKEW_CFG						0x104
#define DESKEW_SYS_CYCLES					GENMASK(7, 0)

/* CSI-2 registers */
#define CSI2_GENERAL_CFG					0x200
#define ECC_VCX_OVERRIDE					BIT(1)
#define CSI2_RXSYNCHS_FILTER_EN				BIT(0)

#define CSI2_DESCRAMBLING_CFG				0x204
#define CSI2_DESCRAMBLING_EN				BIT(0)

#define CSI2_DESCRAMBLING0_CFG				0x208
#define CSI2_DESCRAMBLING_L0_SEED1			GENMASK(31, 16)
#define CSI2_DESCRAMBLING_L0_SEED0			GENMASK(15, 0)

/* SDI registers */
#define SDI_CFG								0x400
#define SDI_ENCODE_MODE						BIT(1)
#define SDI_ENABLE							BIT(0)

#define SDI_FILTER_CFG						0x404
#define SDI_HDR_FE_SP						BIT(20)
#define SDI_EXCLUDE_HDR_FE					BIT(19)
#define SDI_EXCLUDE_SP						BIT(18)
#define SDI_SELECT_DT_EN					BIT(17)
#define SDI_SELECT_VC_EN					BIT(16)
#define SDI_SELECT_DT						GENMASK(13, 8)
#define SDI_SELECT_VC						GENMASK(4, 0)

#define SDI_CTRL							0x408

/* INT registers */
#define INT_ST_TO							0x528
#define HSRX_TO_ERR_IRQ						BIT(0)

#define INT_UNMASK_CSI2						0x59c
#define UNMASK_MAX_N_DATA_IDS_ERR_IRQ		BIT(5)
#define UNMASK_INVALID_DT_ERR_IRQ			BIT(4)
#define UNMASK_CRC_ERR_IRQ					BIT(3)
#define UNMASK_INVALID_RX_LENGTH_ERR_IRQ	BIT(2)
#define UNMASK_HDR_NON_FATAL_ERR_IRQ		BIT(1)
#define UNMASK_HDR_FATAL_ERR_IRQ			BIT(0)

#define INT_UNMASK_FRAME					0x5a0
#define UNMASK_FRAME_SEQUENCE_ERR_IRQ		BIT(1)
#define UNMASK_FRAME_BOUNDARY_ERR_IRQ		BIT(0)

#define INT_UNMASK_LINE						0x5a4
#define UNMASK_LINE_SEQUENCE_ERR_IRQ		BIT(1)
#define UNMASK_LINE_BOUNDARY_ERR_IRQ		BIT(0)
/****/

struct rcsi2_cphy_setting {
	u16 msps;
	u16 rw_hs_rx_2;
	u16 rw_trio_0;
	u16 rw_trio_1;
	u16 rw_trio_2;
	u16 afe_lane0_29;
	u16 afe_lane0_27;
};

static const struct rcsi2_cphy_setting cphy_setting_table_r8a779g0[] = {
	{ CSI2_CPHY_SETTING(80, 0x0038, 0x0200, 0x0134, 0x006a, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(100, 0x0038, 0x0200, 0x00f5, 0x0055, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(200, 0x0038, 0x0200, 0x0077, 0x002b, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(300, 0x0038, 0x0200, 0x004d, 0x001d, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(400, 0x0038, 0x0200, 0x0038, 0x0016, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(500, 0x0038, 0x0200, 0x002c, 0x0012, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(600, 0x0038, 0x0200, 0x0023, 0x000f, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(700, 0x0038, 0x0200, 0x001d, 0x000d, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(800, 0x0038, 0x0200, 0x0019, 0x000c, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(900, 0x0038, 0x0200, 0x0015, 0x000b, 0x0000, 0x0000) },
	{ CSI2_CPHY_SETTING(1000, 0x003e, 0x0200, 0x0013, 0x000a, 0x0000, 0x0400) },
	{ CSI2_CPHY_SETTING(1100, 0x0044, 0x0200, 0x0010, 0x0009, 0x0000, 0x0800) },
	{ CSI2_CPHY_SETTING(1200, 0x004a, 0x0200, 0x000e, 0x0008, 0x0000, 0x0c00) },
	{ CSI2_CPHY_SETTING(1300, 0x0051, 0x0200, 0x000d, 0x0008, 0x0000, 0x0c00) },
	{ CSI2_CPHY_SETTING(1400, 0x0057, 0x0200, 0x000b, 0x0007, 0x0000, 0x1000) },
	{ CSI2_CPHY_SETTING(1500, 0x005d, 0x0400, 0x000a, 0x0007, 0x0000, 0x1000) },
	{ CSI2_CPHY_SETTING(1600, 0x0063, 0x0400, 0x0009, 0x0007, 0x0000, 0x1400) },
	{ CSI2_CPHY_SETTING(1700, 0x006a, 0x0400, 0x0008, 0x0006, 0x0000, 0x1400) },
	{ CSI2_CPHY_SETTING(1800, 0x0070, 0x0400, 0x0007, 0x0006, 0x0000, 0x1400) },
	{ CSI2_CPHY_SETTING(1900, 0x0076, 0x0400, 0x0007, 0x0006, 0x0000, 0x1400) },
	{ CSI2_CPHY_SETTING(2000, 0x007c, 0x0400, 0x0006, 0x0006, 0x0000, 0x1800) },
	{ CSI2_CPHY_SETTING(2100, 0x0083, 0x0400, 0x0005, 0x0005, 0x0000, 0x1800) },
	{ CSI2_CPHY_SETTING(2200, 0x0089, 0x0600, 0x0005, 0x0005, 0x0000, 0x1800) },
	{ CSI2_CPHY_SETTING(2300, 0x008f, 0x0600, 0x0004, 0x0005, 0x0000, 0x1800) },
	{ CSI2_CPHY_SETTING(2400, 0x0095, 0x0600, 0x0004, 0x0005, 0x0000, 0x1800) },
	{ CSI2_CPHY_SETTING(2500, 0x009c, 0x0600, 0x0004, 0x0005, 0x0000, 0x1c00) },
	{ CSI2_CPHY_SETTING(2600, 0x00a2, 0x0600, 0x0003, 0x0005, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(2700, 0x00a8, 0x0600, 0x0003, 0x0005, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(2800, 0x00ae, 0x0600, 0x0002, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(2900, 0x00b5, 0x0800, 0x0002, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(3000, 0x00bb, 0x0800, 0x0002, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(3100, 0x00c1, 0x0800, 0x0002, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(3200, 0x00c7, 0x0800, 0x0001, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(3300, 0x00ce, 0x0800, 0x0001, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(3400, 0x00d4, 0x0800, 0x0001, 0x0004, 0x0010, 0x1c00) },
	{ CSI2_CPHY_SETTING(3500, 0x00da, 0x0800, 0x0001, 0x0004, 0x0010, 0x1c00) },
	{ /* sentinel */ },
};

#define PHYFRX_REG			0x64
#define PHYFRX_FORCERX_MODE_3		BIT(3)
#define PHYFRX_FORCERX_MODE_2		BIT(2)
#define PHYFRX_FORCERX_MODE_1		BIT(1)
#define PHYFRX_FORCERX_MODE_0		BIT(0)

struct phtw_value {
	u16 data;
	u16 code;
};

struct rcsi2_mbps_reg {
	u16 mbps;
	u16 reg;
};

static const struct rcsi2_mbps_reg phtw_mbps_v3u[] = {
	{ .mbps = 1500, .reg = 0xcc },
	{ .mbps = 1550, .reg = 0x1d },
	{ .mbps = 1600, .reg = 0x27 },
	{ .mbps = 1650, .reg = 0x30 },
	{ .mbps = 1700, .reg = 0x39 },
	{ .mbps = 1750, .reg = 0x42 },
	{ .mbps = 1800, .reg = 0x4b },
	{ .mbps = 1850, .reg = 0x55 },
	{ .mbps = 1900, .reg = 0x5e },
	{ .mbps = 1950, .reg = 0x67 },
	{ .mbps = 2000, .reg = 0x71 },
	{ .mbps = 2050, .reg = 0x79 },
	{ .mbps = 2100, .reg = 0x83 },
	{ .mbps = 2150, .reg = 0x8c },
	{ .mbps = 2200, .reg = 0x95 },
	{ .mbps = 2250, .reg = 0x9e },
	{ .mbps = 2300, .reg = 0xa7 },
	{ .mbps = 2350, .reg = 0xb0 },
	{ .mbps = 2400, .reg = 0xba },
	{ .mbps = 2450, .reg = 0xc3 },
	{ .mbps = 2500, .reg = 0xcc },
	{ /* sentinel */ },
};

static const struct rcsi2_mbps_reg phtw_mbps_h3_v3h_m3n[] = {
	{ .mbps =   80, .reg = 0x86 },
	{ .mbps =   90, .reg = 0x86 },
	{ .mbps =  100, .reg = 0x87 },
	{ .mbps =  110, .reg = 0x87 },
	{ .mbps =  120, .reg = 0x88 },
	{ .mbps =  130, .reg = 0x88 },
	{ .mbps =  140, .reg = 0x89 },
	{ .mbps =  150, .reg = 0x89 },
	{ .mbps =  160, .reg = 0x8a },
	{ .mbps =  170, .reg = 0x8a },
	{ .mbps =  180, .reg = 0x8b },
	{ .mbps =  190, .reg = 0x8b },
	{ .mbps =  205, .reg = 0x8c },
	{ .mbps =  220, .reg = 0x8d },
	{ .mbps =  235, .reg = 0x8e },
	{ .mbps =  250, .reg = 0x8e },
	{ /* sentinel */ },
};

static const struct rcsi2_mbps_reg phtw_mbps_v3m_e3[] = {
	{ .mbps =   80, .reg = 0x00 },
	{ .mbps =   90, .reg = 0x20 },
	{ .mbps =  100, .reg = 0x40 },
	{ .mbps =  110, .reg = 0x02 },
	{ .mbps =  130, .reg = 0x22 },
	{ .mbps =  140, .reg = 0x42 },
	{ .mbps =  150, .reg = 0x04 },
	{ .mbps =  170, .reg = 0x24 },
	{ .mbps =  180, .reg = 0x44 },
	{ .mbps =  200, .reg = 0x06 },
	{ .mbps =  220, .reg = 0x26 },
	{ .mbps =  240, .reg = 0x46 },
	{ .mbps =  250, .reg = 0x08 },
	{ .mbps =  270, .reg = 0x28 },
	{ .mbps =  300, .reg = 0x0a },
	{ .mbps =  330, .reg = 0x2a },
	{ .mbps =  360, .reg = 0x4a },
	{ .mbps =  400, .reg = 0x0c },
	{ .mbps =  450, .reg = 0x2c },
	{ .mbps =  500, .reg = 0x0e },
	{ .mbps =  550, .reg = 0x2e },
	{ .mbps =  600, .reg = 0x10 },
	{ .mbps =  650, .reg = 0x30 },
	{ .mbps =  700, .reg = 0x12 },
	{ .mbps =  750, .reg = 0x32 },
	{ .mbps =  800, .reg = 0x52 },
	{ .mbps =  850, .reg = 0x72 },
	{ .mbps =  900, .reg = 0x14 },
	{ .mbps =  950, .reg = 0x34 },
	{ .mbps = 1000, .reg = 0x54 },
	{ .mbps = 1050, .reg = 0x74 },
	{ .mbps = 1125, .reg = 0x16 },
	{ /* sentinel */ },
};

/* PHY Test Interface Clear */
#define PHTC_REG			0x58
#define PHTC_TESTCLR			BIT(0)

/* PHY Frequency Control */
#define PHYPLL_REG			0x68
#define PHYPLL_HSFREQRANGE(n)		((n) << 16)

static const struct rcsi2_mbps_reg hsfreqrange_v3u[] = {
	{ .mbps =   80, .reg = 0x00 },
	{ .mbps =   90, .reg = 0x10 },
	{ .mbps =  100, .reg = 0x20 },
	{ .mbps =  110, .reg = 0x30 },
	{ .mbps =  120, .reg = 0x01 },
	{ .mbps =  130, .reg = 0x11 },
	{ .mbps =  140, .reg = 0x21 },
	{ .mbps =  150, .reg = 0x31 },
	{ .mbps =  160, .reg = 0x02 },
	{ .mbps =  170, .reg = 0x12 },
	{ .mbps =  180, .reg = 0x22 },
	{ .mbps =  190, .reg = 0x32 },
	{ .mbps =  205, .reg = 0x03 },
	{ .mbps =  220, .reg = 0x13 },
	{ .mbps =  235, .reg = 0x23 },
	{ .mbps =  250, .reg = 0x33 },
	{ .mbps =  275, .reg = 0x04 },
	{ .mbps =  300, .reg = 0x14 },
	{ .mbps =  325, .reg = 0x25 },
	{ .mbps =  350, .reg = 0x35 },
	{ .mbps =  400, .reg = 0x05 },
	{ .mbps =  450, .reg = 0x16 },
	{ .mbps =  500, .reg = 0x26 },
	{ .mbps =  550, .reg = 0x37 },
	{ .mbps =  600, .reg = 0x07 },
	{ .mbps =  650, .reg = 0x18 },
	{ .mbps =  700, .reg = 0x28 },
	{ .mbps =  750, .reg = 0x39 },
	{ .mbps =  800, .reg = 0x09 },
	{ .mbps =  850, .reg = 0x19 },
	{ .mbps =  900, .reg = 0x29 },
	{ .mbps =  950, .reg = 0x3a },
	{ .mbps = 1000, .reg = 0x0a },
	{ .mbps = 1050, .reg = 0x1a },
	{ .mbps = 1100, .reg = 0x2a },
	{ .mbps = 1150, .reg = 0x3b },
	{ .mbps = 1200, .reg = 0x0b },
	{ .mbps = 1250, .reg = 0x1b },
	{ .mbps = 1300, .reg = 0x2b },
	{ .mbps = 1350, .reg = 0x3c },
	{ .mbps = 1400, .reg = 0x0c },
	{ .mbps = 1450, .reg = 0x1c },
	{ .mbps = 1500, .reg = 0x2c },
	{ .mbps = 1550, .reg = 0x3d },
	{ .mbps = 1600, .reg = 0x0d },
	{ .mbps = 1650, .reg = 0x1d },
	{ .mbps = 1700, .reg = 0x2e },
	{ .mbps = 1750, .reg = 0x3e },
	{ .mbps = 1800, .reg = 0x0e },
	{ .mbps = 1850, .reg = 0x1e },
	{ .mbps = 1900, .reg = 0x2f },
	{ .mbps = 1950, .reg = 0x3f },
	{ .mbps = 2000, .reg = 0x0f },
	{ .mbps = 2050, .reg = 0x40 },
	{ .mbps = 2100, .reg = 0x41 },
	{ .mbps = 2150, .reg = 0x42 },
	{ .mbps = 2200, .reg = 0x43 },
	{ .mbps = 2300, .reg = 0x45 },
	{ .mbps = 2350, .reg = 0x46 },
	{ .mbps = 2400, .reg = 0x47 },
	{ .mbps = 2450, .reg = 0x48 },
	{ .mbps = 2500, .reg = 0x49 },
	{ /* sentinel */ },
};

static const struct rcsi2_mbps_reg hsfreqrange_h3_v3h_m3n[] = {
	{ .mbps =   80, .reg = 0x00 },
	{ .mbps =   90, .reg = 0x10 },
	{ .mbps =  100, .reg = 0x20 },
	{ .mbps =  110, .reg = 0x30 },
	{ .mbps =  120, .reg = 0x01 },
	{ .mbps =  130, .reg = 0x11 },
	{ .mbps =  140, .reg = 0x21 },
	{ .mbps =  150, .reg = 0x31 },
	{ .mbps =  160, .reg = 0x02 },
	{ .mbps =  170, .reg = 0x12 },
	{ .mbps =  180, .reg = 0x22 },
	{ .mbps =  190, .reg = 0x32 },
	{ .mbps =  205, .reg = 0x03 },
	{ .mbps =  220, .reg = 0x13 },
	{ .mbps =  235, .reg = 0x23 },
	{ .mbps =  250, .reg = 0x33 },
	{ .mbps =  275, .reg = 0x04 },
	{ .mbps =  300, .reg = 0x14 },
	{ .mbps =  325, .reg = 0x25 },
	{ .mbps =  350, .reg = 0x35 },
	{ .mbps =  400, .reg = 0x05 },
	{ .mbps =  450, .reg = 0x16 },
	{ .mbps =  500, .reg = 0x26 },
	{ .mbps =  550, .reg = 0x37 },
	{ .mbps =  600, .reg = 0x07 },
	{ .mbps =  650, .reg = 0x18 },
	{ .mbps =  700, .reg = 0x28 },
	{ .mbps =  750, .reg = 0x39 },
	{ .mbps =  800, .reg = 0x09 },
	{ .mbps =  850, .reg = 0x19 },
	{ .mbps =  900, .reg = 0x29 },
	{ .mbps =  950, .reg = 0x3a },
	{ .mbps = 1000, .reg = 0x0a },
	{ .mbps = 1050, .reg = 0x1a },
	{ .mbps = 1100, .reg = 0x2a },
	{ .mbps = 1150, .reg = 0x3b },
	{ .mbps = 1200, .reg = 0x0b },
	{ .mbps = 1250, .reg = 0x1b },
	{ .mbps = 1300, .reg = 0x2b },
	{ .mbps = 1350, .reg = 0x3c },
	{ .mbps = 1400, .reg = 0x0c },
	{ .mbps = 1450, .reg = 0x1c },
	{ .mbps = 1500, .reg = 0x2c },
	{ /* sentinel */ },
};

static const struct rcsi2_mbps_reg hsfreqrange_m3w_h3es1[] = {
	{ .mbps =   80,	.reg = 0x00 },
	{ .mbps =   90,	.reg = 0x10 },
	{ .mbps =  100,	.reg = 0x20 },
	{ .mbps =  110,	.reg = 0x30 },
	{ .mbps =  120,	.reg = 0x01 },
	{ .mbps =  130,	.reg = 0x11 },
	{ .mbps =  140,	.reg = 0x21 },
	{ .mbps =  150,	.reg = 0x31 },
	{ .mbps =  160,	.reg = 0x02 },
	{ .mbps =  170,	.reg = 0x12 },
	{ .mbps =  180,	.reg = 0x22 },
	{ .mbps =  190,	.reg = 0x32 },
	{ .mbps =  205,	.reg = 0x03 },
	{ .mbps =  220,	.reg = 0x13 },
	{ .mbps =  235,	.reg = 0x23 },
	{ .mbps =  250,	.reg = 0x33 },
	{ .mbps =  275,	.reg = 0x04 },
	{ .mbps =  300,	.reg = 0x14 },
	{ .mbps =  325,	.reg = 0x05 },
	{ .mbps =  350,	.reg = 0x15 },
	{ .mbps =  400,	.reg = 0x25 },
	{ .mbps =  450,	.reg = 0x06 },
	{ .mbps =  500,	.reg = 0x16 },
	{ .mbps =  550,	.reg = 0x07 },
	{ .mbps =  600,	.reg = 0x17 },
	{ .mbps =  650,	.reg = 0x08 },
	{ .mbps =  700,	.reg = 0x18 },
	{ .mbps =  750,	.reg = 0x09 },
	{ .mbps =  800,	.reg = 0x19 },
	{ .mbps =  850,	.reg = 0x29 },
	{ .mbps =  900,	.reg = 0x39 },
	{ .mbps =  950,	.reg = 0x0a },
	{ .mbps = 1000,	.reg = 0x1a },
	{ .mbps = 1050,	.reg = 0x2a },
	{ .mbps = 1100,	.reg = 0x3a },
	{ .mbps = 1150,	.reg = 0x0b },
	{ .mbps = 1200,	.reg = 0x1b },
	{ .mbps = 1250,	.reg = 0x2b },
	{ .mbps = 1300,	.reg = 0x3b },
	{ .mbps = 1350,	.reg = 0x0c },
	{ .mbps = 1400,	.reg = 0x1c },
	{ .mbps = 1450,	.reg = 0x2c },
	{ .mbps = 1500,	.reg = 0x3c },
	{ /* sentinel */ },
};

static const struct rcsi2_mbps_reg hsfreqrange_v4m[] = {
	{ .mbps = 80, .reg = 0x00 },
	{ .mbps = 90, .reg = 0x10 },
	{ .mbps = 100, .reg = 0x20 },
	{ .mbps = 110, .reg = 0x30 },
	{ .mbps = 120, .reg = 0x01 },
	{ .mbps = 130, .reg = 0x11 },
	{ .mbps = 140, .reg = 0x21 },
	{ .mbps = 150, .reg = 0x31 },
	{ .mbps = 160, .reg = 0x02 },
	{ .mbps = 170, .reg = 0x12 },
	{ .mbps = 180, .reg = 0x22 },
	{ .mbps = 190, .reg = 0x32 },
	{ .mbps = 205, .reg = 0x03 },
	{ .mbps = 220, .reg = 0x13 },
	{ .mbps = 235, .reg = 0x23 },
	{ .mbps = 250, .reg = 0x33 },
	{ .mbps = 275, .reg = 0x04 },
	{ .mbps = 300, .reg = 0x14 },
	{ .mbps = 325, .reg = 0x25 },
	{ .mbps = 350, .reg = 0x35 },
	{ .mbps = 400, .reg = 0x05 },
	{ .mbps = 450, .reg = 0x16 },
	{ .mbps = 500, .reg = 0x26 },
	{ .mbps = 550, .reg = 0x37 },
	{ .mbps = 600, .reg = 0x07 },
	{ .mbps = 650, .reg = 0x18 },
	{ .mbps = 700, .reg = 0x28 },
	{ .mbps = 750, .reg = 0x39 },
	{ .mbps = 800, .reg = 0x09 },
	{ .mbps = 850, .reg = 0x19 },
	{ .mbps = 900, .reg = 0x29 },
	{ .mbps = 950, .reg = 0x3A },
	{ .mbps = 1000, .reg = 0x0A },
	{ .mbps = 1050, .reg = 0x1A },
	{ .mbps = 1100, .reg = 0x2A },
	{ .mbps = 1150, .reg = 0x3B },
	{ .mbps = 1200, .reg = 0x0B },
	{ .mbps = 1250, .reg = 0x1B },
	{ .mbps = 1300, .reg = 0x2B },
	{ .mbps = 1350, .reg = 0x3C },
	{ .mbps = 1400, .reg = 0x0C },
	{ .mbps = 1450, .reg = 0x1C },
	{ .mbps = 1500, .reg = 0x2C },
	{ .mbps = 1550, .reg = 0x3D },
	{ .mbps = 1600, .reg = 0x0D },
	{ .mbps = 1650, .reg = 0x1D },
	{ .mbps = 1700, .reg = 0x2E },
	{ .mbps = 1750, .reg = 0x3E },
	{ .mbps = 1800, .reg = 0x0E },
	{ .mbps = 1850, .reg = 0x1E },
	{ .mbps = 1900, .reg = 0x2F },
	{ .mbps = 1950, .reg = 0x3F },
	{ .mbps = 2000, .reg = 0x0F },
	{ .mbps = 2050, .reg = 0x40 },
	{ .mbps = 2100, .reg = 0x41 },
	{ .mbps = 2150, .reg = 0x42 },
	{ .mbps = 2200, .reg = 0x43 },
	{ .mbps = 2250, .reg = 0x44 },
	{ .mbps = 2300, .reg = 0x45 },
	{ .mbps = 2350, .reg = 0x46 },
	{ .mbps = 2400, .reg = 0x47 },
	{ .mbps = 2450, .reg = 0x48 },
	{ .mbps = 2500, .reg = 0x49 },
	{ /* sentinel */ },
};

static const struct rcsi2_mbps_reg osc_freq_target_v4m[] = {
	{ .mbps = 80, .reg = 0x1A9 },
	{ .mbps = 90, .reg = 0x1A9 },
	{ .mbps = 100, .reg = 0x1A9 },
	{ .mbps = 110, .reg = 0x1A9 },
	{ .mbps = 120, .reg = 0x1A9 },
	{ .mbps = 130, .reg = 0x1A9 },
	{ .mbps = 140, .reg = 0x1A9 },
	{ .mbps = 150, .reg = 0x1A9 },
	{ .mbps = 160, .reg = 0x1A9 },
	{ .mbps = 170, .reg = 0x1A9 },
	{ .mbps = 180, .reg = 0x1A9 },
	{ .mbps = 190, .reg = 0x1A9 },
	{ .mbps = 205, .reg = 0x1A9 },
	{ .mbps = 220, .reg = 0x1A9 },
	{ .mbps = 235, .reg = 0x1A9 },
	{ .mbps = 250, .reg = 0x1A9 },
	{ .mbps = 275, .reg = 0x1A9 },
	{ .mbps = 300, .reg = 0x1A9 },
	{ .mbps = 325, .reg = 0x1A9 },
	{ .mbps = 350, .reg = 0x1A9 },
	{ .mbps = 400, .reg = 0x1A9 },
	{ .mbps = 450, .reg = 0x1A9 },
	{ .mbps = 500, .reg = 0x1A9 },
	{ .mbps = 550, .reg = 0x1A9 },
	{ .mbps = 600, .reg = 0x1A9 },
	{ .mbps = 650, .reg = 0x1A9 },
	{ .mbps = 700, .reg = 0x1A9 },
	{ .mbps = 750, .reg = 0x1A9 },
	{ .mbps = 800, .reg = 0x1A9 },
	{ .mbps = 850, .reg = 0x1A9 },
	{ .mbps = 900, .reg = 0x1A9 },
	{ .mbps = 950, .reg = 0x1A9 },
	{ .mbps = 1000, .reg = 0x1A9 },
	{ .mbps = 1050, .reg = 0x1A9 },
	{ .mbps = 1100, .reg = 0x1A9 },
	{ .mbps = 1150, .reg = 0x1A9 },
	{ .mbps = 1200, .reg = 0x1A9 },
	{ .mbps = 1250, .reg = 0x1A9 },
	{ .mbps = 1300, .reg = 0x1A9 },
	{ .mbps = 1350, .reg = 0x1A9 },
	{ .mbps = 1400, .reg = 0x1A9 },
	{ .mbps = 1450, .reg = 0x1A9 },
	{ .mbps = 1500, .reg = 0x1A9 },
	{ .mbps = 1550, .reg = 0x108 },
	{ .mbps = 1600, .reg = 0x110 },
	{ .mbps = 1650, .reg = 0x119 },
	{ .mbps = 1700, .reg = 0x121 },
	{ .mbps = 1750, .reg = 0x12A },
	{ .mbps = 1800, .reg = 0x132 },
	{ .mbps = 1850, .reg = 0x13B },
	{ .mbps = 1900, .reg = 0x143 },
	{ .mbps = 1950, .reg = 0x14C },
	{ .mbps = 2000, .reg = 0x154 },
	{ .mbps = 2050, .reg = 0x15D },
	{ .mbps = 2100, .reg = 0x165 },
	{ .mbps = 2150, .reg = 0x16E },
	{ .mbps = 2200, .reg = 0x176 },
	{ .mbps = 2250, .reg = 0x17F },
	{ .mbps = 2300, .reg = 0x187 },
	{ .mbps = 2350, .reg = 0x190 },
	{ .mbps = 2400, .reg = 0x198 },
	{ .mbps = 2450, .reg = 0x1A1 },
	{ .mbps = 2500, .reg = 0x1A9 },
	{ /* sentinel */ },
};

/* PHY ESC Error Monitor */
#define PHEERM_REG			0x74

/* PHY Clock Lane Monitor */
#define PHCLM_REG			0x78
#define PHCLM_STOPSTATECKL		BIT(0)

/* PHY Data Lane Monitor */
#define PHDLM_REG			0x7c

/* CSI0CLK Frequency Configuration Preset Register */
#define CSI0CLKFCPR_REG			0x260
#define CSI0CLKFREQRANGE(n)		((n & 0x3f) << 16)

struct rcar_csi2_format {
	u32 code;
	unsigned int datatype;
	unsigned int bpp;
};

static const struct rcar_csi2_format rcar_csi2_formats[] = {
	{ .code = MEDIA_BUS_FMT_RGB888_1X24,	.datatype = 0x24, .bpp = 24 },
	{ .code = MEDIA_BUS_FMT_UYVY8_1X16,	.datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_YUYV8_1X16,	.datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_UYVY8_2X8,	.datatype = 0x1e, .bpp = 16 },
	{ .code = MEDIA_BUS_FMT_YUYV10_2X10,	.datatype = 0x1e, .bpp = 20 },
	{ .code = MEDIA_BUS_FMT_Y10_1X10,	.datatype = 0x2b, .bpp = 10 },
	{ .code = MEDIA_BUS_FMT_SBGGR8_1X8,     .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SGBRG8_1X8,     .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SGRBG8_1X8,     .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_SRGGB8_1X8,     .datatype = 0x2a, .bpp = 8 },
	{ .code = MEDIA_BUS_FMT_Y8_1X8,		.datatype = 0x2a, .bpp = 8 },
};

#define	ABC		0x0
#define	CBA		0x1
#define	ACB		0x2
#define	CAB		0x3
#define	BAC		0x4
#define	BCA		0x5

/* RX ABC Order */
struct rcar_csi2_pin_swap {
	u8 code;
	u32 rw_cfg0_b2_0;
	u32 rw_cfg0_b3;
	u32 afe_clane_29_b8;
};

static const struct rcar_csi2_pin_swap rcar_csi2_pin_swaps[] = {
	{ .code = ABC, .rw_cfg0_b2_0 = 0x0, .rw_cfg0_b3 = 0x0, .afe_clane_29_b8 = 0x0 },
	{ .code = CBA, .rw_cfg0_b2_0 = 0x1, .rw_cfg0_b3 = 0x1, .afe_clane_29_b8 = 0x1 },
	{ .code = ACB, .rw_cfg0_b2_0 = 0x2, .rw_cfg0_b3 = 0x1, .afe_clane_29_b8 = 0x1 },
	{ .code = CAB, .rw_cfg0_b2_0 = 0x3, .rw_cfg0_b3 = 0x0, .afe_clane_29_b8 = 0x0 },
	{ .code = BAC, .rw_cfg0_b2_0 = 0x4, .rw_cfg0_b3 = 0x1, .afe_clane_29_b8 = 0x1 },
	{ .code = BCA, .rw_cfg0_b2_0 = 0x5, .rw_cfg0_b3 = 0x0, .afe_clane_29_b8 = 0x0 },
};

struct rcar_csi2_cphy_specific {
	u8 trio;
	unsigned int hs_receive_reg;
	unsigned int pin_swap_reg;
	unsigned int ctrl27_reg;
	unsigned int rwconf_reg;
};

static const struct rcar_csi2_cphy_specific cphy_specific_reg[] = {
	{ .trio = 0,
		.hs_receive_reg = CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2(9),
		.pin_swap_reg = CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2(9),
		.ctrl27_reg = CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2(7),
		.rwconf_reg = CORE_DIG_CLANE_0_RW_CFG_0 },
	{ .trio = 1,
		.hs_receive_reg = CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2(9),
		.pin_swap_reg = CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2(9),
		.ctrl27_reg = CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2(7),
		.rwconf_reg = CORE_DIG_CLANE_1_RW_CFG_0 },
	{ .trio = 2,
		.hs_receive_reg = CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2(9),
		.pin_swap_reg = CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2(9),
		.ctrl27_reg = CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2(7),
		.rwconf_reg = CORE_DIG_CLANE_2_RW_CFG_0 },
};

static const struct rcar_csi2_format *rcsi2_code_to_fmt(unsigned int code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(rcar_csi2_formats); i++)
		if (rcar_csi2_formats[i].code == code)
			return &rcar_csi2_formats[i];

	return NULL;
}

enum rcar_csi2_pads {
	RCAR_CSI2_SINK,
	RCAR_CSI2_SOURCE_VC0,
	RCAR_CSI2_SOURCE_VC1,
	RCAR_CSI2_SOURCE_VC2,
	RCAR_CSI2_SOURCE_VC3,
	NR_OF_RCAR_CSI2_PAD,
};

struct rcar_csi2_info {
	int (*init_phtw)(struct rcar_csi2 *priv, unsigned int mbps);
	int (*phy_post_init)(struct rcar_csi2 *priv);
	const struct rcsi2_mbps_reg *hsfreqrange;
	unsigned int csi0clkfreqrange;
	unsigned int num_channels;
	unsigned int features;
	bool clear_ulps;
	bool no_use_vdt;
	bool has_phyfrx_reg;
	const struct rcsi2_mbps_reg *osc_freq_target;
};

struct rcar_csi2 {
	struct device *dev;
	void __iomem *base;
	const struct rcar_csi2_info *info;
	struct reset_control *rstc;

	struct v4l2_subdev subdev;
	struct media_pad pads[NR_OF_RCAR_CSI2_PAD];

	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *remote;
	unsigned int remote_pad;

	struct v4l2_mbus_framefmt mf;

	struct mutex lock;
	int stream_count;

	unsigned short lanes;
	unsigned char lane_swap[4];

	bool cphy_connection;
	bool pin_swap;
	unsigned int pin_swap_rx_order[4];
	unsigned int hs_receive_eq[4];
#ifdef CONFIG_VIDEO_SNPS_CSI2_CAMERA
	struct csi2cam *cam;
#endif
};

static int rcsi2_phtw_write_array(struct rcar_csi2 *priv,
				  const struct phtw_value *values);

static inline struct rcar_csi2 *sd_to_csi2(struct v4l2_subdev *sd)
{
	return container_of(sd, struct rcar_csi2, subdev);
}

static inline struct rcar_csi2 *notifier_to_csi2(struct v4l2_async_notifier *n)
{
	return container_of(n, struct rcar_csi2, notifier);
}

static u32 rcsi2_read(struct rcar_csi2 *priv, unsigned int reg)
{
	return ioread32(priv->base + reg);
}

static void rcsi2_write(struct rcar_csi2 *priv, unsigned int reg, u32 data)
{
	iowrite32(data, priv->base + reg);
}

static void rcsi2_modify(struct rcar_csi2 *priv, unsigned int reg, u32 data, u32 mask)
{
	u32 val;

	val = rcsi2_read(priv, reg);
	val &= ~mask;
	val |= data;
	rcsi2_write(priv, reg, val);
}

static u16 rcsi2_read16(struct rcar_csi2 *priv, unsigned int reg)
{
	return ioread16(priv->base + reg);
}

static void rcsi2_write16(struct rcar_csi2 *priv, unsigned int reg, u16 data)
{
	iowrite16(data, priv->base + reg);
}

static void rcsi2_modify16(struct rcar_csi2 *priv, unsigned int reg, u16 data, u16 mask)
{
	u16 val;

	val = rcsi2_read16(priv, reg);
	val &= ~mask;
	val |= data;
	rcsi2_write16(priv, reg, val);
}

static void rcsi2_enter_standby(struct rcar_csi2 *priv)
{
	if (!(priv->info->features & RCAR_VIN_R8A779G0_FEATURE)) {
		rcsi2_write(priv, PHYCNT_REG, 0);
		rcsi2_write(priv, PHTC_REG, PHTC_TESTCLR);
	}

	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE))
		reset_control_assert(priv->rstc);

	usleep_range(100, 150);
	pm_runtime_put(priv->dev);
}

static void rcsi2_exit_standby(struct rcar_csi2 *priv)
{
	pm_runtime_get_sync(priv->dev);
	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE))
		reset_control_deassert(priv->rstc);
}

static int rcsi2_wait_phy_start(struct rcar_csi2 *priv,
				unsigned int lanes)
{
	unsigned int timeout;

	if (priv->info->features & RCAR_VIN_R8A78000_FEATURE)
		return 0;

	/* Wait for the clock and data lanes to enter LP-11 state. */
	for (timeout = 0; timeout <= 20; timeout++) {
		const u32 lane_mask = (1 << lanes) - 1;

		if ((rcsi2_read(priv, PHCLM_REG) & PHCLM_STOPSTATECKL)  &&
			(rcsi2_read(priv, PHDLM_REG) & lane_mask) == lane_mask)
			return 0;

		usleep_range(1000, 2000);
	}

	dev_err(priv->dev, "Timeout waiting for LP-11 state\n");

	return -ETIMEDOUT;
}

static int rcsi2_set_phypll(struct rcar_csi2 *priv, unsigned int mbps)
{
	const struct rcsi2_mbps_reg *hsfreq;
	const struct rcsi2_mbps_reg *hsfreq_prev = NULL;

	if (mbps < priv->info->hsfreqrange->mbps)
		dev_warn(priv->dev, "%u Mbps less than min PHY speed %u Mbps",
			 mbps, priv->info->hsfreqrange->mbps);

	for (hsfreq = priv->info->hsfreqrange; hsfreq->mbps != 0; hsfreq++) {
		if (hsfreq->mbps >= mbps)
			break;
		hsfreq_prev = hsfreq;
	}

	if (!hsfreq->mbps) {
		dev_err(priv->dev, "Unsupported PHY speed (%u Mbps)", mbps);
		return -ERANGE;
	}

	if (hsfreq_prev &&
	    ((mbps - hsfreq_prev->mbps) <= (hsfreq->mbps - mbps)))
		hsfreq = hsfreq_prev;

	if (priv->info->features & RCAR_VIN_R8A779H0_FEATURE)
		rcsi2_write(priv, V4M_PHYPLL, PHYPLL_HSFREQRANGE(hsfreq->reg));
	else
		rcsi2_write(priv, PHYPLL_REG, PHYPLL_HSFREQRANGE(hsfreq->reg));

	return 0;
}

static u16 rcsi2_get_osc_freq(struct rcar_csi2 *priv, unsigned int mbps)
{
	const struct rcsi2_mbps_reg *osc_freq;
	const struct rcsi2_mbps_reg *osc_freq_prev = NULL;

	if (mbps < priv->info->osc_freq_target->mbps)
		dev_warn(priv->dev, "%u Mbps less than min PHY speed %u Mbps",
			 mbps, priv->info->osc_freq_target->mbps);

	for (osc_freq = priv->info->osc_freq_target; osc_freq->mbps != 0; osc_freq++) {
		if (osc_freq->mbps >= mbps)
			break;
		osc_freq_prev = osc_freq;
	}

	if (!osc_freq->mbps) {
		dev_err(priv->dev, "Unsupported PHY speed (%u Mbps)", mbps);
		return -ERANGE;
	}

	if (osc_freq_prev &&
	    ((mbps - osc_freq_prev->mbps) <= (osc_freq->mbps - mbps)))
		osc_freq = osc_freq_prev;

	return osc_freq->reg;
}

static int rcsi2_calc_mbps(struct rcar_csi2 *priv, unsigned int bpp,
			   unsigned int lanes)
{
	struct v4l2_subdev *source;
	struct v4l2_ctrl *ctrl;
	u64 mbps;

	mbps = 7423000000;
	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE)) {
		if (!priv->remote)
			return -ENODEV;

		source = priv->remote;

		/* Read the pixel rate control from remote. */
		ctrl = v4l2_ctrl_find(source->ctrl_handler, V4L2_CID_PIXEL_RATE);
		if (!ctrl) {
			dev_err(priv->dev, "no pixel rate control in subdev %s\n",
				source->name);
			return -EINVAL;
		}

		/*
		 * Calculate the phypll in mbps.
		 * link_freq = (pixel_rate * bits_per_sample) / (2 * nr_of_lanes)
		 * bps = link_freq * 2
		 */
		mbps = v4l2_ctrl_g_ctrl_int64(ctrl) * bpp;
	}
	do_div(mbps, lanes * 1000000);

	return mbps;
}

static int rcsi2_get_active_lanes(struct rcar_csi2 *priv,
				  unsigned int *lanes)
{
	struct v4l2_mbus_config mbus_config = { 0 };
	unsigned int num_lanes = UINT_MAX;
	int ret;

	*lanes = priv->lanes;

	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE)) {
		ret = v4l2_subdev_call(priv->remote, pad, get_mbus_config,
								priv->remote_pad, &mbus_config);
		if (ret == -ENOIOCTLCMD) {
			dev_dbg(priv->dev, "No remote mbus configuration available\n");
			return 0;
		}

		if (ret) {
			dev_err(priv->dev, "Failed to get remote mbus configuration\n");
			return ret;
		}

		if (mbus_config.type != V4L2_MBUS_CSI2_DPHY &&
			mbus_config.type != V4L2_MBUS_CSI2_CPHY) {
			dev_err(priv->dev, "Unsupported media bus type %u\n",
				mbus_config.type);
			return -EINVAL;
		}

		if (mbus_config.flags & V4L2_MBUS_CSI2_1_LANE)
			num_lanes = 1;
		else if (mbus_config.flags & V4L2_MBUS_CSI2_2_LANE)
			num_lanes = 2;
		else if (mbus_config.flags & V4L2_MBUS_CSI2_3_LANE)
			num_lanes = 3;
		else if (mbus_config.flags & V4L2_MBUS_CSI2_4_LANE)
			num_lanes = 4;

		if (num_lanes > priv->lanes) {
			dev_err(priv->dev,
				"Unsupported mbus config: too many data lanes %u\n",
				num_lanes);
			return -EINVAL;
		}

		*lanes = num_lanes;
	}

	return 0;
}

static int rcsi2_start_receiver(struct rcar_csi2 *priv)
{
	const struct rcar_csi2_format *format;
	u32 phycnt, vcdt = 0, vcdt2 = 0, fld = 0;
	unsigned int lanes;
	unsigned int i;
	int mbps, ret;

	dev_dbg(priv->dev, "Input size (%ux%u%c)\n",
		priv->mf.width, priv->mf.height,
		priv->mf.field == V4L2_FIELD_NONE ? 'p' : 'i');

	/* Code is validated in set_fmt. */
	format = rcsi2_code_to_fmt(priv->mf.code);
	if (!format)
		return -EINVAL;

	/*
	 * Enable all supported CSI-2 channels with virtual channel and
	 * data type matching.
	 *
	 * NOTE: It's not possible to get individual datatype for each
	 *       source virtual channel. Once this is possible in V4L2
	 *       it should be used here.
	 */
	for (i = 0; i < priv->info->num_channels; i++) {
		u32 vcdt_part;

		vcdt_part = VCDT_SEL_VC(i) | VCDT_VCDTN_EN | VCDT_SEL_DTN_ON |
			VCDT_SEL_DT(format->datatype);

		/* Store in correct reg and offset. */
		if (i < 2)
			vcdt |= vcdt_part << ((i % 2) * 16);
		else
			vcdt2 |= vcdt_part << ((i % 2) * 16);
	}

	if (priv->mf.field == V4L2_FIELD_ALTERNATE) {
		fld = FLD_DET_SEL(1) | FLD_FLD_EN4 | FLD_FLD_EN3 | FLD_FLD_EN2
			| FLD_FLD_EN;

		if (priv->mf.height == 240)
			fld |= FLD_FLD_NUM(0);
		else
			fld |= FLD_FLD_NUM(1);
	}

	/*
	 * Get the number of active data lanes inspecting the remote mbus
	 * configuration.
	 */
	ret = rcsi2_get_active_lanes(priv, &lanes);
	if (ret)
		return ret;

	phycnt = PHYCNT_ENABLECLK;
	phycnt |= (1 << lanes) - 1;

	mbps = rcsi2_calc_mbps(priv, format->bpp, lanes);
	if (mbps < 0)
		return mbps;

	/* Enable interrupts. */
	rcsi2_write(priv, INTEN_REG, INTEN_INT_AFIFO_OF | INTEN_INT_ERRSOTHS
		    | INTEN_INT_ERRSOTSYNCHS);

	/* Init */
	rcsi2_write(priv, TREF_REG, TREF_TREF);
	rcsi2_write(priv, PHTC_REG, 0);

	/* Configure */
	if (!priv->info->no_use_vdt) {
		rcsi2_write(priv, VCDT_REG, vcdt);
		if (vcdt2)
			rcsi2_write(priv, VCDT2_REG, vcdt2);
	}

	/* Lanes are zero indexed. */
	rcsi2_write(priv, LSWAP_REG,
		    LSWAP_L0SEL(priv->lane_swap[0] - 1) |
		    LSWAP_L1SEL(priv->lane_swap[1] - 1) |
		    LSWAP_L2SEL(priv->lane_swap[2] - 1) |
		    LSWAP_L3SEL(priv->lane_swap[3] - 1));

	/* Start */
	if (priv->info->init_phtw) {
		ret = priv->info->init_phtw(priv, mbps);
		if (ret)
			return ret;
	}

	if (priv->info->hsfreqrange) {
		ret = rcsi2_set_phypll(priv, mbps);
		if (ret)
			return ret;
	}

	if (priv->info->csi0clkfreqrange)
		rcsi2_write(priv, CSI0CLKFCPR_REG,
			    CSI0CLKFREQRANGE(priv->info->csi0clkfreqrange));

	if (priv->info->has_phyfrx_reg)
		rcsi2_write(priv, PHYFRX_REG, PHYFRX_FORCERX_MODE_3 |
					      PHYFRX_FORCERX_MODE_2 |
					      PHYFRX_FORCERX_MODE_1 |
					      PHYFRX_FORCERX_MODE_0);

	rcsi2_write(priv, PHYCNT_REG, phycnt);
	rcsi2_write(priv, LINKCNT_REG, LINKCNT_MONITOR_EN |
		    LINKCNT_REG_MONI_PACT_EN | LINKCNT_ICLK_NONSTOP);
	rcsi2_write(priv, FLD_REG, fld);
	rcsi2_write(priv, PHYCNT_REG, phycnt | PHYCNT_SHUTDOWNZ);
	rcsi2_write(priv, PHYCNT_REG, phycnt | PHYCNT_SHUTDOWNZ | PHYCNT_RSTZ);

	ret = rcsi2_wait_phy_start(priv, lanes);
	if (ret)
		return ret;

	if (priv->info->has_phyfrx_reg)
		rcsi2_write(priv, PHYFRX_REG, 0);

	/* Run post PHY start initialization, if needed. */
	if (priv->info->phy_post_init) {
		ret = priv->info->phy_post_init(priv);
		if (ret)
			return ret;
	}

	/* Clear Ultra Low Power interrupt. */
	if (priv->info->clear_ulps)
		rcsi2_write(priv, INTSTATE_REG,
			    INTSTATE_INT_ULPS_START |
			    INTSTATE_INT_ULPS_END);
	return 0;
}

static int rcsi2_c_phy_setting(struct rcar_csi2 *priv, int data_rate)
{
	const struct rcsi2_cphy_setting *cphy_setting_value;
	unsigned int timeout, i, j;
	u32 status;
	u16 val;

	for ( cphy_setting_value = cphy_setting_table_r8a779g0;
			cphy_setting_value->msps != 0; cphy_setting_value++ ) {
		if (cphy_setting_value->msps > data_rate)
			break;
	}

	if (!cphy_setting_value->msps) {
		dev_err(priv->dev, "Unsupported PHY speed for mpsp setting (%u Msps)", data_rate);
		return -ERANGE;
	}

	/* C-PHY specific */
	rcsi2_write16(priv, CORE_DIG_RW_COMMON(7), 0x0155);
	rcsi2_write16(priv, PPI_STARTUP_RW_COMMON_DPHY(7), 0x0068);
	rcsi2_write16(priv, PPI_STARTUP_RW_COMMON_DPHY(8), 0x0010);

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_LP_0, 0x463C);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_LP_0, 0x463C);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_LP_0, 0x463C);

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_HS_RX(0), 0x0195);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_HS_RX(0), 0x0195);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_HS_RX(0), 0x0195);

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_HS_RX(1), 0x0013);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_HS_RX(1), 0x0013);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_HS_RX(1), 0x0013);

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_HS_RX(5), 0x0013);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_HS_RX(5), 0x0013);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_HS_RX(5), 0x0013);

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_HS_RX(6), 0x000A);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_HS_RX(6), 0x000A);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_HS_RX(6), 0x000A);

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_HS_RX(2), cphy_setting_value->rw_hs_rx_2);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_HS_RX(2), cphy_setting_value->rw_hs_rx_2);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_HS_RX(2), cphy_setting_value->rw_hs_rx_2);

	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2(2), 0x0001);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2(2), 0);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2(2), 0x0001);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2(2), 0x0001);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2(2), 0);

	rcsi2_write16(priv, CORE_DIG_RW_TRIO0(0), 0x044A);
	rcsi2_write16(priv, CORE_DIG_RW_TRIO1(0), 0x044A);
	rcsi2_write16(priv, CORE_DIG_RW_TRIO2(0), 0x044A);

	/* Write value from LUT to CORE_DIG_RW_TRIO[0-2]_2, [7:0] */
	rcsi2_modify16(priv, CORE_DIG_RW_TRIO0(2),
		       cphy_setting_value->rw_trio_2, GENMASK(7, 0));
	rcsi2_modify16(priv, CORE_DIG_RW_TRIO1(2),
		       cphy_setting_value->rw_trio_2, GENMASK(7, 0));
	rcsi2_modify16(priv, CORE_DIG_RW_TRIO2(2),
		       cphy_setting_value->rw_trio_2, GENMASK(7, 0));

	rcsi2_write16(priv, CORE_DIG_RW_TRIO0(1), cphy_setting_value->rw_trio_1);
	rcsi2_write16(priv, CORE_DIG_RW_TRIO1(1), cphy_setting_value->rw_trio_1);
	rcsi2_write16(priv, CORE_DIG_RW_TRIO2(1), cphy_setting_value->rw_trio_1);

	/* Write value from LUT to CORE_DIG_RW_TRIO[0-2]_0, [11:9] */
	rcsi2_modify16(priv, CORE_DIG_RW_TRIO0(0),
		       cphy_setting_value->rw_trio_0, GENMASK(11, 9));
	rcsi2_modify16(priv, CORE_DIG_RW_TRIO1(0),
		       cphy_setting_value->rw_trio_0, GENMASK(11, 9));
	rcsi2_modify16(priv, CORE_DIG_RW_TRIO2(0),
		       cphy_setting_value->rw_trio_0, GENMASK(11, 9));

	rcsi2_write16(priv, CORE_DIG_CLANE_0_RW_LP_0, 0x163C);
	rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_LP_0, 0x163C);
	rcsi2_write16(priv, CORE_DIG_CLANE_2_RW_LP_0, 0x163C);

	for (i = 0; i < ARRAY_SIZE(cphy_specific_reg); i++) {
		val = cphy_setting_value->afe_lane0_29;
		val |= priv->hs_receive_eq[i];
		rcsi2_modify16(priv, cphy_specific_reg[i].hs_receive_reg, val,
			       GENMASK(4, 0));
		val = cphy_setting_value->afe_lane0_27;
		rcsi2_modify16(priv, cphy_specific_reg[i].ctrl27_reg, val, GENMASK(12, 10));
	}

	if (priv->pin_swap) {
		/* For WhiteHawk board */
		rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_CFG_0, 0xf5);
		rcsi2_write16(priv, CORE_DIG_CLANE_1_RW_HS_TX_6, 0x5000);
		for (i = 0; i < ARRAY_SIZE(cphy_specific_reg); i++) {
			val = rcar_csi2_pin_swaps[i].afe_clane_29_b8 << 8;
			rcsi2_modify16(priv, cphy_specific_reg[i].pin_swap_reg, val, GENMASK(8, 8));
			for (j = 0; j < ARRAY_SIZE(rcar_csi2_pin_swaps); j++) {
				if (priv->pin_swap_rx_order[i] == rcar_csi2_pin_swaps[j].code) {
					val = rcar_csi2_pin_swaps[j].rw_cfg0_b2_0;
					val |= rcar_csi2_pin_swaps[j].rw_cfg0_b3 << 3;
					rcsi2_modify16(priv, cphy_specific_reg[i].rwconf_reg, val,
						       GENMASK(3, 0));
				}
			}
		}
	}

	/* Step T4: Leave Shutdown mode */
	rcsi2_write(priv, DPHY_RSTZ, BIT(0));
	rcsi2_write(priv, PHY_SHUTDOWNZ, BIT(0));

	/* Step T5: wating for calibration */
	for (timeout = 10; timeout > 0; --timeout) {
		status = rcsi2_read(priv, ST_PHYST);
		if (status & ST_PHY_READY)
			break;
		usleep_range(1000, 2000);
	}

	if (!timeout) {
		dev_err(priv->dev, "PHY calibration failed\n");
		return -ETIMEDOUT;
	}

	/* Step T6: C-PHY setting - analog programing*/
	/* Fix me */

	return 0;
}

/* V4M D-PHY*/
static int rcsi2_d_phy_setting(struct rcar_csi2 *priv, int data_rate)
{
	unsigned int timeout, ret;
	u32 status;

	static const struct phtw_value step5[] = {
		{ .data = 0x00, .code = 0x00 },		/* H'0100_0100 */
		{ .data = 0x00, .code = 0x1E },		/* H'0000_011E */
		{ /* sentinel */ },
	};

	/* T4: Set PHY_SHUTDOWNZ/phy_shutdownz and DPHY_RSTZ/dphy_rstz = 1'b1. */
	rcsi2_write(priv, DPHY_RSTZ, BIT(0));
	rcsi2_write(priv, PHY_SHUTDOWNZ, BIT(0));

	/* T5: Internal calibrations ongoing */
	ret = rcsi2_phtw_write_array(priv, step5);
	if (ret)
		return ret;

	/* Wait for PHTR[19:16] = H'7 that POR is complete */
	for (timeout = 10; timeout > 0; --timeout) {
		status = rcsi2_read(priv, V4M_PHTR);
		if ((status & 0xf0000) & 0x70000)
			break;
		usleep_range(1000, 2000);
	}

	if (!timeout) {
		dev_err(priv->dev, "PHY calibration failed\n");
		return -ETIMEDOUT;
	}

	/*T6: (Skip) */

	return 0;
}

static int rcsi2_start_receiver_v4h(struct rcar_csi2 *priv)
{
	const struct rcar_csi2_format *format;
	int data_rate, ret;
	unsigned int lanes;
	u32 read32;

	/* Calculate parameters */
	format = rcsi2_code_to_fmt(priv->mf.code);

	ret = rcsi2_get_active_lanes(priv, &lanes);
	if (ret)
		return ret;

	data_rate = rcsi2_calc_mbps(priv, format->bpp, lanes);
	if (data_rate < 0)
		return data_rate;

	if (priv->cphy_connection)
		do_div(data_rate, 2.8);

	/* Step T0: Reset LINK and PHY*/
	rcsi2_write(priv, CSI2_RESETN, 0);
	rcsi2_write(priv, DPHY_RSTZ, 0);
	rcsi2_write(priv, PHY_SHUTDOWNZ, 0);

	/* Step T1: PHY static setting */
	read32 = rcsi2_read(priv, PHY_EN);
	rcsi2_write(priv, PHY_EN, read32 | (PHY_ENABLE_DCK | PHY_ENABLE_0
				| PHY_ENABLE_1 | PHY_ENABLE_2));
	read32 = rcsi2_read(priv, FRXM);
	rcsi2_write(priv, FRXM, read32 | (FRXM_FORCERXMODE_DCK | FRXM_FORCERXMODE_0
				| FRXM_FORCERXMODE_1 | FRXM_FORCERXMODE_2));
	read32 = rcsi2_read(priv, OVR1);
	rcsi2_write(priv, OVR1, read32 | (OVR1_forcerxmode_dck | OVR1_forcerxmode_0
				| OVR1_forcerxmode_1 | OVR1_forcerxmode_2));
	rcsi2_write(priv, FLDC, 0);
	rcsi2_write(priv, FLDD, 0);
	rcsi2_write(priv, IDIC, 0);
	rcsi2_write(priv, PHY_MODE, BIT(0));
	rcsi2_write(priv, N_LANES, lanes - 1);

	/* Step T2: Reset CSI2 */
	rcsi2_write(priv, CSI2_RESETN, BIT(0));

	/* Step T3: Registers static setting through APB */
	/* Common setting */
	rcsi2_write16(priv, PPI_STARTUP_RW_COMMON_DPHY(10), 0x0030);
	rcsi2_write16(priv, CORE_DIG_ANACTRL_RW_COMMON_ANACTRL(2), 0x1444);
	rcsi2_write16(priv, CORE_DIG_ANACTRL_RW_COMMON_ANACTRL(0), 0x1BFD);
	rcsi2_write16(priv, PPI_STARTUP_RW_COMMON_STARTUP_1_1, 0x0233);
	rcsi2_write16(priv, PPI_STARTUP_RW_COMMON_DPHY(6), 0x0027);
	rcsi2_write16(priv, PPI_CALIBCTRL_RW_COMMON_BG_0, 0x01F4);
	rcsi2_write16(priv, PPI_RW_TERMCAL_CFG_0, 0x0013);
	rcsi2_write16(priv, PPI_RW_OFFSETCAL_CFG_0, 0x0003);
	rcsi2_write16(priv, PPI_RW_LPDCOCAL_TIMEBASE, 0x004F);
	rcsi2_write16(priv, PPI_RW_LPDCOCAL_NREF, 0x0320);
	rcsi2_write16(priv, PPI_RW_LPDCOCAL_NREF_RANGE, 0x000F);
	rcsi2_write16(priv, PPI_RW_LPDCOCAL_TWAIT_CONFIG, 0xFE18);
	rcsi2_write16(priv, PPI_RW_LPDCOCAL_VT_CONFIG, 0x0C3C);
	rcsi2_write16(priv, PPI_RW_LPDCOCAL_COARSE_CFG, 0x0105);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(6), 0x1000);
	rcsi2_write16(priv, PPI_RW_COMMON_CFG, 0x0003);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(0), 0x0000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(1), 0x0400);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(3), 0x41F6);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(0), 0x0000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(3), 0x43F6);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(6), 0x3000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(7), 0x0000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(6), 0x3000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(7), 0x0000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(6), 0x7000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(7), 0x0000);
	rcsi2_write16(priv, CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2(5), 0x4000);

	if (priv->cphy_connection) {
		ret = rcsi2_c_phy_setting(priv, data_rate);
		if (ret) {
			dev_err(priv->dev, "Setting C-PHY failed\n");
		}
	} else {
		ret = rcsi2_d_phy_setting(priv, data_rate);
		if (ret) {
			dev_err(priv->dev, "Setting D-PHY failed\n");
		}
	}

	return 0;
}

static int rcsi2_start_receiver_v4m(struct rcar_csi2 *priv)
{
	const struct rcar_csi2_format *format;
	int data_rate, ret;
	unsigned int lanes;
	u32 read32;

	/* Calculate parameters */
	format = rcsi2_code_to_fmt(priv->mf.code);

	ret = rcsi2_get_active_lanes(priv, &lanes);
	if (ret)
		return ret;

	data_rate = rcsi2_calc_mbps(priv, format->bpp, lanes);
	if (data_rate < 0)
		return data_rate;

	/* Step T0: Reset LINK and PHY*/
	rcsi2_write(priv, CSI2_RESETN, 0);
	rcsi2_write(priv, DPHY_RSTZ, 0);
	rcsi2_write(priv, PHY_SHUTDOWNZ, 0);
	rcsi2_write(priv, PHTC_REG, PHTC_TESTCLR);

	/* Step T1: PHY static setting */
	read32 = rcsi2_read(priv, FRXM);
	rcsi2_write(priv, FRXM, read32 | (FRXM_FORCERXMODE_0
				| FRXM_FORCERXMODE_1 | FRXM_FORCERXMODE_2));
	read32 = rcsi2_read(priv, OVR1);
	rcsi2_write(priv, OVR1, read32 | (OVR1_forcerxmode_0
				| OVR1_forcerxmode_1 | OVR1_forcerxmode_2));
	rcsi2_write(priv, FLDC, 0);
	rcsi2_write(priv, FLDD, 0);
	rcsi2_write(priv, IDIC, 0);

	/* Step T2: Reset CSI2 */
	rcsi2_write(priv, PHTC_REG, 0);
	rcsi2_write(priv, CSI2_RESETN, BIT(0));

	/* Step T3: PHY register is programmed/read with PHY in reset. */
	if (priv->info->init_phtw) {
		ret = priv->info->init_phtw(priv, data_rate);
		if (ret)
			return ret;
	}

	/* V4M has D-PHY only */
	if (!priv->cphy_connection) {
		ret = rcsi2_d_phy_setting(priv, data_rate);
		if (ret)
			dev_err(priv->dev, "Setting D-PHY failed\n");
	} else {
		dev_err(priv->dev, "R-Car V4M does not support C-PHY.\n");
		return -EINVAL;
	}

	return 0;
}

static int rcsi2_start_receiver_x5h(struct rcar_csi2 *priv)
{
	const struct rcar_csi2_format *format;
	int data_rate, ret;
	unsigned int lanes;
	bool bBypassMode = true;
	bool bExcludeSP = false;
	bool bEnableVCFilter = false;
	unsigned int nVCtoFilter = 0;
	bool bEnableDTFilter = false;
	unsigned int nDTtoFilter = 0;

	/* Calculate parameters */
	format = rcsi2_code_to_fmt(priv->mf.code);

	ret = rcsi2_get_active_lanes(priv, &lanes);
	if (ret)
		return ret;

	data_rate = rcsi2_calc_mbps(priv, format->bpp, lanes);
	if (data_rate < 0)
		return data_rate;

	/* 1. Set the csi_hard_rstn signal low and then high to reset the CSI-2 v4 controller. */
	rcsi2_modify(priv, PWR_UP, 0x0, PWR_UP_BIT);

	/* 2. PHY Programming */
	rcsi2_modify(priv, PHY_MODE_CFG, priv->cphy_connection ? 1 : 0, PHY_MODE_BIT);
	rcsi2_modify(priv, PHY_MODE_CFG, format->bpp, PPI_WIDTH);

	rcsi2_modify(priv, PHY_DESKEW_CFG, 0x0, DESKEW_SYS_CYCLES);

	/* 3. CSI-2 Programming */

	/* 4. (Optional) CSE programming */

	/* 5. (Optional) IPI Programming */

	/* 6. SDI Programming */
	rcsi2_modify(priv, SDI_CFG, 0x1, SDI_ENABLE);
	rcsi2_modify(priv, SDI_CFG, bBypassMode ? 0 : 1, SDI_ENCODE_MODE);

	rcsi2_modify(priv, SDI_FILTER_CFG, bEnableVCFilter ? 1 : 0, SDI_SELECT_VC_EN);
	rcsi2_modify(priv, SDI_FILTER_CFG, nVCtoFilter, SDI_SELECT_VC);
	rcsi2_modify(priv, SDI_FILTER_CFG, bEnableDTFilter ? 1 : 0, SDI_SELECT_DT_EN);
	rcsi2_modify(priv, SDI_FILTER_CFG, nDTtoFilter, SDI_SELECT_DT);
	rcsi2_modify(priv, SDI_FILTER_CFG, bExcludeSP ? 1 : 0, SDI_EXCLUDE_SP);

	/* 7. Interrupt mask (INT_UNMASK_<group>) programming */
	rcsi2_modify(priv, INT_UNMASK_CSI2, 0x1, UNMASK_HDR_FATAL_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_CSI2, 0x1, UNMASK_HDR_NON_FATAL_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_CSI2, 0x1, UNMASK_INVALID_RX_LENGTH_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_CSI2, 0x1, UNMASK_CRC_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_CSI2, 0x1, UNMASK_INVALID_DT_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_CSI2, 0x1, UNMASK_MAX_N_DATA_IDS_ERR_IRQ);

	rcsi2_modify(priv, INT_UNMASK_FRAME, 0x1, UNMASK_FRAME_BOUNDARY_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_FRAME, 0x1, UNMASK_FRAME_SEQUENCE_ERR_IRQ);

	rcsi2_modify(priv, INT_UNMASK_LINE, 0x1, UNMASK_LINE_BOUNDARY_ERR_IRQ);
	rcsi2_modify(priv, INT_UNMASK_LINE, 0x1, UNMASK_LINE_SEQUENCE_ERR_IRQ);

	/* 8. Assert the PWR_UP[0] to wake-up controller. */
	rcsi2_modify(priv, PWR_UP, 0x1, PWR_UP_BIT);

	return 0;
}

static int rcsi2_wait_phy_start_v4h(struct rcar_csi2 *priv)
{
	unsigned int timeout;
	u32 status;

	/* Step T7: wait for stopstate_N */
	for (timeout = 10; timeout > 0; --timeout) {
		status = rcsi2_read(priv, ST_PHYST);
		if (status & ST_STOPSTATE_0 &&
			status & ST_STOPSTATE_1 &&
			status & ST_STOPSTATE_2 &&
			status & ST_STOPSTATE_DCK)
			return 0;
		usleep_range(1000, 2000);
	}

	return -ETIMEDOUT;
}

static int rcsi2_start(struct rcar_csi2 *priv)
{
	int ret;
	u32 read32;

	/* Start CSI PHY */
	rcsi2_exit_standby(priv);

	if (priv->info->features & RCAR_VIN_R8A779G0_FEATURE)
		/* init V4H PHY */
		ret = rcsi2_start_receiver_v4h(priv);
	else if (priv->info->features & RCAR_VIN_R8A779H0_FEATURE)
		/* init V4M PHY */
		ret = rcsi2_start_receiver_v4m(priv);
	else if (priv->info->features & RCAR_VIN_R8A78000_FEATURE)
		/* init X5H Controller */
		ret = rcsi2_start_receiver_x5h(priv);
	else
		ret = rcsi2_start_receiver(priv);

	if (ret) {
		rcsi2_enter_standby(priv);
			return ret;
	}
	dev_dbg(priv->dev, "Set the Link and PHY of CSI-2 module registers\n");

	/* Start camera side device */
	if (priv->info->features & RCAR_VIN_R8A78000_FEATURE)
		ret = csi2cam_start(priv->cam, priv->mf.width, priv->mf.height, priv->mf.code);
	else
		ret = v4l2_subdev_call(priv->remote, video, s_stream, 1);

	if (ret) {
		rcsi2_enter_standby(priv);
		return ret;
	}

	/* Confirmation of CSI PHY */
	if (priv->info->features & RCAR_VIN_R8A779G0_FEATURE ||
	    priv->info->features & RCAR_VIN_R8A779H0_FEATURE)
		rcsi2_wait_phy_start_v4h(priv);

	/* Step T8: De-assert FRXM */
	if (priv->info->features & RCAR_VIN_R8A779G0_FEATURE) {
		read32 = rcsi2_read(priv, FRXM);
		rcsi2_write(priv, FRXM, read32 & ~(FRXM_FORCERXMODE_DCK | FRXM_FORCERXMODE_0
					| FRXM_FORCERXMODE_1 | FRXM_FORCERXMODE_2));
	} else if (priv->info->features & RCAR_VIN_R8A779H0_FEATURE) {
		read32 = rcsi2_read(priv, FRXM);
		rcsi2_write(priv, FRXM, read32 & ~(FRXM_FORCERXMODE_0
					| FRXM_FORCERXMODE_1 | FRXM_FORCERXMODE_2));
	}
	dev_dbg(priv->dev, "Confirmed PHY of CSI-2 module starts.\n");

	return 0;
}

static void rcsi2_stop(struct rcar_csi2 *priv)
{
	rcsi2_enter_standby(priv);
	if (priv->info->features & RCAR_VIN_R8A78000_FEATURE)
		csi2cam_stop(priv->cam);
	else
		v4l2_subdev_call(priv->remote, video, s_stream, 0);
}

static int rcsi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct rcar_csi2 *priv = sd_to_csi2(sd);
	int ret = 0;

	mutex_lock(&priv->lock);

	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE)) {
		if (!priv->remote) {
			ret = -ENODEV;
			goto out;
		}
	}

	if (enable && priv->stream_count == 0) {
		ret = rcsi2_start(priv);
		if (ret)
			goto out;
	} else if (!enable && priv->stream_count == 1) {
		rcsi2_stop(priv);
	}

	priv->stream_count += enable ? 1 : -1;
out:
	mutex_unlock(&priv->lock);

	return ret;
}

static int rcsi2_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct rcar_csi2 *priv = sd_to_csi2(sd);
	struct v4l2_mbus_framefmt *framefmt;

	if (!rcsi2_code_to_fmt(format->format.code))
		format->format.code = rcar_csi2_formats[0].code;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		priv->mf = format->format;
	} else {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, 0);
		*framefmt = format->format;
	}

	return 0;
}

static int rcsi2_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct rcar_csi2 *priv = sd_to_csi2(sd);

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		format->format = priv->mf;
	else
		format->format = *v4l2_subdev_get_try_format(sd, cfg, 0);

	return 0;
}

static const struct v4l2_subdev_video_ops rcar_csi2_video_ops = {
	.s_stream = rcsi2_s_stream,
};

static const struct v4l2_subdev_pad_ops rcar_csi2_pad_ops = {
	.set_fmt = rcsi2_set_pad_format,
	.get_fmt = rcsi2_get_pad_format,
};

static const struct v4l2_subdev_ops rcar_csi2_subdev_ops = {
	.video	= &rcar_csi2_video_ops,
	.pad	= &rcar_csi2_pad_ops,
};

static irqreturn_t rcsi2_irq(int irq, void *data)
{
	struct rcar_csi2 *priv = data;
	u32 status, err_status;

	status = rcsi2_read(priv, INTSTATE_REG);
	err_status = rcsi2_read(priv, INTERRSTATE_REG);

	if (!status)
		return IRQ_HANDLED;

	rcsi2_write(priv, INTSTATE_REG, status);

	if (!err_status)
		return IRQ_HANDLED;

	rcsi2_write(priv, INTERRSTATE_REG, err_status);

	dev_info(priv->dev, "Transfer error, restarting CSI-2 receiver\n");

	return IRQ_WAKE_THREAD;
}

static irqreturn_t rcsi2_irq_thread(int irq, void *data)
{
	struct rcar_csi2 *priv = data;

	mutex_lock(&priv->lock);
	rcsi2_stop(priv);
	usleep_range(1000, 2000);
	if (rcsi2_start(priv))
		dev_warn(priv->dev, "Failed to restart CSI-2 receiver\n");
	mutex_unlock(&priv->lock);

	return IRQ_HANDLED;
}

/* -----------------------------------------------------------------------------
 * Async handling and registration of subdevices and links.
 */

static int rcsi2_notify_bound(struct v4l2_async_notifier *notifier,
			      struct v4l2_subdev *subdev,
			      struct v4l2_async_subdev *asd)
{
	struct rcar_csi2 *priv = notifier_to_csi2(notifier);
	int pad;

	pad = media_entity_get_fwnode_pad(&subdev->entity, asd->match.fwnode,
					  MEDIA_PAD_FL_SOURCE);
	if (pad < 0) {
		dev_err(priv->dev, "Failed to find pad for %s\n", subdev->name);
		return pad;
	}

	priv->remote = subdev;
	priv->remote_pad = pad;

	dev_dbg(priv->dev, "Bound %s pad: %d\n", subdev->name, pad);

	return media_create_pad_link(&subdev->entity, pad,
				     &priv->subdev.entity, 0,
				     MEDIA_LNK_FL_ENABLED |
				     MEDIA_LNK_FL_IMMUTABLE);
}

static void rcsi2_notify_unbind(struct v4l2_async_notifier *notifier,
				struct v4l2_subdev *subdev,
				struct v4l2_async_subdev *asd)
{
	struct rcar_csi2 *priv = notifier_to_csi2(notifier);

	priv->remote = NULL;

	dev_dbg(priv->dev, "Unbind %s\n", subdev->name);
}

static const struct v4l2_async_notifier_operations rcar_csi2_notify_ops = {
	.bound = rcsi2_notify_bound,
	.unbind = rcsi2_notify_unbind,
};

static int rcsi2_parse_v4l2(struct rcar_csi2 *priv,
			    struct v4l2_fwnode_endpoint *vep)
{
	unsigned int i;

	/* Only port 0 endpoint 0 is valid. */
	if (vep->base.port || vep->base.id)
		return -ENOTCONN;

	if (vep->bus_type != V4L2_MBUS_CSI2_DPHY &&
		vep->bus_type != V4L2_MBUS_CSI2_CPHY) {
		dev_err(priv->dev, "Unsupported bus: %u\n", vep->bus_type);
		return -EINVAL;
	}

	priv->lanes = vep->bus.mipi_csi2.num_data_lanes;
	if (vep->bus_type == V4L2_MBUS_CSI2_DPHY) {
		if (priv->lanes != 1 && priv->lanes != 2 && priv->lanes != 4) {
			dev_err(priv->dev, "Unsupported number of data-lanes: %u\n",
				priv->lanes);
			return -EINVAL;
		}
		priv->cphy_connection = false;
	} else {
		if (priv->lanes != 3) {
			dev_err(priv->dev, "Unsupported number of data-lanes: %u\n",
				priv->lanes);
			return -EINVAL;
		}
		priv->cphy_connection = true;
	}

	for (i = 0; i < ARRAY_SIZE(priv->lane_swap); i++) {
		priv->lane_swap[i] = i < priv->lanes ?
			vep->bus.mipi_csi2.data_lanes[i] : i;

		/* Check for valid lane number. */
		if (priv->lane_swap[i] < 1 || priv->lane_swap[i] > 4) {
			dev_err(priv->dev, "data-lanes must be in 1-4 range\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int rcsi2_parse_dt(struct rcar_csi2 *priv)
{
	struct v4l2_async_subdev *asd;
	struct fwnode_handle *fwnode;
	struct device_node *ep;
	struct v4l2_fwnode_endpoint v4l2_ep = { .bus_type = 0 };
	int ret, rval, i;
	unsigned int hs_arr[4], order_arr[4];
	struct device_node *remote_ep;
	struct platform_device *pdev;

	if (of_find_property(priv->dev->of_node, "pin-swap", NULL))
		priv->pin_swap = true;
	else
		priv->pin_swap = false;

	ep = of_graph_get_endpoint_by_regs(priv->dev->of_node, 0, 0);
	if (!ep) {
		dev_dbg(priv->dev, "Not connected to subdevice\n");
		return 0;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep), &v4l2_ep);
	if (ret) {
		dev_err(priv->dev, "Could not parse v4l2 endpoint\n");
		of_node_put(ep);
		return -EINVAL;
	}

	ret = rcsi2_parse_v4l2(priv, &v4l2_ep);
	if (ret) {
		of_node_put(ep);
		return ret;
	}

	if (of_find_property(ep, "hs-receive-eq", NULL)) {
		rval = of_property_read_u32_array(ep, "hs-receive-eq", hs_arr, priv->lanes);
		if (rval) {
			dev_err(priv->dev, "Failed to read hs-receive-eq\n");
			return rval;
		}
		for (i = 0; i < priv->lanes; i++)
			priv->hs_receive_eq[i] = hs_arr[i];
	} else {
		/* Witout pin-swap-rx-order, ABC is default order */
		for (i = 0; i < priv->lanes; i++)
			priv->hs_receive_eq[i] = 0x4;
	}

	if (of_find_property(ep, "pin-swap-rx-order", NULL)) {
		rval = of_property_read_u32_array(ep, "pin-swap-rx-order", order_arr, priv->lanes);
		if (rval) {
			dev_err(priv->dev, "Failed to read pin-swap-rx-order\n");
			return rval;
		}
		for (i = 0; i < priv->lanes; i++)
			priv->pin_swap_rx_order[i] = order_arr[i];
	} else {
		/* Without pin-swap-rx-order, ABC is default order */
		for (i = 0; i < priv->lanes; i++)
			priv->pin_swap_rx_order[i] = ABC;
	}

	if (priv->info->features & RCAR_VIN_R8A78000_FEATURE) {
		/* Synopsys CSI-2 Camera */
		remote_ep = of_graph_get_remote_node(priv->dev->of_node, 0, 0);
		if (!remote_ep) {
			pr_err("Failed to find remote endpoint in the device tree\n");
			return -ENODEV;
		}
		dev_dbg(priv->dev, "Found '%pOF'\n", remote_ep);

		pdev = of_find_device_by_node(remote_ep);
		of_node_put(remote_ep);
		if (!pdev)
			return -ENOMEM;

		priv->cam = platform_get_drvdata(pdev);
		platform_device_put(pdev);
	} else {
		fwnode = fwnode_graph_get_remote_endpoint(of_fwnode_handle(ep));
		of_node_put(ep);

		dev_dbg(priv->dev, "Found '%pOF'\n", to_of_node(fwnode));

		v4l2_async_notifier_init(&priv->notifier);
		priv->notifier.ops = &rcar_csi2_notify_ops;

		asd = v4l2_async_notifier_add_fwnode_subdev(&priv->notifier, fwnode,
								sizeof(*asd));
		fwnode_handle_put(fwnode);
		if (IS_ERR(asd))
			return PTR_ERR(asd);

		ret = v4l2_async_subdev_notifier_register(&priv->subdev,
							  &priv->notifier);
		if (ret)
			v4l2_async_notifier_cleanup(&priv->notifier);
	}

	return ret;
}

/* -----------------------------------------------------------------------------
 * PHTW initialization sequences.
 *
 * NOTE: Magic values are from the datasheet and lack documentation.
 */

static int rcsi2_phtw_write(struct rcar_csi2 *priv, u16 data, u16 code)
{
	unsigned int timeout;

	rcsi2_write(priv, PHTW_REG,
		    PHTW_DWEN | PHTW_TESTDIN_DATA(data) |
		    PHTW_CWEN | PHTW_TESTDIN_CODE(code));

	if (priv->info->features & RCAR_VIN_R8A78000_FEATURE)
		return 0;

	/* Wait for DWEN and CWEN to be cleared by hardware. */
	for (timeout = 0; timeout <= 20; timeout++) {
		if (!(rcsi2_read(priv, PHTW_REG) & (PHTW_DWEN | PHTW_CWEN)))
			return 0;

		usleep_range(1000, 2000);
	}

	dev_err(priv->dev, "Timeout waiting for PHTW_DWEN and/or PHTW_CWEN\n");

	return -ETIMEDOUT;
}

static int rcsi2_phtw_write_array(struct rcar_csi2 *priv,
				  const struct phtw_value *values)
{
	const struct phtw_value *value;
	int ret;

	for (value = values; value->data || value->code; value++) {
		ret = rcsi2_phtw_write(priv, value->data, value->code);
		if (ret)
			return ret;
	}

	return 0;
}

static int rcsi2_phtw_write_mbps(struct rcar_csi2 *priv, unsigned int mbps,
				 const struct rcsi2_mbps_reg *values, u16 code)
{
	const struct rcsi2_mbps_reg *value;
	const struct rcsi2_mbps_reg *prev_value = NULL;

	for (value = values; value->mbps; value++) {
		if (value->mbps >= mbps)
			break;
		prev_value = value;
	}

	if (prev_value &&
	    ((mbps - prev_value->mbps) <= (value->mbps - mbps)))
		value = prev_value;

	if (!value->mbps) {
		dev_err(priv->dev, "Unsupported PHY speed (%u Mbps)", mbps);
		return -ERANGE;
	}

	return rcsi2_phtw_write(priv, value->reg, code);
}

static int __rcsi2_init_phtw_h3_v3h_m3n(struct rcar_csi2 *priv,
					unsigned int mbps)
{
	static const struct phtw_value step1[] = {
		{ .data = 0xcc, .code = 0xe2 },
		{ .data = 0x01, .code = 0xe3 },
		{ .data = 0x11, .code = 0xe4 },
		{ .data = 0x01, .code = 0xe5 },
		{ .data = 0x10, .code = 0x04 },
		{ /* sentinel */ },
	};

	static const struct phtw_value step2[] = {
		{ .data = 0x38, .code = 0x08 },
		{ .data = 0x01, .code = 0x00 },
		{ .data = 0x4b, .code = 0xac },
		{ .data = 0x03, .code = 0x00 },
		{ .data = 0x80, .code = 0x07 },
		{ /* sentinel */ },
	};

	int ret;

	ret = rcsi2_phtw_write_array(priv, step1);
	if (ret)
		return ret;

	if (mbps != 0 && mbps <= 250) {
		ret = rcsi2_phtw_write(priv, 0x39, 0x05);
		if (ret)
			return ret;

		ret = rcsi2_phtw_write_mbps(priv, mbps, phtw_mbps_h3_v3h_m3n,
					    0xf1);
		if (ret)
			return ret;
	}

	return rcsi2_phtw_write_array(priv, step2);
}

static int rcsi2_init_phtw_h3_v3h_m3n(struct rcar_csi2 *priv, unsigned int mbps)
{
	return __rcsi2_init_phtw_h3_v3h_m3n(priv, mbps);
}

static int rcsi2_init_phtw_h3es2(struct rcar_csi2 *priv, unsigned int mbps)
{
	return __rcsi2_init_phtw_h3_v3h_m3n(priv, 0);
}

static int rcsi2_init_phtw_v3m_e3(struct rcar_csi2 *priv, unsigned int mbps)
{
	return rcsi2_phtw_write_mbps(priv, mbps, phtw_mbps_v3m_e3, 0x44);
}

static int rcsi2_phy_post_init_v3m_e3(struct rcar_csi2 *priv)
{
	static const struct phtw_value step1[] = {
		{ .data = 0xee, .code = 0x34 },
		{ .data = 0xee, .code = 0x44 },
		{ .data = 0xee, .code = 0x54 },
		{ .data = 0xee, .code = 0x84 },
		{ .data = 0xee, .code = 0x94 },
		{ /* sentinel */ },
	};

	return rcsi2_phtw_write_array(priv, step1);
}

static int rcsi2_init_phtw_v3u(struct rcar_csi2 *priv,
			       unsigned int mbps)
{
	/* In case of 1500Mbps or less */
	static const struct phtw_value step1[] = {
		{ .data = 0xcc, .code = 0xe2 },
		{ /* sentinel */ },
	};

	static const struct phtw_value step2[] = {
		{ .data = 0x01, .code = 0xe3 },
		{ .data = 0x11, .code = 0xe4 },
		{ .data = 0x01, .code = 0xe5 },
		{ /* sentinel */ },
	};

	/* In case of 1500Mbps or less */
	static const struct phtw_value step3[] = {
		{ .data = 0x38, .code = 0x08 },
		{ /* sentinel */ },
	};

	static const struct phtw_value step4[] = {
		{ .data = 0x01, .code = 0x00 },
		{ .data = 0x4b, .code = 0xac },
		{ .data = 0x03, .code = 0x00 },
		{ .data = 0x80, .code = 0x07 },
		{ /* sentinel */ },
	};

	int ret;

	if (mbps != 0 && mbps <= 1500)
		ret = rcsi2_phtw_write_array(priv, step1);
	else
		ret = rcsi2_phtw_write_mbps(priv, mbps, phtw_mbps_v3u, 0xe2);
	if (ret)
		return ret;

	ret = rcsi2_phtw_write_array(priv, step2);
	if (ret)
		return ret;

	if (mbps != 0 && mbps <= 1500) {
		ret = rcsi2_phtw_write_array(priv, step3);
		if (ret)
			return ret;
	}

	ret = rcsi2_phtw_write_array(priv, step4);
	if (ret)
		return ret;

	return ret;
}

static int rcsi2_init_phtw_v4m(struct rcar_csi2 *priv,
			       unsigned int mbps)
{
	/* pp = osc_freq_target[7:0], q = osc_freq_target[11:8]) */
	static const struct phtw_value step32[] = {
		{ .data = 0x00, .code = 0x00 },		/* H’0100_0100 */
		{ .data = 0x00, .code = 0xE2 },		/* H’H’01_pp_01E2 */
		{ .data = 0x00, .code = 0xE3 },		/* H’010_q_01E3 */
		{ .data = 0x01, .code = 0xE4 },		/* H’0101_01E4 */
		{ /* sentinel */ },
	};

	/* In case of 1500Mbps or less */
	static const struct phtw_value step33[] = {
		{ .data = 0x00, .code = 0x00 },		/* H'0100_0100 */
		{ .data = 0x3C, .code = 0x08 },		/* H'013C_0108 */
		{ /* sentinel */ },
	};

	/* In case of higher than 1500Mbps */
	static const struct phtw_value step36[] = {
		{ .data = 0x00, .code = 0x00 },		/* H’0100_0100 */
		{ .data = 0x80, .code = 0xE0 },		/* H’0180_01E0 */
		{ .data = 0x01, .code = 0xE1 },		/* H’0101_01E1 */
		{ .data = 0x06, .code = 0x00 },		/* H’0106_0100 */
		{ .data = 0x0F, .code = 0x11 },		/* H’010F_0111 */
		{ .data = 0x08, .code = 0x00 },		/* H’0108_0100 */
		{ .data = 0x0F, .code = 0x11 },		/* H’010F_0111 */
		{ .data = 0x0A, .code = 0x00 },		/* H’010A_0100 */
		{ .data = 0x0F, .code = 0x11 },		/* H’010F_0111 */
		{ .data = 0x0C, .code = 0x00 },		/* H’010C_0100 */
		{ .data = 0x0F, .code = 0x11 },		/* H’010F_0111 */
		{ .data = 0x01, .code = 0x00 },		/* H’0101_0100 */
		{ .data = 0x31, .code = 0xAA },		/* H’0131_01AA */
		{ .data = 0x05, .code = 0x00 },		/* H’0105_0100 */
		{ .data = 0x05, .code = 0x09 },		/* H’0105_0109 */
		{ .data = 0x07, .code = 0x00 },		/* H’0107_0100 */
		{ .data = 0x05, .code = 0x09 },		/* H’0105_0109 */
		{ .data = 0x09, .code = 0x00 },		/* H’0109_0100 */
		{ .data = 0x05, .code = 0x09 },		/* H’0105_0109 */
		{ .data = 0x0B, .code = 0x00 },		/* H’010B_0100 */
		{ .data = 0x05, .code = 0x09 },		/* H’0105_0109 */
		{ /* sentinel */ },
	};

	int ret;
	u16 osc_freq;
	const struct phtw_value *value;
	u32 read32;

	/* T3-1: Set PHYPLL/HSFREQRANGE[6:0] */
	if (priv->info->hsfreqrange) {
		ret = rcsi2_set_phypll(priv, mbps);
		if (ret)
			return ret;
	}

	/* T3-2: Configure the appropriate DDL target oscillation frequency */
	if (priv->info->osc_freq_target) {
		osc_freq = rcsi2_get_osc_freq(priv, mbps);
		if (!osc_freq)
			return ret;
	}
	for (value = step32; value->data || value->code; value++) {
		if (value->code == 0xE2)
			ret = rcsi2_phtw_write(priv, V4M_PHTW_DIN_DATA_PP(osc_freq), value->code);
		else if (value->code == 0xE3)
			ret = rcsi2_phtw_write(priv, V4M_PHTW_DIN_DATA_D(osc_freq), value->code);
		else
			ret = rcsi2_phtw_write(priv, value->data, value->code);
		if (ret)
			return ret;
	}

	/* T3-3: (Only used in case the speed is less than or equal to 1.5 Gbps)*/
	if (mbps != 0 && mbps <= 1500) {
		ret = rcsi2_phtw_write_array(priv, step33);
		if (ret)
			return ret;
	}

	/* T3-4: Set CSI0CLKFCPR/csi0clkfreqrange[7:0] = 8'b00001100 */
	if (priv->info->csi0clkfreqrange)
		rcsi2_write(priv, V4M_CSI0CLKFCPR,
			    V4M_CSI0CLKFREQRANGE(priv->info->csi0clkfreqrange));

	/* T3-5: Set PHY_EN/ENABLE_N(N=0,1,2,3) and PHY_EN/ENABLECLK = 1'b1 */
	read32 = rcsi2_read(priv, PHY_EN);
	rcsi2_write(priv, PHY_EN, read32 | (PHY_ENABLE_DCK | PHY_ENABLE_0
				| PHY_ENABLE_1 | PHY_ENABLE_2));

	/* T3-6: (Only used in case the speed is higher than 1.5 Gbps) */
	if (mbps != 0 && mbps > 1500) {
		ret = rcsi2_phtw_write_array(priv, step36);
		if (ret)
			return ret;
	}

	return ret;
}

/* -----------------------------------------------------------------------------
 * Platform Device Driver.
 */

static const struct media_entity_operations rcar_csi2_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int rcsi2_probe_resources(struct rcar_csi2 *priv,
				 struct platform_device *pdev)
{
	struct resource *res;
	int irq, ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(&pdev->dev, irq, rcsi2_irq,
					rcsi2_irq_thread, IRQF_SHARED,
					KBUILD_MODNAME, priv);
	if (ret)
		return ret;

	if (priv->info->features & RCAR_VIN_R8A78000_FEATURE)
		return 0;

	priv->rstc = devm_reset_control_get(&pdev->dev, NULL);
	return PTR_ERR_OR_ZERO(priv->rstc);
}

static const struct rcar_csi2_info rcar_csi2_info_r8a7795 = {
	.init_phtw = rcsi2_init_phtw_h3_v3h_m3n,
	.hsfreqrange = hsfreqrange_h3_v3h_m3n,
	.csi0clkfreqrange = 0x20,
	.num_channels = 4,
	.clear_ulps = true,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a7795es1 = {
	.hsfreqrange = hsfreqrange_m3w_h3es1,
	.num_channels = 4,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a7795es2 = {
	.init_phtw = rcsi2_init_phtw_h3es2,
	.hsfreqrange = hsfreqrange_h3_v3h_m3n,
	.csi0clkfreqrange = 0x20,
	.num_channels = 4,
	.clear_ulps = true,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a7796 = {
	.hsfreqrange = hsfreqrange_m3w_h3es1,
	.num_channels = 4,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a77965 = {
	.init_phtw = rcsi2_init_phtw_h3_v3h_m3n,
	.hsfreqrange = hsfreqrange_h3_v3h_m3n,
	.csi0clkfreqrange = 0x20,
	.num_channels = 4,
	.clear_ulps = true,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a77970 = {
	.init_phtw = rcsi2_init_phtw_v3m_e3,
	.phy_post_init = rcsi2_phy_post_init_v3m_e3,
	.num_channels = 4,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a77980 = {
	.init_phtw = rcsi2_init_phtw_h3_v3h_m3n,
	.hsfreqrange = hsfreqrange_h3_v3h_m3n,
	.csi0clkfreqrange = 0x20,
	.num_channels = 4,
	.clear_ulps = true,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a77990 = {
	.init_phtw = rcsi2_init_phtw_v3m_e3,
	.phy_post_init = rcsi2_phy_post_init_v3m_e3,
	.num_channels = 2,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a779g0 = {
	.features = RCAR_VIN_R8A779G0_FEATURE,
	.num_channels = 16,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a779a0 = {
	.init_phtw = rcsi2_init_phtw_v3u,
	.hsfreqrange = hsfreqrange_v3u,
	.csi0clkfreqrange = 0x20,
	.clear_ulps = true,
	.num_channels = 4,
	.no_use_vdt = true,
	.has_phyfrx_reg = true,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a779h0 = {
	.features = RCAR_VIN_R8A779H0_FEATURE,
	.init_phtw = rcsi2_init_phtw_v4m,
	.hsfreqrange = hsfreqrange_v4m,
	.csi0clkfreqrange = 0x0C,
	.osc_freq_target = osc_freq_target_v4m,
	.num_channels = 16,
};

static const struct rcar_csi2_info rcar_csi2_info_r8a78000 = {
	.features = RCAR_VIN_R8A78000_FEATURE,
	.init_phtw = rcsi2_init_phtw_v3u,
	.hsfreqrange = hsfreqrange_v3u,
	.csi0clkfreqrange = 0x20,
	.clear_ulps = true,
	.num_channels = 4,
	.no_use_vdt = true,
	.has_phyfrx_reg = true,
};

static const struct of_device_id rcar_csi2_of_table[] = {
	{
		.compatible = "renesas,r8a774a1-csi2",
		.data = &rcar_csi2_info_r8a7796,
	},
	{
		.compatible = "renesas,r8a774b1-csi2",
		.data = &rcar_csi2_info_r8a77965,
	},
	{
		.compatible = "renesas,r8a774c0-csi2",
		.data = &rcar_csi2_info_r8a77990,
	},
	{
		.compatible = "renesas,r8a774e1-csi2",
		.data = &rcar_csi2_info_r8a7795,
	},
	{
		.compatible = "renesas,r8a7795-csi2",
		.data = &rcar_csi2_info_r8a7795,
	},
	{
		.compatible = "renesas,r8a7796-csi2",
		.data = &rcar_csi2_info_r8a7796,
	},
	{
		.compatible = "renesas,r8a77961-csi2",
		.data = &rcar_csi2_info_r8a7796,
	},
	{
		.compatible = "renesas,r8a77965-csi2",
		.data = &rcar_csi2_info_r8a77965,
	},
	{
		.compatible = "renesas,r8a77970-csi2",
		.data = &rcar_csi2_info_r8a77970,
	},
	{
		.compatible = "renesas,r8a77980-csi2",
		.data = &rcar_csi2_info_r8a77980,
	},
	{
		.compatible = "renesas,r8a77990-csi2",
		.data = &rcar_csi2_info_r8a77990,
	},
	{
		.compatible = "renesas,r8a779g0-csi2",
		.data = &rcar_csi2_info_r8a779g0,
	},
	{
		.compatible = "renesas,r8a78000-csi2",
		.data = &rcar_csi2_info_r8a78000,
	},
	{
		.compatible = "renesas,r8a779a0-csi2",
		.data = &rcar_csi2_info_r8a779a0,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rcar_csi2_of_table);

static const struct soc_device_attribute r8a7795[] = {
	{
		.soc_id = "r8a7795", .revision = "ES1.*",
		.data = &rcar_csi2_info_r8a7795es1,
	},
	{
		.soc_id = "r8a7795", .revision = "ES2.*",
		.data = &rcar_csi2_info_r8a7795es2,
	},
	{ /* sentinel */ },
};

static int rcsi2_probe(struct platform_device *pdev)
{
	const struct soc_device_attribute *attr;
	struct rcar_csi2 *priv;
	unsigned int i;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info = of_device_get_match_data(&pdev->dev);

	/*
	 * The different ES versions of r8a7795 (H3) behave differently but
	 * share the same compatible string.
	 */
	attr = soc_device_match(r8a7795);
	if (attr)
		priv->info = attr->data;

	priv->dev = &pdev->dev;

	mutex_init(&priv->lock);
	priv->stream_count = 0;

	ret = rcsi2_probe_resources(priv, pdev);
	if (ret) {
		dev_err(priv->dev, "Failed to get resources\n");
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	ret = rcsi2_parse_dt(priv);
	if (ret)
		return ret;

	priv->subdev.owner = THIS_MODULE;
	priv->subdev.dev = &pdev->dev;
	v4l2_subdev_init(&priv->subdev, &rcar_csi2_subdev_ops);
	v4l2_set_subdevdata(&priv->subdev, &pdev->dev);
	snprintf(priv->subdev.name, V4L2_SUBDEV_NAME_SIZE, "%s %s",
		 KBUILD_MODNAME, dev_name(&pdev->dev));
	priv->subdev.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;

	priv->subdev.entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	priv->subdev.entity.ops = &rcar_csi2_entity_ops;

	priv->pads[RCAR_CSI2_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = RCAR_CSI2_SOURCE_VC0; i < NR_OF_RCAR_CSI2_PAD; i++)
		priv->pads[i].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&priv->subdev.entity, NR_OF_RCAR_CSI2_PAD,
				     priv->pads);
	if (ret)
		goto error;

	pm_runtime_enable(&pdev->dev);

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0)
		goto error;

	dev_info(priv->dev, "%d lanes found\n", priv->lanes);

	return 0;

error:
	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE)) {
		v4l2_async_notifier_unregister(&priv->notifier);
		v4l2_async_notifier_cleanup(&priv->notifier);
	}

	return ret;
}

static int rcsi2_remove(struct platform_device *pdev)
{
	struct rcar_csi2 *priv = platform_get_drvdata(pdev);

	if (!(priv->info->features & RCAR_VIN_R8A78000_FEATURE))
		v4l2_async_notifier_unregister(&priv->notifier);

	v4l2_async_unregister_subdev(&priv->subdev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver rcar_csi2_pdrv = {
	.remove	= rcsi2_remove,
	.probe	= rcsi2_probe,
	.driver	= {
		.name	= "rcar-csi2",
		.of_match_table	= rcar_csi2_of_table,
	},
};

module_platform_driver(rcar_csi2_pdrv);

MODULE_AUTHOR("Niklas Söderlund <niklas.soderlund@ragnatech.se>");
MODULE_DESCRIPTION("Renesas R-Car MIPI CSI-2 receiver driver");
MODULE_LICENSE("GPL");

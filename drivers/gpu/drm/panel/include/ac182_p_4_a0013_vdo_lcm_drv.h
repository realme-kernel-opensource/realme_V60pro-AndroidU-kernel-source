/***********************************************************
** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd.
** File: AC182_P_3_A0013_VDO_PANEL.h
** Description: source file for lcm AC182 in kernel stage
**
** Version: 1.0
** Date: 2023/12/29
** Author: display.lcd
**
** ------------------------------- Revision History: -------------------------------
**      <author>        <data>        <version >           <desc>
**       display        2023/12/29      1.0              source file for lcm AC182 in kernel stage
**
****************************************************************/

#ifndef AC182_P_4_A0013_VDO_LCM_DRV_H
#define AC182_P_4_A0013_VDO_LCM_DRV_H

#define REGFLAG_CMD                 0xFFFA
#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD

#define FRAME_WIDTH                 720
#define FRAME_HEIGHT                1604
#define PHYSICAL_WIDTH              69401
#define PHYSICAL_HEIGHT             154610

#define HSA                         10
#define HFP                         18
#define HBP                         18
#define VSA                         4
#define VBP                         28
#define MIPI_CLK                    594
#define DATA_RATE                   1188
#define HOPPING_MIPI_CLK            594
#define HOPPING_DATA_RATE           1188
#define HOPPING_HBP                 18
/*Parameter setting for mode 0 Start*/
#define MODE_60_FPS                  60
#define MODE_60_VFP                  2670
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_90_FPS                  90
#define MODE_90_VFP                  1232
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_120_FPS                  120
#define MODE_120_VFP                  512
/*Parameter setting for mode 2 End*/

struct LCM_setting_table {
	unsigned int cmd;
	unsigned int count;
	unsigned char para_list[128];
};

/* ------------------------- initial code start------------------------- */
static struct LCM_setting_table init_setting[] = {
	{REGFLAG_CMD, 4, {0xFF, 0x98, 0x83, 0x06}},
	{REGFLAG_CMD, 2, {0x06, 0xA4}},
	{REGFLAG_CMD, 2, {0x3E, 0xE2}},
	{REGFLAG_CMD, 4, {0xFF, 0x98, 0x83, 0x03}},
	{REGFLAG_CMD, 2, {0x83, 0x20}},
	{REGFLAG_CMD, 2, {0x84, 0x00}},
	{REGFLAG_CMD, 4, {0xFF, 0x98, 0x83, 0x03}},
	{REGFLAG_CMD, 2, {0x86, 0x6C}},
	{REGFLAG_CMD, 2, {0x88, 0xE1}},
	{REGFLAG_CMD, 2, {0x89, 0xe8}},
	{REGFLAG_CMD, 2, {0x8A, 0xF0}},
	{REGFLAG_CMD, 2, {0x8B, 0xF7}},

	{REGFLAG_CMD, 2, {0x8C, 0xBF}},
	{REGFLAG_CMD, 2, {0x8D, 0xC5}},
	{REGFLAG_CMD, 2, {0x8E, 0xC8}},
	{REGFLAG_CMD, 2, {0x8F, 0xCE}},
	{REGFLAG_CMD, 2, {0x90, 0xD1}},
	{REGFLAG_CMD, 2, {0x91, 0xD6}},
	{REGFLAG_CMD, 2, {0x92, 0xDC}},
	{REGFLAG_CMD, 2, {0x93, 0xE3}},
	{REGFLAG_CMD, 2, {0x94, 0xED}},
	{REGFLAG_CMD, 2, {0x95, 0xFA}},
	{REGFLAG_CMD, 2, {0xAF, 0x18}},

	{REGFLAG_CMD, 4, {0xFF, 0x98, 0x83, 0x07}},
	{REGFLAG_CMD, 2, {0x00, 0x00}},
	{REGFLAG_CMD, 2, {0x01, 0x00}},
	{REGFLAG_CMD, 4, {0xFF, 0x98, 0x83, 0x00}},
	{REGFLAG_CMD, 2, {0x53, 0x24}},
	{REGFLAG_CMD, 2, {0x35, 0x00}},
	{REGFLAG_CMD, 2, {0x11, 0x00}},
	{REGFLAG_DELAY, 80, {}},
	{REGFLAG_CMD, 2, {0x29, 0x00}},
	{REGFLAG_DELAY, 20, {}},
};
/* ------------------------- initial code end-------------------------- */

/* ------------------------- Display off sequence start --------------- */
static struct LCM_setting_table lcm_off_setting[] = {
	/* Delay 5ms */
	{REGFLAG_DELAY, 5, {}},
	/* Page 0 */
	{REGFLAG_CMD, 4, {0xFF, 0x98, 0x83, 0x00}},
	{REGFLAG_CMD, 2, {0x28, 0x00}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_CMD, 2, {0x10, 0x00}},
	{REGFLAG_DELAY, 80, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
/* ------------------------- Display off sequence end ---------------- */
#endif /* end of AC182_P_4_A0013_VDO_LCM_DRV_H */

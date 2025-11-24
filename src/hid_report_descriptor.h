/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef HID_REPORT_DESCRIPTOR_H_
#define HID_REPORT_DESCRIPTOR_H_

#include <zephyr/types.h>

/* Report lengths and indices */
#define INPUT_REP_BUTTONS_LEN       3
#define INPUT_REP_MOVEMENT_LEN      3
#define INPUT_REP_MEDIA_PLAYER_LEN  1
#define INPUT_REP_BUTTONS_INDEX     0
#define INPUT_REP_MOVEMENT_INDEX    1
#define INPUT_REP_MPLAYER_INDEX     2
#define INPUT_REP_REF_BUTTONS_ID    1
#define INPUT_REP_REF_MOVEMENT_ID   2
#define INPUT_REP_REF_MPLAYER_ID    3

/* Feature Report IDs and sizes */
#define FEATURE_REPORT_ID_RES_MULT  0x10
#define FEATURE_REPORT_SIZE         1

/* HID descriptor helper macros */
#define HID_LOGICAL_MIN8(x)  0x15, (x)
#define HID_LOGICAL_MAX8(x)  0x25, (x)

/* HID Report Map */
static const uint8_t report_map[] = {
	0x05, 0x01,     /* Usage Page (Generic Desktop) */
	0x09, 0x02,     /* Usage (Mouse) */

	0xA1, 0x01,     /* Collection (Application) */

	/* Report ID 1: Mouse buttons + scroll/pan */
	0x85, 0x01,       /* Report Id 1 */
	0x09, 0x01,       /* Usage (Pointer) */
	0xA1, 0x00,       /* Collection (Physical) */
	
	/* --- INPUT REPORTS --- */
	0x95, 0x05,       /* Report Count (5) */
	0x75, 0x01,       /* Report Size (1) */
	0x05, 0x09,       /* Usage Page (Buttons) */
	0x19, 0x01,       /* Usage Minimum (01) */
	0x29, 0x05,       /* Usage Maximum (05) */
	0x15, 0x00,       /* Logical Minimum (0) */
	0x25, 0x01,       /* Logical Maximum (1) */
	0x81, 0x02,       /* Input (Data, Variable, Absolute) */
	0x95, 0x01,       /* Report Count (1) */
	0x75, 0x03,       /* Report Size (3) */
	0x81, 0x01,       /* Input (Constant) for padding */
	0x75, 0x08,       /* Report Size (8) */
	0x95, 0x01,       /* Report Count (1) */
	0x05, 0x01,       /* Usage Page (Generic Desktop) */
	0x09, 0x38,       /* Usage (Wheel) */
	0x15, 0x81,       /* Logical Minimum (-127) */
	0x25, 0x7F,       /* Logical Maximum (127) */
	0x81, 0x06,       /* Input (Data, Variable, Relative) */
	0x05, 0x0C,       /* Usage Page (Consumer) */
	0x0A, 0x38, 0x02, /* Usage (AC Pan) */
	0x95, 0x01,       /* Report Count (1) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	
	/* --- FEATURE REPORT: Resolution Multiplier --- */
	0x85, 0x10,       /* Report ID (16) for FEATURE */
	0x05, 0x01,       /* Usage Page (Generic Desktop) */
	0x09, 0x48,       /* Usage (Resolution Multiplier) */
 	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(3),       /* Logical Maximum (3) - values 0..3 => 1x,2x,4x,8x */
	0x35, 0x01,       /* Physical Minimum (1) */
	0x45, 0x20,       /* Physical Maximum (16) */
	0x75, 0x02,       /* Report Size (2 bits) */
	0x95, 0x01,       /* Report Count (1) */
	0xB1, 0x02,       /* Feature (Data, Variable, Absolute) */
	
	/* Padding to complete the byte */
	0x75, 0x06,       /* Report Size (6 bits) */
	0x95, 0x01,       /* Report Count (1) */
	0xB1, 0x01,       /* Feature (Constant) */
	
	0xC0,             /* End Collection (Physical) */

	/* Report ID 2: Mouse motion */
	0x85, 0x02,       /* Report Id 2 */
	0x09, 0x01,       /* Usage (Pointer) */
	0xA1, 0x00,       /* Collection (Physical) */
	0x75, 0x0C,       /* Report Size (12) */
	0x95, 0x02,       /* Report Count (2) */
	0x05, 0x01,       /* Usage Page (Generic Desktop) */
	0x09, 0x30,       /* Usage (X) */
	0x09, 0x31,       /* Usage (Y) */
	0x16, 0x01, 0xF8, /* Logical maximum (2047) */
	0x26, 0xFF, 0x07, /* Logical minimum (-2047) */
	0x81, 0x06,       /* Input (Data, Variable, Relative) */
	0xC0,             /* End Collection (Physical) */
	0xC0,             /* End Collection (Application) */

	/* Report ID 3: Advanced buttons */
	0x05, 0x0C,       /* Usage Page (Consumer) */
	0x09, 0x01,       /* Usage (Consumer Control) */
	0xA1, 0x01,       /* Collection (Application) */
	0x85, 0x03,       /* Report Id (3) */
	0x15, 0x00,       /* Logical minimum (0) */
	0x25, 0x01,       /* Logical maximum (1) */
	0x75, 0x01,       /* Report Size (1) */
	0x95, 0x01,       /* Report Count (1) */

	0x09, 0xCD,       /* Usage (Play/Pause) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0x09, 0xB5,       /* Usage (Scan Next Track) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0x09, 0xB6,       /* Usage (Scan Previous Track) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

	0x09, 0xEA,       /* Usage (Volume Down) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0x09, 0xE9,       /* Usage (Volume Up) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0x0A, 0x25, 0x02, /* Usage (AC Forward) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0x0A, 0x24, 0x02, /* Usage (AC Back) */
	0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
	0xC0              /* End Collection */
};

#endif /* HID_REPORT_DESCRIPTOR_H_ */
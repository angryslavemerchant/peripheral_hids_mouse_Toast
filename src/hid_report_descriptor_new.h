/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef HID_REPORT_DESCRIPTOR_H_
#define HID_REPORT_DESCRIPTOR_H_

#include <zephyr/types.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/device.h>

/* Report lengths and indices */
#define INPUT_REP_BUTTONS_LEN       4    /* buttons + x + y + wheel (without report ID) */
#define INPUT_REP_MOVEMENT_LEN      3    /* kept for compatibility */
#define INPUT_REP_MEDIA_PLAYER_LEN  1    /* kept for compatibility */
#define INPUT_REP_BUTTONS_INDEX     0
#define INPUT_REP_MOVEMENT_INDEX    1
#define INPUT_REP_MPLAYER_INDEX     2
#define INPUT_REP_REF_BUTTONS_ID    1
#define INPUT_REP_REF_MOVEMENT_ID   2    /* not used in new descriptor */
#define INPUT_REP_REF_MPLAYER_ID    3    /* not used in new descriptor */

/* Feature Report IDs and sizes */
#define FEATURE_REPORT_ID_RES_MULT  0x10
#define FEATURE_REPORT_SIZE         1

/* HID Report Map using Zephyr macros */
static const uint8_t report_map[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GEN_DESKTOP_MOUSE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),

        0x85, 0x01,  // Report ID (1) for INPUT

        HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),
        HID_COLLECTION(HID_COLLECTION_PHYSICAL),

            // Buttons
            HID_USAGE_PAGE(HID_USAGE_GEN_BUTTON),
            HID_USAGE_MIN8(1),
            HID_USAGE_MAX8(3),
            HID_LOGICAL_MIN8(0),
            HID_LOGICAL_MAX8(1),
            HID_REPORT_COUNT(3),
            HID_REPORT_SIZE(1),
            HID_INPUT(0x02),

            // Padding
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(5),
            HID_INPUT(0x01),

            // X, Y
            HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
            HID_USAGE(HID_USAGE_GEN_DESKTOP_X),
            HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),
            HID_LOGICAL_MIN8(-127),
            HID_LOGICAL_MAX8(127),
            HID_REPORT_SIZE(8),
            HID_REPORT_COUNT(2),
            HID_INPUT(0x06),

            // Wheel (signed, relative)
            HID_USAGE(HID_USAGE_GEN_DESKTOP_WHEEL),
            HID_LOGICAL_MIN8(-127),
            HID_LOGICAL_MAX8(127),
            HID_REPORT_SIZE(8),
            HID_REPORT_COUNT(1),
            HID_INPUT(0x06),

            // ---- Feature: Resolution Multiplier (same PHYSICAL collection) ----
            0x85, 0x10,                 // Report ID (16) for FEATURE
            HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
            HID_USAGE(0x48),            // Resolution Multiplier
            HID_LOGICAL_MIN8(0),
            HID_LOGICAL_MAX8(3),        // 0..3 => 1x,2x,4x,8x
            0x35, 0x01,                 // Physical Min = 1
            0x45, 0x20,                 // Physical Max = 120 (0x78)
            HID_REPORT_SIZE(2),         // 2 bits
            HID_REPORT_COUNT(1),
            0xB1, 0x02,                 // FEATURE (Data,Var,Abs)

            // pad to byte
            HID_REPORT_SIZE(6),
            HID_REPORT_COUNT(1),
            0xB1, 0x01,                 // FEATURE (Const)

        HID_END_COLLECTION, // PHYSICAL

    HID_END_COLLECTION, // APPLICATION
};

/* HID Report structure for Report ID 0x01 */
struct hid_report {
    uint8_t report_id;  // 0x01
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
} __packed;

#endif /* HID_REPORT_DESCRIPTOR_H_ */
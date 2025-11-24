/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <assert.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/hids.h>

#include "hid.h"
#include "bluetooth.h"
#include "hid_report_descriptor.h"

#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/device.h>  

#define BASE_USB_HID_SPEC_VERSION   0x0101

/* RESOLUTION MULTIPLIER */
static uint8_t resolution_multiplier_data = 0;  // Actual stored data
static const uint8_t multiplier_table[] = {1, 2, 4, 8};

/* Feature report handler for resolution multiplier */
/* Feature report handler - called AFTER read/write */
static void feature_report_handler(struct bt_hids_rep *rep,
                                   struct bt_conn *conn,
                                   bool write)
{
	if (write) {
		/* Host wrote new value (SET_REPORT) */
		uint8_t new_multiplier = (*rep->data) & 0x03;
		
		if (new_multiplier != resolution_multiplier_data) {
			resolution_multiplier_data = new_multiplier;
			printk("Resolution multiplier SET to %dx\n", 
			       multiplier_table[resolution_multiplier_data]);
		}
	} else {
		/* Host read value (GET_REPORT) - data was already sent */
		printk("Resolution multiplier GET: %dx\n", 
		       multiplier_table[resolution_multiplier_data]);
	}
}

int16_t hid_apply_resolution_multiplier(int16_t delta)
{
	//return delta / multiplier_table[resolution_multiplier_data];
	return delta;
}

void hid_update_resolution_multiplier(uint8_t multiplier)
{
	resolution_multiplier_data = multiplier & 0x03;
	
	/* Update the stored feature report in HIDS service */
	// You may need to call a HIDS API function here to update the stored data
	// This ensures GET_REPORT returns the correct value
}


/* HIDS instance */
BT_HIDS_DEF(hids_obj,
	    INPUT_REP_BUTTONS_LEN,
	    INPUT_REP_MOVEMENT_LEN,
	    INPUT_REP_MEDIA_PLAYER_LEN);

/* Connection tracking */
static struct conn_mode {
	struct bt_conn *conn;
	bool in_boot_mode;
} conn_mode[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];


static void hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	size_t i;

	for (i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			break;
		}
	}

	if (i >= CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered %s\n", addr);
		conn_mode[i].in_boot_mode = false;
		break;

	default:
		break;
	}
}

void hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	struct bt_hids_outp_feat_rep *feat_rep;
	static const uint8_t mouse_movement_mask[DIV_ROUND_UP(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);

	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE |
				      BT_HIDS_NORMALLY_CONNECTABLE);

	/* Configure input reports */
	hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
	hids_inp_rep->size = INPUT_REP_BUTTONS_LEN;
	hids_inp_rep->id = INPUT_REP_REF_BUTTONS_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MOVEMENT_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MOVEMENT_ID;
	hids_inp_rep->rep_mask = mouse_movement_mask;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MEDIA_PLAYER_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MPLAYER_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	/* Configure feature report */
	feat_rep = &hids_init_param.feat_rep_group_init.reports[0];
	feat_rep->size = FEATURE_REPORT_SIZE;
	feat_rep->id = FEATURE_REPORT_ID_RES_MULT;
	feat_rep->handler = feature_report_handler;
	hids_init_param.feat_rep_group_init.cnt = 1;

	hids_init_param.is_mouse = true;
	hids_init_param.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_param);
	__ASSERT(err == 0, "HIDS initialization failed\n");

	printk("HID service initialized\n");
}



int hid_notify_connected(struct bt_conn *conn)
{
	int err;

	err = bt_hids_connected(&hids_obj, conn);
	if (err) {
		return err;
	}

	/* Insert connection into tracking array */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			conn_mode[i].conn = conn;
			conn_mode[i].in_boot_mode = false;
			return 0;
		}
	}

	printk("HID: No free connection slot\n");
	return -ENOMEM;
}

int hid_notify_disconnected(struct bt_conn *conn)
{
	int err;

	err = bt_hids_disconnected(&hids_obj, conn);
	if (err) {
		return err;
	}

	/* Remove connection from tracking array */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (conn_mode[i].conn == conn) {
			conn_mode[i].conn = NULL;
			break;
		}
	}

	return 0;
}

void hid_mouse_movement_send(int16_t x_delta, int16_t y_delta)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {

		if (!conn_mode[i].conn) {
			continue;
		}

		if (conn_mode[i].in_boot_mode) {
			/* Boot mode - 8-bit values */
			x_delta = MAX(MIN(x_delta, SCHAR_MAX), SCHAR_MIN);
			y_delta = MAX(MIN(y_delta, SCHAR_MAX), SCHAR_MIN);

			bt_hids_boot_mouse_inp_rep_send(&hids_obj,
							conn_mode[i].conn,
							NULL,
							(int8_t)x_delta,
							(int8_t)y_delta,
							NULL);
		} else {
			/* Report mode - 12-bit values */
			uint8_t x_buff[2];
			uint8_t y_buff[2];
			uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

			int16_t x = MAX(MIN(x_delta, 0x07ff), -0x07ff);
			int16_t y = MAX(MIN(y_delta, 0x07ff), -0x07ff);

			/* Convert to little-endian */
			sys_put_le16(x, x_buff);
			sys_put_le16(y, y_buff);

			/* Encode 12-bit report */
			BUILD_ASSERT(sizeof(buffer) == 3,
				     "Only 2 axis, 12-bit each, are supported");

			buffer[0] = x_buff[0];
			buffer[1] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
			buffer[2] = (y_buff[1] << 4) | (y_buff[0] >> 4);

			bt_hids_inp_rep_send(&hids_obj, conn_mode[i].conn,
					     INPUT_REP_MOVEMENT_INDEX,
					     buffer, sizeof(buffer), NULL);
		}
	}
}

void hid_mouse_scroll_send(int8_t wheel, int8_t pan)
{
	uint8_t report[INPUT_REP_BUTTONS_LEN];
	
	/* Report ID 1 structure: buttons(1 byte) + wheel(1 byte) + pan(1 byte) */
	report[0] = 0;      // No buttons pressed
	report[1] = wheel;  // Vertical scroll
	report[2] = pan;    // Horizontal scroll
	
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!conn_mode[i].conn) {
			continue;
		}
		
		//printk("sending %d)\n", wheel);
		/* Send Report ID 1 (buttons + scroll) */
		bt_hids_inp_rep_send(&hids_obj, conn_mode[i].conn,
				     INPUT_REP_BUTTONS_INDEX,
				     report, sizeof(report), NULL);

	}
}
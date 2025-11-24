/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/services/bas.h>

#include "bluetooth.h"
#include "hid.h"
#include "encoder.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define POLL_INTERVAL_MS 10



int main(void)
{

	k_sleep(K_MSEC(3000));  // buffer
	int err;

	printk("Starting Bluetooth Peripheral HIDS mouse sample\n");
	printk("Board: nice!nano / Pro Micro nRF52840\n");

	/* Initialize HID service */
	hid_init();
	
	/* Initialize rotary encoder */
	err = encoder_init();
	if (err) {
		printk("Failed to initialize encoder (err %d)\n", err);
		return 0;
	}

	/* Initialize Bluetooth stack and start advertising */
	err = bluetooth_init(DEVICE_NAME);
	if (err) {
		printk("Bluetooth initialization failed (err %d)\n", err);
		return 0;
	}

	printk("Device ready - advertising as '%s'\n", DEVICE_NAME);
	printk("Waiting for connection...\n");




	/* Main loop - keep alive and update battery */
	while (1) {
    
	int32_t delta;
	encoder_read_delta(&delta);
	if (delta != 0) {
		// Apply resolution multiplier
		int16_t adjusted = hid_apply_resolution_multiplier(delta);
		
		// Clamp to 8-bit range
		int8_t wheel = (int8_t)MAX(MIN(adjusted, 127), -127);
		
		hid_mouse_scroll_send(wheel, 0);
		}
		
		k_sleep(K_MSEC(POLL_INTERVAL_MS));
	}


	return 0;
}
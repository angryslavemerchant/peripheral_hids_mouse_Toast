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

/* Scroll tuning parameters - adjust these to taste */
#define SCROLL_DEADZONE      25    // Ignore offsets smaller than this (noise filter)
#define SCROLL_DIVISOR       75   // Higher = slower scroll, lower = faster
#define SCROLL_MAX           127    // Maximum scroll value per tick (caps speed)

/**
 * @brief Convert encoder offset to scroll value
 * 
 * Maps the absolute offset from reference position to a scroll speed.
 * Larger offset = faster scroll. Includes deadzone and clamping.
 * 
 * @param offset Raw encoder offset from reference
 * @return Scroll value to send (-SCROLL_MAX to +SCROLL_MAX), 0 if in deadzone
 */
static int8_t offset_to_scroll(int32_t offset)
{
    /* Deadzone - ignore small movements */
    if (offset > -SCROLL_DEADZONE && offset < SCROLL_DEADZONE) {
        return 0;
    }

    /* Remove deadzone from calculation */
    int32_t adjusted;
    if (offset > 0) {
        adjusted = offset - SCROLL_DEADZONE;
    } else {
        adjusted = offset + SCROLL_DEADZONE;
    }

    /* Apply acceleration curve */
    int32_t scroll;
    int32_t abs_adjusted = adjusted < 0 ? -adjusted : adjusted;
    int32_t sign = adjusted < 0 ? -1 : 1;
    
    /* Linear base + quadratic acceleration */
    int32_t linear = abs_adjusted / SCROLL_DIVISOR;
    int32_t accel = (abs_adjusted * abs_adjusted) / (SCROLL_DIVISOR * 1000);
    
    scroll = sign * (linear + accel);

    /* Clamp to max scroll speed */
    if (scroll > SCROLL_MAX) {
        scroll = SCROLL_MAX;
    } else if (scroll < -SCROLL_MAX) {
        scroll = -SCROLL_MAX;
    }

    return (int8_t)scroll;
}

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

    /* Main loop */
    while (1) {
        int32_t delta;
        encoder_read_delta(&delta);
        
        int8_t wheel = offset_to_scroll(delta);
        
        if (wheel != 0) {
            hid_mouse_scroll_send(wheel, 0);
        }
        
        k_sleep(K_MSEC(POLL_INTERVAL_MS));
    }

    return 0;
}
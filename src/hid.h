/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef HID_H_
#define HID_H_

#include <zephyr/bluetooth/conn.h>
#include <stdint.h>

/**
 * @brief Initialize HID service
 */
void hid_init(void);

/**
 * @brief Notify HID service of new connection
 * 
 * @param conn Connection handle
 * @return 0 on success, negative errno on failure
 */
int hid_notify_connected(struct bt_conn *conn);

/**
 * @brief Notify HID service of disconnection
 * 
 * @param conn Connection handle
 * @return 0 on success, negative errno on failure
 */
int hid_notify_disconnected(struct bt_conn *conn);

/**
 * @brief Send mouse movement to all connected devices
 * 
 * @param x_delta X-axis movement
 * @param y_delta Y-axis movement
 */
void hid_mouse_movement_send(int16_t x_delta, int16_t y_delta);

/**
 * @brief Apply resolution multiplier to scroll delta
 * 
 * @param delta Raw encoder delta
 * @return Multiplied delta value
 */
int16_t hid_apply_resolution_multiplier(int16_t delta);

#endif /* HID_H_ */
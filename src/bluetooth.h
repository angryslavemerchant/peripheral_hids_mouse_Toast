/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BLUETOOTH_H_
#define BLUETOOTH_H_

#include <zephyr/bluetooth/conn.h>

/**
 * @brief Initialize Bluetooth stack and start advertising
 * 
 * @param device_name Name to advertise
 * @return 0 on success, negative errno on failure
 */
int bluetooth_init(const char *device_name);

/**
 * @brief Check if a connection slot is available
 * 
 * @return true if a slot is free, false otherwise
 */
bool bluetooth_is_conn_slot_free(void);

/**
 * @brief Get connection by index
 * 
 * @param index Connection index
 * @return Pointer to connection or NULL
 */
struct bt_conn *bluetooth_get_conn(size_t index);


/**
 * @brief Check if a connected
 * 
 * @return true if a security passed, and device connected.
 */
bool bluetooth_security_passed(void);

#endif /* BLUETOOTH_H_ */
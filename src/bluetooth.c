/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/drivers/gpio.h>

#include "bluetooth.h"
#include "hid.h"

/* Bluetooth connection management */
static struct bt_conn *connections[CONFIG_BT_HIDS_MAX_CLIENT_COUNT];
static volatile bool is_adv_running;
static struct k_work adv_work;
static const char *device_name;
static size_t device_name_len;

/* Unpair button configuration */
#define UNPAIR_BUTTON_PIN 9
static const struct device *button_dev;
static struct gpio_callback button_cb_data;
static struct k_work unpair_work;

#if CONFIG_BT_DIRECTED_ADVERTISING
K_MSGQ_DEFINE(bonds_queue, sizeof(bt_addr_le_t), CONFIG_BT_MAX_PAIRED, 4);

static void bond_find(const struct bt_bond_info *info, void *user_data)
{
	int err;

	/* Filter already connected peers */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (connections[i]) {
			const bt_addr_le_t *dst = bt_conn_get_dst(connections[i]);
			if (!bt_addr_le_cmp(&info->addr, dst)) {
				return;
			}
		}
	}

	err = k_msgq_put(&bonds_queue, (void *)&info->addr, K_NO_WAIT);
	if (err) {
		printk("No space in the queue for the bond.\n");
	}
}
#endif

/* Advertising data - must be at file scope */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static void advertising_continue(void)
{
	struct bt_le_adv_param adv_param;

#if CONFIG_BT_DIRECTED_ADVERTISING
	bt_addr_le_t addr;

	if (!k_msgq_get(&bonds_queue, &addr, K_NO_WAIT)) {
		char addr_buf[BT_ADDR_LE_STR_LEN];
		int err;

		if (is_adv_running) {
			err = bt_le_adv_stop();
			if (err) {
				printk("Advertising failed to stop (err %d)\n", err);
				return;
			}
			is_adv_running = false;
		}

		adv_param = *BT_LE_ADV_CONN_DIR(&addr);
		adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;

		err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);
		if (err) {
			printk("Directed advertising failed to start (err %d)\n", err);
			return;
		}

		bt_addr_le_to_str(&addr, addr_buf, BT_ADDR_LE_STR_LEN);
		printk("Direct advertising to %s started\n", addr_buf);
	} else
#endif
	{
		int err;

		if (is_adv_running) {
			return;
		}

		/* Scan response data with device name */
		struct bt_data sd[] = {
			BT_DATA(BT_DATA_NAME_COMPLETE, device_name, device_name_len),
		};

		adv_param = *BT_LE_ADV_CONN_FAST_2;
		err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		if (err) {
			printk("Advertising failed to start (err %d)\n", err);
			return;
		}

		printk("Regular advertising started\n");
	}

	is_adv_running = true;
}

static void advertising_start(void)
{
#if CONFIG_BT_DIRECTED_ADVERTISING
	k_msgq_purge(&bonds_queue);
	bt_foreach_bond(BT_ID_DEFAULT, bond_find, NULL);
#endif

	k_work_submit(&adv_work);
}

static void advertising_process(struct k_work *work)
{
	advertising_continue();
}

static void insert_conn_object(struct bt_conn *conn)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!connections[i]) {
			connections[i] = conn;
			return;
		}
	}

	printk("Connection object could not be inserted %p\n", conn);
}

static void remove_conn_object(struct bt_conn *conn)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (connections[i] == conn) {
			connections[i] = NULL;
			break;
		}
	}
}

bool bluetooth_is_conn_slot_free(void)
{
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (!connections[i]) {
			return true;
		}
	}
	return false;
}

struct bt_conn *bluetooth_get_conn(size_t index)
{
	if (index < CONFIG_BT_HIDS_MAX_CLIENT_COUNT) {
		return connections[index];
	}
	return NULL;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	is_adv_running = false;
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		if (err == BT_HCI_ERR_ADV_TIMEOUT) {
			printk("Direct advertising to %s timed out\n", addr);
			k_work_submit(&adv_work);
		} else {
			printk("Failed to connect to %s 0x%02x %s\n", 
			       addr, err, bt_hci_err_to_str(err));
		}
		return;
	}

	printk("Connected %s\n", addr);

	/* Notify HID service */
	err = hid_notify_connected(conn);
	if (err) {
		printk("Failed to notify HID service about connection\n");
		return;
	}

	insert_conn_object(conn);

	struct bt_conn_info info;
	bt_conn_get_info(conn, &info);
	
	printk("Initial connection parameters:\n");
	printk("  Interval: %u (%.2f ms)\n", 
	       info.le.interval, info.le.interval * 1.25);
	printk("  Latency: %u\n", info.le.latency);
	printk("  Timeout: %u\n", info.le.timeout);

	if (bluetooth_is_conn_slot_free()) {
		advertising_start();
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Disconnected from %s, reason 0x%02x %s\n", 
	       addr, reason, bt_hci_err_to_str(reason));

	/* Notify HID service */
	int err = hid_notify_disconnected(conn);
	if (err) {
		printk("Failed to notify HID service about disconnection\n");
	}

	remove_conn_object(conn);
	advertising_start();
}

#ifdef CONFIG_BT_HIDS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d %s\n", 
		       addr, level, err, bt_security_err_to_str(err));
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
#ifdef CONFIG_BT_HIDS_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};

/* Authentication callbacks */
#if defined(CONFIG_BT_HIDS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Auto-accepting pairing for %s: %06u\n", addr, passkey);
	
	bt_conn_auth_passkey_confirm(conn);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing failed conn: %s, reason %d %s\n", 
	       addr, reason, bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

/* Unpair button implementation */
static void unpair_all_bonds(struct k_work *work)
{
	int err;
	
	printk("=== UNPAIR BUTTON PRESSED ===\n");
	printk("Unpairing all bonded devices...\n");
	
	/* Disconnect all active connections first */
	for (size_t i = 0; i < CONFIG_BT_HIDS_MAX_CLIENT_COUNT; i++) {
		if (connections[i]) {
			char addr[BT_ADDR_LE_STR_LEN];
			bt_addr_le_to_str(bt_conn_get_dst(connections[i]), addr, sizeof(addr));
			printk("Disconnecting %s...\n", addr);
			
			err = bt_conn_disconnect(connections[i], 
			                         BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			if (err) {
				printk("Failed to disconnect connection %zu (err %d)\n", 
				       i, err);
			}
		}
	}
	
	/* Wait for disconnections to complete */
	k_sleep(K_MSEC(100));
	
	/* Clear all bonds */
	err = bt_unpair(BT_ID_DEFAULT, NULL);
	if (err) {
		printk("Failed to unpair devices (err %d)\n", err);
	} else {
		printk("All devices unpaired successfully!\n");
	}
	
	/* Restart advertising */
	k_sleep(K_MSEC(500));
	advertising_start();
	
	printk("=== UNPAIR COMPLETE ===\n");
}

static void button_pressed(const struct device *dev, 
                          struct gpio_callback *cb, 
                          uint32_t pins)
{
	printk(">>> Unpair button interrupt triggered! <<<\n");
	k_work_submit(&unpair_work);
}

static int unpair_button_init(void)
{
	int err;
	
	printk("\n=== Initializing Unpair Button ===\n");
	
	/* Get GPIO device */
	button_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	
	if (!button_dev) {
		printk("ERROR: Failed to get GPIO device\n");
		return -ENODEV;
	}
	
	printk("GPIO device pointer: %p\n", button_dev);
	
	if (!device_is_ready(button_dev)) {
		printk("ERROR: GPIO device not ready\n");
		return -ENODEV;
	}
	
	printk("GPIO device ready: %s\n", button_dev->name);
	
	/* Configure pin as input with pull-up */
	err = gpio_pin_configure(button_dev, UNPAIR_BUTTON_PIN, 
	                         GPIO_INPUT | GPIO_PULL_UP);
	if (err) {
		printk("ERROR: Failed to configure pin P0.%02d (err %d)\n", 
		       UNPAIR_BUTTON_PIN, err);
		return err;
	}
	
	printk("Pin P0.%02d configured as input with pull-up\n", UNPAIR_BUTTON_PIN);
	
	/* Test pin reading */
	int pin_state = gpio_pin_get(button_dev, UNPAIR_BUTTON_PIN);
	printk("Current pin state: %d (should be 1 when not pressed)\n", pin_state);
	
	/* Configure interrupt on falling edge (button press) */
	err = gpio_pin_interrupt_configure(button_dev, UNPAIR_BUTTON_PIN,
	                                   GPIO_INT_EDGE_FALLING);
	if (err) {
		printk("ERROR: Failed to configure interrupt (err %d)\n", err);
		return err;
	}
	
	printk("Interrupt configured for falling edge\n");
	
	/* Setup and add callback */
	gpio_init_callback(&button_cb_data, button_pressed, BIT(UNPAIR_BUTTON_PIN));
	err = gpio_add_callback(button_dev, &button_cb_data);
	if (err) {
		printk("ERROR: Failed to add callback (err %d)\n", err);
		return err;
	}
	
	printk("GPIO callback added\n");
	
	/* Initialize work queue */
	k_work_init(&unpair_work, unpair_all_bonds);
	
	printk("✓ Unpair button successfully initialized on P0.09\n");
	printk("  Press button to unpair all devices\n");
	printk("=================================\n\n");
	
	return 0;
}

int bluetooth_init(const char *name)
{
	int err;

	printk("\n=== Bluetooth Initialization Start ===\n");

	device_name = name;
	device_name_len = strlen(name);

	/* Register authentication callbacks */
	if (IS_ENABLED(CONFIG_BT_HIDS_SECURITY_ENABLED)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			printk("Failed to register authorization callbacks.\n");
			return err;
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			printk("Failed to register authorization info callbacks.\n");
			return err;
		}
	}

	/* Enable Bluetooth */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	/* Initialize work queues */
	k_work_init(&adv_work, advertising_process);

	/* Load settings if enabled */
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* Initialize unpair button */
	err = unpair_button_init();
	if (err) {
		printk("WARNING: Failed to initialize unpair button (err %d)\n", err);
		printk("Continuing without unpair button functionality...\n");
		/* Non-fatal, continue */
	}

	/* Start advertising */
	advertising_start();

	printk("=== Bluetooth Initialization Complete ===\n\n");

	return 0;
}
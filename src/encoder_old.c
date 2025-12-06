/**
 * @file encoder.c
 * @brief MT6701 Magnetic Encoder via I2C (Dual encoder support)
 */

#include "encoder.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>

/* MT6701 I2C Configuration */
#define MT6701_I2C_ADDR         0x06
#define MT6701_ANGLE_REG_H      0x03  // Angle high byte register
#define MT6701_ANGLE_REG_L      0x04  // Angle low byte register
#define MT6701_RESOLUTION       16384  // 14-bit: 2^14 = 16384
#define MT6701_HALF_RESOLUTION  8192   // For wraparound detection

/* Sensitivity tuning - adjust this to taste! */
#define SCROLL_SCALING_FACTOR   10  // Lower = more sensitive
#define DEAD_ZONE_FACTOR   10  // Lower = more sensitive


/* LED for debugging */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* I2C devices */
static const struct device *i2c_dev_0;
static const struct device *i2c_dev_1;

/* Encoder 0 state (i2c0) */
static uint16_t last_angle_0 = 0;
static int32_t accumulated_0 = 0;
static bool initialized_0 = false;

/* Encoder 1 state (i2c1) */
static uint16_t last_angle_1 = 0;
static int32_t accumulated_1 = 0;
static bool initialized_1 = false;

/**
 * @brief Read 14-bit angle from MT6701 via I2C
 * 
 * @param i2c_dev I2C device to use
 * @param angle Pointer to store angle value (0-16383)
 * @return 0 on success, negative errno on failure
 */
static int mt6701_read_angle(const struct device *i2c_dev, uint16_t *angle)
{
    uint8_t data[2];
    int ret;

    if (!angle || !i2c_dev) {
        return -EINVAL;
    }

    /* Read 2 bytes starting from angle high register */
    ret = i2c_burst_read(i2c_dev, MT6701_I2C_ADDR, MT6701_ANGLE_REG_H, data, 2);
    if (ret < 0) {
        return ret;
    }

    /* 
     * Combine bytes into 14-bit angle
     * MT6701 format: [13:6] in high byte, [5:0] in low byte bits [7:2]
     * High byte has bits 13-6 (8 bits)
     * Low byte has bits 5-0 in positions [7:2], shift right by 2
     */
    *angle = ((uint16_t)data[0] << 6) | (data[1] >> 2);
    
    /* Ensure angle is within valid range */
    *angle &= 0x3FFF;  // Mask to 14 bits (0-16383)

    return 0;
}

/**
 * @brief Initialize both MT6701 encoders
 * 
 * Sets up I2C communication and reads initial positions
 * 
 * @return 0 on success, negative errno on failure
 */
int encoder_init(void)
{
    int ret;

    /* Initialize LED for debugging */
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    }

    /* Get I2C0 device (encoder 0) */
    i2c_dev_0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev_0)) {
        printk("Encoder 0: I2C0 device not ready\n");
        return -ENODEV;
    }

    /* Read initial angle for encoder 0 */
    ret = mt6701_read_angle(i2c_dev_0, &last_angle_0);
    if (ret < 0) {
        printk("Encoder 0: Failed to read initial angle (err %d)\n", ret);
        return ret;
    }
    initialized_0 = true;
    printk("Encoder 0: Initialized on I2C0\n");

    /* Get I2C1 device (encoder 1) */
    i2c_dev_1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));
    if (!device_is_ready(i2c_dev_1)) {
        printk("Encoder 1: I2C1 device not ready\n");
        return -ENODEV;
    }

    /* Read initial angle for encoder 1 */
    ret = mt6701_read_angle(i2c_dev_1, &last_angle_1);
    if (ret < 0) {
        printk("Encoder 1: Failed to read initial angle (err %d)\n", ret);
        return ret;
    }
    initialized_1 = true;
    printk("Encoder 1: Initialized on I2C1\n");

    return 0;
}

/**
 * @brief Read delta from encoder 0
 */
int encoder_read_delta_0(int32_t *delta)
{
    uint16_t current_angle;
    int32_t raw_delta;
    int ret;

    if (!delta) {
        return -EINVAL;
    }

    if (!initialized_0) {
        *delta = 0;
        return -ENODEV;
    }

    ret = mt6701_read_angle(i2c_dev_0, &current_angle);
    if (ret < 0) {
        *delta = 0;
        return ret;
    }

    /* Calculate delta */
    raw_delta = (int32_t)current_angle - (int32_t)last_angle_0;

    /* Handle wraparound */
    if (raw_delta > MT6701_HALF_RESOLUTION) {
        raw_delta -= MT6701_RESOLUTION;
    } else if (raw_delta < -MT6701_HALF_RESOLUTION) {
        raw_delta += MT6701_RESOLUTION;
    }

    /* Accumulate movement */
    accumulated_0 += raw_delta;
    
    *delta = accumulated_0 / SCROLL_SCALING_FACTOR;
    accumulated_0 -= (*delta * SCROLL_SCALING_FACTOR);

    last_angle_0 = current_angle;
    if (*delta <= DEAD_ZONE_FACTOR) {
        *delta = 0;
    }
    return 0;
}

/**
 * @brief Read delta from encoder 1
 */
int encoder_read_delta_1(int32_t *delta)
{
    uint16_t current_angle;
    int32_t raw_delta;
    int ret;

    if (!delta) {
        return -EINVAL;
    }

    if (!initialized_1) {
        *delta = 0;
        return -ENODEV;
    }

    ret = mt6701_read_angle(i2c_dev_1, &current_angle);
    if (ret < 0) {
        *delta = 0;
        return ret;
    }

    /* Calculate delta */
    raw_delta = (int32_t)current_angle - (int32_t)last_angle_1;

    /* Handle wraparound */
    if (raw_delta > MT6701_HALF_RESOLUTION) {
        raw_delta -= MT6701_RESOLUTION;
    } else if (raw_delta < -MT6701_HALF_RESOLUTION) {
        raw_delta += MT6701_RESOLUTION;
    }

    /* Accumulate movement */
    accumulated_1 += raw_delta;
    
    *delta = accumulated_1 / SCROLL_SCALING_FACTOR;
    accumulated_1 -= (*delta * SCROLL_SCALING_FACTOR);

    last_angle_1 = current_angle;
    if (*delta <= DEAD_ZONE_FACTOR) {
        *delta = 0;
    }
    return 0;
}

/**
 * @brief Read combined delta from both encoders (legacy compatibility)
 * 
 * Reads both encoders and returns their combined delta.
 * Also toggles LED on any movement.
 */
int encoder_read_delta(int32_t *delta)
{
    int32_t delta_0 = 0;
    int32_t delta_1 = 0;

    if (!delta) {
        return -EINVAL;
    }

    /* Read both encoders - don't fail if one is missing */
    encoder_read_delta_0(&delta_0);
    encoder_read_delta_1(&delta_1);

    /* Combine deltas */
    *delta = delta_0 + delta_1;

    /* LED feedback */
    if (*delta != 0 && device_is_ready(led.port)) {
        gpio_pin_toggle_dt(&led);
    }

    return 0;
}
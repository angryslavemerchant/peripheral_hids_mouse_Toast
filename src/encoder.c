/**
 * @file encoder.c
 * @brief MT6701 Magnetic Encoder via I2C
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
//#define WINDOWS_SCROLL_MULTIPLIER 120 // dont think we need this. 

/* LED for debugging */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* I2C device and state */
static const struct device *i2c_dev;
static uint16_t last_angle = 0;
static bool initialized = false;

/**
 * @brief Read 14-bit angle from MT6701 via I2C
 * 
 * @param angle Pointer to store angle value (0-16383)
 * @return 0 on success, negative errno on failure
 */

static int mt6701_read_angle(uint16_t *angle)
{
    uint8_t data[2];
    int ret;

    if (!angle) {
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
 * @brief Initialize MT6701 encoder
 * 
 * Sets up I2C communication and reads initial position
 * 
 * @return 0 on success, negative errno on failure
 */
int encoder_init(void)
{
    int ret;

    /* Get I2C device */
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        return -ENODEV;
    }

    /* Initialize LED for debugging */
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    }

    /* Read initial angle to establish baseline */
    ret = mt6701_read_angle(&last_angle);
    if (ret < 0) {
        return ret;
    }

    initialized = true;
    return 0;
}

/**
 * @brief Read encoder delta since last call
 * 
 * Reads current angle from MT6701, calculates delta from last position,
 * handles 360° wraparound, and scales to appropriate scroll speed.
 * 
 * @param delta Pointer to store calculated delta value
 * @return 0 on success, negative errno on failure
 */

int encoder_read_delta(int32_t *delta)
{
    uint16_t current_angle;
    int32_t raw_delta;
    static int32_t accumulated = 0;  // ADD THIS - persists between calls
    int ret;

    if (!delta) {
        return -EINVAL;
    }

    if (!initialized) {
        return -ENODEV;
    }

    ret = mt6701_read_angle(&current_angle);
    if (ret < 0) {
        *delta = 0;
        return ret;
    }

    /* Calculate delta */
    raw_delta = (int32_t)current_angle - (int32_t)last_angle;

    /* Handle wraparound */
    if (raw_delta > MT6701_HALF_RESOLUTION) {
        raw_delta -= MT6701_RESOLUTION;
    } else if (raw_delta < -MT6701_HALF_RESOLUTION) {
        raw_delta += MT6701_RESOLUTION;
    }

    /* Accumulate movement */
    accumulated += raw_delta;
    
    *delta = accumulated / SCROLL_SCALING_FACTOR;
    accumulated -= (*delta * SCROLL_SCALING_FACTOR);

    /* LED feedback */
    if (*delta != 0 && device_is_ready(led.port)) {
        gpio_pin_toggle_dt(&led);
    }

    last_angle = current_angle;
    return 0;
}
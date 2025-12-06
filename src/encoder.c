/**
 * @file encoder.c
 * @brief MT6701 Magnetic Encoder via I2C (Dual encoder support)
 * 
 * Tracks absolute offset from initial reference position.
 * The further the encoder is turned from its starting position,
 * the larger the delta value returned.
 */

#include "encoder.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>

/* MT6701 I2C Configuration */
#define MT6701_I2C_ADDR         0x06
#define MT6701_ANGLE_REG_H      0x03
#define MT6701_ANGLE_REG_L      0x04
#define MT6701_RESOLUTION       16384  // 14-bit: 2^14 = 16384
#define MT6701_HALF_RESOLUTION  8192   // For wraparound detection

/* LED for debugging */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* I2C devices */
static const struct device *i2c_dev_0;
static const struct device *i2c_dev_1;

/* Encoder 0 state (i2c0) */
static uint16_t reference_angle_0 = 0;
static bool initialized_0 = false;

/* Encoder 1 state (i2c1) */
static uint16_t reference_angle_1 = 0;
static bool initialized_1 = false;

/**
 * @brief Read 14-bit angle from MT6701 via I2C
 */
static int mt6701_read_angle(const struct device *i2c_dev, uint16_t *angle)
{
    uint8_t data[2];
    int ret;

    if (!angle || !i2c_dev) {
        return -EINVAL;
    }

    ret = i2c_burst_read(i2c_dev, MT6701_I2C_ADDR, MT6701_ANGLE_REG_H, data, 2);
    if (ret < 0) {
        return ret;
    }

    *angle = ((uint16_t)data[0] << 6) | (data[1] >> 2);
    *angle &= 0x3FFF;

    return 0;
}

/**
 * @brief Calculate signed offset from reference, handling wraparound
 */
static int32_t calculate_offset(uint16_t current, uint16_t reference)
{
    int32_t offset = (int32_t)current - (int32_t)reference;

    /* Handle wraparound */
    if (offset > MT6701_HALF_RESOLUTION) {
        offset -= MT6701_RESOLUTION;
    } else if (offset < -MT6701_HALF_RESOLUTION) {
        offset += MT6701_RESOLUTION;
    }

    return offset;
}

/**
 * @brief Initialize both MT6701 encoders
 * 
 * Saves current positions as reference points.
 */
int encoder_init(void)
{
    int ret;

    /* Initialize LED for debugging */
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    }

    /* Initialize encoder 0 on I2C0 */
    i2c_dev_0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev_0)) {
        printk("Encoder 0: I2C0 device not ready\n");
        return -ENODEV;
    }

    ret = mt6701_read_angle(i2c_dev_0, &reference_angle_0);
    if (ret < 0) {
        printk("Encoder 0: Failed to read reference angle (err %d)\n", ret);
        return ret;
    }
    initialized_0 = true;
    printk("Encoder 0: Initialized, reference = %u\n", reference_angle_0);


    /* Initialize encoder 1 on I2C1 */

    i2c_dev_1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));
    if (!device_is_ready(i2c_dev_1)) {
        printk("Encoder 1: I2C1 device not ready\n");
        return -ENODEV;
    }

    ret = mt6701_read_angle(i2c_dev_1, &reference_angle_1);
    if (ret < 0) {
        printk("Encoder 1: Failed to read reference angle (err %d)\n", ret);
        return ret;
    }
    initialized_1 = true;
    printk("Encoder 1: Initialized, reference = %u\n", reference_angle_1);

    return 0;
}

/**
 * @brief Read offset from reference for encoder 0
 */
int encoder_read_delta_0(int32_t *delta)
{
    uint16_t current_angle;
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

    *delta = calculate_offset(current_angle, reference_angle_0);
   
    return 0;
}

/**
 * @brief Read offset from reference for encoder 1
 */
int encoder_read_delta_1(int32_t *delta)
{
    uint16_t current_angle;
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

    *delta = calculate_offset(current_angle, reference_angle_1);
 
    return 0;
}

/**
 * @brief Read combined offset from both encoders
 */
int encoder_read_delta(int32_t *delta)
{
    int32_t delta_0 = 0;
    int32_t delta_1 = 0;

    if (!delta) {
        return -EINVAL;
    }

    encoder_read_delta_0(&delta_0);
    encoder_read_delta_1(&delta_1);

    *delta = delta_0 + delta_1;

    /* LED feedback */
    if (*delta != 0 && device_is_ready(led.port)) {
        gpio_pin_toggle_dt(&led);
    }

    return 0;
}

/**
 * @brief Reset encoder 0 reference to current position
 */
int encoder_reset_reference_0(void)
{
    if (!initialized_0) {
        return -ENODEV;
    }
    return mt6701_read_angle(i2c_dev_0, &reference_angle_0);
}

/**
 * @brief Reset encoder 1 reference to current position
 */
int encoder_reset_reference_1(void)
{
    if (!initialized_1) {
        return -ENODEV;
    }
    return mt6701_read_angle(i2c_dev_1, &reference_angle_1);
}
/**
 * @file encoder.h
 * @brief MT6701 Magnetic Encoder interface (Dual encoder support)
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/**
 * @brief Initialize both encoders
 * @return 0 on success, negative errno on failure
 */
int encoder_init(void);

/**
 * @brief Read delta from encoder 0 (I2C0)
 * @param delta Pointer to store delta value
 * @return 0 on success, negative errno on failure
 */
int encoder_read_delta_0(int32_t *delta);

/**
 * @brief Read delta from encoder 1 (I2C1)
 * @param delta Pointer to store delta value
 * @return 0 on success, negative errno on failure
 */
int encoder_read_delta_1(int32_t *delta);

/**
 * @brief Read combined delta from both encoders
 * 
 * For backwards compatibility - returns sum of both encoder deltas
 * 
 * @param delta Pointer to store combined delta value
 * @return 0 on success, negative errno on failure
 */
int encoder_read_delta(int32_t *delta);

#endif /* ENCODER_H */
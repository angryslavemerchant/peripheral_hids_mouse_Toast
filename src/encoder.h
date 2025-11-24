/**
 * @file encoder.h
 * @brief Rotary Encoder Hardware Abstraction
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

int encoder_init(void);
int encoder_read_delta(int32_t *delta);

#endif /* ENCODER_H */
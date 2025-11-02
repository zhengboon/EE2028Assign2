/**
 * @file    grove_5way.h
 * @brief   Minimal driver for the Grove 5-Way Tactile Switch (I2C Multi Switch)
 */

#ifndef GROVE_5WAY_H
#define GROVE_5WAY_H

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define GROVE5WAY_DEFAULT_ADDR   0x03U

typedef enum {
    GROVE5WAY_BTN_UP     = (1U << 0),
    GROVE5WAY_BTN_CENTER = (1U << 1),
    GROVE5WAY_BTN_LEFT   = (1U << 2),
    GROVE5WAY_BTN_RIGHT  = (1U << 3),
    GROVE5WAY_BTN_DOWN   = (1U << 4),
} Grove5Way_ButtonMask;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t address;
    uint8_t present;
    uint8_t last_pressed;
} Grove5Way_Handle;

typedef struct {
    uint8_t pressed;    /* bitmask of buttons currently pressed (1 = pressed) */
    uint8_t changed;    /* bitmask of buttons that toggled state since last poll */
} Grove5Way_Event;

bool Grove5Way_Init(Grove5Way_Handle *handle, I2C_HandleTypeDef *hi2c, uint8_t address);
bool Grove5Way_Poll(Grove5Way_Handle *handle, Grove5Way_Event *event);

#endif /* GROVE_5WAY_H */

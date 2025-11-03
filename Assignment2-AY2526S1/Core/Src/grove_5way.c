/**
 * @file    grove_5way.c
 * @brief   Minimal driver for the Grove 5-Way Tactile Switch (I2C Multi Switch)
 */

#include "grove_5way.h"
#include <string.h>

#define GROVE5WAY_CMD_GET_EVENT       0x01U
#define GROVE5WAY_CMD_EVENT_MODE      0x02U
#define GROVE5WAY_CMD_BLOCK_MODE      0x03U
#define GROVE5WAY_CMD_AUTO_SLEEP_OFF  0xB3U

#define GROVE5WAY_BUTTON_COUNT        5U
#define GROVE5WAY_I2C_TIMEOUT         100U
#define GROVE5WAY_EVENT_HEADER_BYTES  4U

static bool grove5way_write_cmd(Grove5Way_Handle *handle, uint8_t cmd);
static bool grove5way_read_event(Grove5Way_Handle *handle, uint8_t *button_bytes);

static const Grove5Way_ButtonMask g_button_map[GROVE5WAY_BUTTON_COUNT] = {
    GROVE5WAY_BTN_RIGHT,   /* raw index 0 -> physical RIGHT */
    GROVE5WAY_BTN_UP,      /* raw index 1 -> physical UP */
    GROVE5WAY_BTN_LEFT,    /* raw index 2 -> physical LEFT */
    GROVE5WAY_BTN_DOWN,    /* raw index 3 -> physical DOWN */
    GROVE5WAY_BTN_CENTER   /* raw index 4 -> physical CENTRE */
};

bool Grove5Way_Init(Grove5Way_Handle *handle, I2C_HandleTypeDef *hi2c, uint8_t address)
{
    if (handle == NULL || hi2c == NULL) {
        return false;
    }

    memset(handle, 0, sizeof(*handle));
    handle->hi2c   = hi2c;
    handle->address = address;

    (void)grove5way_write_cmd(handle, GROVE5WAY_CMD_AUTO_SLEEP_OFF);
    (void)grove5way_write_cmd(handle, GROVE5WAY_CMD_EVENT_MODE);

    uint8_t buttons[GROVE5WAY_BUTTON_COUNT];
    if (!grove5way_read_event(handle, buttons)) {
        handle->present = 0U;
        return false;
    }

    uint8_t pressed_mask = 0U;
    for (uint8_t i = 0U; i < GROVE5WAY_BUTTON_COUNT; ++i) {
        uint8_t raw = buttons[i] & 0x01U;
        if (raw == 0U) {
            pressed_mask |= g_button_map[i];
        }
    }
    handle->last_pressed = pressed_mask;
    handle->present = 1U;
    return true;
}

bool Grove5Way_Poll(Grove5Way_Handle *handle, Grove5Way_Event *event)
{
    if (handle == NULL || event == NULL || handle->hi2c == NULL) {
        return false;
    }
    if (!handle->present) {
        return false;
    }

    uint8_t buttons[GROVE5WAY_BUTTON_COUNT];
    if (!grove5way_read_event(handle, buttons)) {
        handle->present = 0U;
        return false;
    }

    uint8_t pressed_mask = 0U;
    for (uint8_t i = 0U; i < GROVE5WAY_BUTTON_COUNT; ++i) {
        uint8_t raw = buttons[i] & 0x01U;
        if (raw == 0U) {
            pressed_mask |= g_button_map[i];
        }
    }

    event->changed = (uint8_t)(pressed_mask ^ handle->last_pressed);
    event->pressed = pressed_mask;
    handle->last_pressed = pressed_mask;

    return true;
}

static bool grove5way_write_cmd(Grove5Way_Handle *handle, uint8_t cmd)
{
    if (handle == NULL || handle->hi2c == NULL) {
        return false;
    }
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(handle->hi2c,
                                                       (uint16_t)(handle->address << 1),
                                                       &cmd,
                                                       1U,
                                                       GROVE5WAY_I2C_TIMEOUT);
    return (status == HAL_OK);
}

static bool grove5way_read_event(Grove5Way_Handle *handle, uint8_t *button_bytes)
{
    if (handle == NULL || handle->hi2c == NULL || button_bytes == NULL) {
        return false;
    }

    uint8_t reg = GROVE5WAY_CMD_GET_EVENT;
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(handle->hi2c,
                                                       (uint16_t)(handle->address << 1),
                                                       &reg,
                                                       1U,
                                                       GROVE5WAY_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return false;
    }

    uint8_t payload[GROVE5WAY_EVENT_HEADER_BYTES + GROVE5WAY_BUTTON_COUNT];
    status = HAL_I2C_Master_Receive(handle->hi2c,
                                    (uint16_t)(handle->address << 1),
                                    payload,
                                    sizeof(payload),
                                    GROVE5WAY_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return false;
    }

    memcpy(button_bytes, &payload[GROVE5WAY_EVENT_HEADER_BYTES], GROVE5WAY_BUTTON_COUNT);
    return true;
}

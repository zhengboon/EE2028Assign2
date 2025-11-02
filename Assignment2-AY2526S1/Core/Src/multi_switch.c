/**
 * @file    multi_switch.c
 * @brief   Grove 5-Way Tactile & Grove 6-Position DIP Switch library for STM32
 *
 * @author  Ported from Arduino library by turmary <turmary@126.com>
 * @date    2025
 */

#include "multi_switch.h"
#include <string.h>

/* Private function prototypes */
static int readDev(GroveMultiSwitch_t *handle, uint8_t *data, int len);
static int writeDev(GroveMultiSwitch_t *handle, const uint8_t *data, int len);
static int readReg(GroveMultiSwitch_t *handle, uint8_t reg, uint8_t *data, int len);

/**
 * @brief Read data from device
 */
static int readDev(GroveMultiSwitch_t *handle, uint8_t *data, int len) {
    HAL_StatusTypeDef status;

    if (handle == NULL || data == NULL || len <= 0) {
        return -1;
    }

    status = HAL_I2C_Master_Receive(handle->hi2c,
                                    (handle->dev_addr << 1),
                                    data,
                                    len,
                                    GROVE_I2C_TIMEOUT);

    if (status != HAL_OK) {
        return -1;
    }

    return len;
}

/**
 * @brief Write data to device
 */
static int writeDev(GroveMultiSwitch_t *handle, const uint8_t *data, int len) {
    HAL_StatusTypeDef status;

    if (handle == NULL || data == NULL || len <= 0) {
        return -1;
    }

    status = HAL_I2C_Master_Transmit(handle->hi2c,
                                     (handle->dev_addr << 1),
                                     (uint8_t*)data,
                                     len,
                                     GROVE_I2C_TIMEOUT);

    if (status != HAL_OK) {
        return -1;
    }

    return len;
}

/**
 * @brief Read register from device
 */
static int readReg(GroveMultiSwitch_t *handle, uint8_t reg, uint8_t *data, int len) {
    int ret;

    if (handle == NULL || data == NULL || len <= 0) {
        return -1;
    }

    ret = writeDev(handle, &reg, 1);
    if (ret <= 0) {
        return -1;
    }

    HAL_Delay(1);

    ret = readDev(handle, data, len);
    return ret;
}

/**
 * @brief Initialize the Grove Multi Switch
 */
bool GroveMultiSwitch_Init(GroveMultiSwitch_t *handle, I2C_HandleTypeDef *hi2c, uint8_t addr) {
    if (handle == NULL || hi2c == NULL) {
        return false;
    }

    memset(handle, 0, sizeof(GroveMultiSwitch_t));
    handle->hi2c = hi2c;
    handle->dev_addr = addr;
    handle->dev_id = 0;
    handle->btn_count = 0;
    handle->version = 0;
    handle->initialized = false;

    if (GroveMultiSwitch_ProbeDevID(handle) == 0) {
        return false;
    }

    GroveMultiSwitch_GetDevVer(handle);

    handle->btn_count = GroveMultiSwitch_GetSwitchCount(handle);
    if (handle->btn_count == 0) {
        return false;
    }

    GroveMultiSwitch_SetEventMode(handle, true);

    handle->initialized = true;
    return true;
}

/**
 * @brief Probe device ID
 */
uint32_t GroveMultiSwitch_ProbeDevID(GroveMultiSwitch_t *handle) {
    uint32_t id = 0;
    uint8_t dummy;
    int tries;
    int ret;

    if (handle == NULL) {
        return 0;
    }

    for (tries = 4; tries > 0; tries--) {
        ret = readReg(handle, I2C_CMD_GET_DEV_ID, (uint8_t*)&id, sizeof(id));

        if (ret <= 0) {
            id = 0;
        }

        if (VID_VAL(id) == VID_MULTI_SWITCH) {
            handle->dev_id = id;
            return id;
        }

        readDev(handle, &dummy, 1);
    }

    handle->dev_id = id;
    return id;
}

/**
 * @brief Get device ID
 */
uint32_t GroveMultiSwitch_GetDevID(GroveMultiSwitch_t *handle) {
    if (handle == NULL) {
        return 0;
    }
    return handle->dev_id;
}

/**
 * @brief Get device firmware version string
 */
const char* GroveMultiSwitch_GetDevVer(GroveMultiSwitch_t *handle) {
    int ret;

    if (handle == NULL || handle->dev_id == 0) {
        return NULL;
    }

    ret = readReg(handle, I2C_CMD_TEST_GET_VER, (uint8_t*)handle->versions, sizeof(handle->versions));

    if (ret <= 0) {
        return NULL;
    }

    if (handle->versions[0] == 'v' && handle->versions[2] == '.') {
        handle->version = ((unsigned)handle->versions[1] - '0') * 10 +
                         ((unsigned)handle->versions[3] - '0');
    }

    return handle->versions;
}

/**
 * @brief Set device I2C address
 */
void GroveMultiSwitch_SetDevAddr(GroveMultiSwitch_t *handle, uint8_t addr) {
    uint8_t data[2];

    if (handle == NULL || handle->dev_id == 0) {
        return;
    }

    data[0] = I2C_CMD_SET_ADDR;
    data[1] = addr;

    writeDev(handle, data, sizeof(data));

    handle->dev_addr = addr;
}

/**
 * @brief Get number of switches/buttons
 */
int GroveMultiSwitch_GetSwitchCount(GroveMultiSwitch_t *handle) {
    if (handle == NULL) {
        return 0;
    }

    if (VID_VAL(handle->dev_id) != VID_MULTI_SWITCH) {
        return 0;
    }

    if (PID_VAL(handle->dev_id) == PID_5_WAY_TACTILE_SWITCH) {
        return 5;
    } else if (PID_VAL(handle->dev_id) == PID_6_POS_DIP_SWITCH) {
        return 6;
    }

    return 0;
}

/**
 * @brief Get button event
 */
ButtonEvent_t* GroveMultiSwitch_GetEvent(GroveMultiSwitch_t *handle) {
    static ButtonEvent_t event;
    int len;
    int ret;
    int i;

    if (handle == NULL || handle->dev_id == 0) {
        return NULL;
    }

    len = sizeof(uint32_t) + handle->btn_count;

    ret = readReg(handle, I2C_CMD_GET_DEV_EVENT, (uint8_t*)&event, len);

    if (ret <= 0) {
        return NULL;
    }

    if (handle->version > 1) {
        return &event;
    }

    if (!handle->initialized) {
        handle->last_event = event;
    }

    for (i = 0; i < BUTTON_MAX; i++) {
        event.button[i] &= ~BTN_EV_LEVEL_CHANGED;

        if ((event.button[i] ^ handle->last_event.button[i]) & BTN_EV_RAW_STATUS) {
            event.button[i] |= BTN_EV_LEVEL_CHANGED;
            event.event |= BTN_EV_HAS_EVENT;
        }
    }

    handle->last_event = event;
    return &event;
}

/**
 * @brief Set event detection mode
 */
bool GroveMultiSwitch_SetEventMode(GroveMultiSwitch_t *handle, bool enable) {
    uint8_t data;
    int ret;

    if (handle == NULL || handle->dev_id == 0) {
        return false;
    }

    data = enable ? I2C_CMD_EVENT_DET_MODE : I2C_CMD_BLOCK_DET_MODE;

    ret = writeDev(handle, &data, sizeof(data));

    return (ret > 0);
}

/**
 * @brief Enable/disable auto sleep mode
 */
bool GroveMultiSwitch_SetAutoSleep(GroveMultiSwitch_t *handle, bool enable) {
    uint8_t data;
    int ret;

    if (handle == NULL || handle->dev_id == 0) {
        return false;
    }

    data = enable ? I2C_CMD_AUTO_SLEEP_ON : I2C_CMD_AUTO_SLEEP_OFF;

    ret = writeDev(handle, &data, sizeof(data));

    return (ret > 0);
}

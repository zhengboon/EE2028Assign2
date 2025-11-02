/**
 * @file    multi_switch.h
 * @brief   Grove 5-Way Tactile & Grove 6-Position DIP Switch library for STM32
 *
 * @author  Original: turmary <turmary@126.com>
 *          Ported to STM32 C
 * @date    2025
 *
 * The MIT License (MIT)
 */

#ifndef MULTI_SWITCH_H
#define MULTI_SWITCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
   I2C COMMAND DEFINITIONS - CRITICAL: THESE MUST MATCH THE .C FILE
   ============================================================================ */

#define I2C_CMD_GET_DEV_ID          0x00    // Gets device ID information
#define I2C_CMD_GET_DEV_EVENT       0x01    // Gets device event status
#define I2C_CMD_EVENT_DET_MODE      0x02    // Enable button event detect mode
#define I2C_CMD_BLOCK_DET_MODE      0x03    // Enable button block detect mode
#define I2C_CMD_AUTO_SLEEP_ON       0xB2    // Enable device auto sleep mode
#define I2C_CMD_AUTO_SLEEP_OFF      0xB3    // Disable device auto sleep mode
#define I2C_CMD_SET_ADDR            0xC0    // Sets device I2C address
#define I2C_CMD_RST_ADDR            0xC1    // Resets device I2C address
#define I2C_CMD_TEST_TX_RX_ON       0xE0    // Enable TX RX pin test mode
#define I2C_CMD_TEST_TX_RX_OFF      0xE1    // Disable TX RX pin test mode
#define I2C_CMD_TEST_GET_VER        0xE2    // Use to get software version
#define I2C_CMD_GET_DEVICE_UID      0xF1    // Use to get chip ID

/* ============================================================================
   DEVICE IDENTIFICATION
   ============================================================================ */

/* Default I2C address */
#define GROVE_MULTI_SWITCH_DEF_I2C_ADDR     0x03
#define GROVE_MULTI_SWITCH_ADDR             0x03  // Alias for compatibility

/* Vendor and Product IDs */
#define VID_MULTI_SWITCH                    0x2886
#define PID_5_WAY_TACTILE_SWITCH            0x0001
#define PID_6_POS_DIP_SWITCH                0x0002

/* Macros to extract VID and PID */
#define VID_VAL(x)                          ((x) & 0xFFFF)
#define PID_VAL(x)                          (((x) >> 16) & 0xFFFF)

/* ============================================================================
   BUTTON EVENT FLAGS
   ============================================================================ */

#define BTN_EV_NO_EVENT                     0x00000000UL
#define BTN_EV_HAS_EVENT                    0x80000000UL
#define BTN_EV_RAW_STATUS                   0x01
#define BTN_EV_SINGLE_CLICK                 0x02
#define BTN_EV_DOUBLE_CLICK                 0x04
#define BTN_EV_LONG_PRESS                   0x08
#define BTN_EV_LEVEL_CHANGED                0x02

/* Button state definitions */
#define RAW_DIGITAL_BTN_PRESSED             0
#define RAW_DIP_SWITCH_ON                   0

/* ============================================================================
   CONFIGURATION
   ============================================================================ */

#define BUTTON_MAX                          6
#define GROVE_MULTI_SWITCH_VERSIONS_SZ      10
#define GROVE_I2C_TIMEOUT                   100

/* ============================================================================
   DATA STRUCTURES
   ============================================================================ */

typedef struct {
    uint32_t event;
    uint8_t button[BUTTON_MAX];
} ButtonEvent_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t dev_addr;
    uint32_t dev_id;
    uint8_t btn_count;
    uint8_t version;
    char versions[GROVE_MULTI_SWITCH_VERSIONS_SZ];
    ButtonEvent_t last_event;
    bool initialized;
} GroveMultiSwitch_t;

/* ============================================================================
   FUNCTION PROTOTYPES
   ============================================================================ */

bool GroveMultiSwitch_Init(GroveMultiSwitch_t *handle, I2C_HandleTypeDef *hi2c, uint8_t addr);
uint32_t GroveMultiSwitch_ProbeDevID(GroveMultiSwitch_t *handle);
uint32_t GroveMultiSwitch_GetDevID(GroveMultiSwitch_t *handle);
const char* GroveMultiSwitch_GetDevVer(GroveMultiSwitch_t *handle);
void GroveMultiSwitch_SetDevAddr(GroveMultiSwitch_t *handle, uint8_t addr);
int GroveMultiSwitch_GetSwitchCount(GroveMultiSwitch_t *handle);
ButtonEvent_t* GroveMultiSwitch_GetEvent(GroveMultiSwitch_t *handle);
bool GroveMultiSwitch_SetEventMode(GroveMultiSwitch_t *handle, bool enable);
bool GroveMultiSwitch_SetAutoSleep(GroveMultiSwitch_t *handle, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MULTI_SWITCH_H */

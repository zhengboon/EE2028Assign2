/**
 * @file    menu_system.h
 * @brief   Menu system for adjusting game parameters using Grove 5-Way Switch
 * @author  Your Name
 * @date    2025
 */

#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "multi_switch.h"

/* Menu States */
typedef enum {
    MENU_CLOSED = 0,           // Menu is not active
    MENU_VARIABLE_SELECT,      // Selecting which variable to adjust
    MENU_VALUE_ADJUST          // Adjusting the selected variable's value
} menu_state_t;

/* Parameter IDs for different games */
typedef enum {
    // Game 1: RLGL parameters
    PARAM_ACCEL_THRESHOLD = 0,
    PARAM_GYRO_THRESHOLD,
    
    // Game 2: Catch & Run parameters
    PARAM_MAG_THRESH_LOW,
    PARAM_MAG_THRESH_MED,
    PARAM_MAG_THRESH_HIGH,
    
    // Game 3: Arrow game parameter
    PARAM_SEQUENCE_TIME,
    
    PARAM_COUNT  // Total number of parameters
} param_id_t;

/* Parameter definition structure */
typedef struct {
    const char *name;          // Display name
    float *value_ptr;          // Pointer to actual variable
    float min_value;           // Minimum allowed value
    float max_value;           // Maximum allowed value
    float step;                // Increment/decrement step
    uint8_t game_id;           // Which game this belongs to (0=RLGL, 1=Catch, 2=Arrow)
} param_def_t;

/* Menu handle structure */
typedef struct {
    menu_state_t state;
    param_id_t current_param;
    uint8_t current_game;
    GroveMultiSwitch_t *switch_handle;
    uint32_t last_update_ms;
} menu_handle_t;

/* Function prototypes */

/**
 * @brief Initialize the menu system
 * @param menu Pointer to menu handle
 * @param switch_handle Pointer to initialized Grove switch handle
 * @param game_id Current game ID (0=RLGL, 1=Catch, 2=Arrow)
 */
void Menu_Init(menu_handle_t *menu, GroveMultiSwitch_t *switch_handle, uint8_t game_id);

/**
 * @brief Process menu input and update state
 * @param menu Pointer to menu handle
 * @return true if menu is active, false if closed
 */
bool Menu_Process(menu_handle_t *menu);

/**
 * @brief Update which game's parameters to show
 * @param menu Pointer to menu handle
 * @param game_id Game ID (0=RLGL, 1=Catch, 2=Arrow)
 */
void Menu_SetGame(menu_handle_t *menu, uint8_t game_id);

/**
 * @brief Get current menu state
 * @param menu Pointer to menu handle
 * @return Current menu state
 */
menu_state_t Menu_GetState(menu_handle_t *menu);

/**
 * @brief Get display string for current menu state
 * @param menu Pointer to menu handle
 * @param buffer Buffer to store display string
 * @param buffer_size Size of buffer
 */
void Menu_GetDisplayString(menu_handle_t *menu, char *buffer, size_t buffer_size);

#endif /* MENU_SYSTEM_H */

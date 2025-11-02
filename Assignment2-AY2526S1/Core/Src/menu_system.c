/**
 * @file    menu_system.c
 * @brief   Menu system implementation for adjusting game parameters
 */

#include "menu_system.h"
#include <stdio.h>
#include <string.h>

extern void printu(const char *fmt, ...);

/* External variable references - these should match your main.c */
extern float ACCEL_THRESHOLD_MS2;
extern float GYRO_THRESHOLD_DPS;
extern int MAG_THRESH[3];
extern uint32_t SEQUENCE_TIME_MS;  // Add this to your main.c for game 3

/* Parameter definitions array */
static param_def_t params[PARAM_COUNT] = {
    // Game 1: RLGL
    {"Accel Thresh", &ACCEL_THRESHOLD_MS2, 5.0f, 50.0f, 1.0f, 0},
    {"Gyro Thresh", &GYRO_THRESHOLD_DPS, 50.0f, 500.0f, 10.0f, 0},
    
    // Game 2: Catch & Run (using float pointers to int array)
    {"Mag Low", NULL, 100.0f, 2000.0f, 50.0f, 1},
    {"Mag Med", NULL, 500.0f, 5000.0f, 100.0f, 1},
    {"Mag High", NULL, 2000.0f, 20000.0f, 500.0f, 1},
    
    // Game 3: Arrow
    {"Seq Time(ms)", NULL, 500.0f, 10000.0f, 100.0f, 2}
};

/* Helper function to get float value for mag thresholds */
static float get_mag_value(int index) {
    extern int MAG_THRESH[3];
    return (float)MAG_THRESH[index];
}

/* Helper function to set mag threshold value */
static void set_mag_value(int index, float value) {
    extern int MAG_THRESH[3];
    MAG_THRESH[index] = (int)value;
}

/* Helper function to get sequence time */
static float get_sequence_time(void) {
    extern uint32_t SEQUENCE_TIME_MS;
    return (float)SEQUENCE_TIME_MS;
}

/* Helper function to set sequence time */
static void set_sequence_time(float value) {
    extern uint32_t SEQUENCE_TIME_MS;
    SEQUENCE_TIME_MS = (uint32_t)value;
}

/* Initialize menu system */
void Menu_Init(menu_handle_t *menu, GroveMultiSwitch_t *switch_handle, uint8_t game_id) {
    if (menu == NULL || switch_handle == NULL) return;
    
    menu->state = MENU_CLOSED;
    menu->current_param = PARAM_ACCEL_THRESHOLD;
    menu->current_game = game_id;
    menu->switch_handle = switch_handle;
    menu->last_update_ms = 0;
}

/* Update which game's parameters to show */
void Menu_SetGame(menu_handle_t *menu, uint8_t game_id) {
    if (menu == NULL) return;
    
    menu->current_game = game_id;
    
    // Find first parameter for this game
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (params[i].game_id == game_id) {
            menu->current_param = (param_id_t)i;
            break;
        }
    }
}

/* Get current parameter value */
static float get_param_value(param_id_t param_id) {
    if (param_id >= PARAM_COUNT) return 0.0f;
    
    param_def_t *param = &params[param_id];
    
    // Handle special cases for non-float parameters
    if (param_id == PARAM_MAG_THRESH_LOW) return get_mag_value(0);
    if (param_id == PARAM_MAG_THRESH_MED) return get_mag_value(1);
    if (param_id == PARAM_MAG_THRESH_HIGH) return get_mag_value(2);
    if (param_id == PARAM_SEQUENCE_TIME) return get_sequence_time();
    
    // Normal float parameter
    if (param->value_ptr != NULL) {
        return *(param->value_ptr);
    }
    
    return 0.0f;
}

/* Set current parameter value */
static void set_param_value(param_id_t param_id, float value) {
    if (param_id >= PARAM_COUNT) return;
    
    param_def_t *param = &params[param_id];
    
    // Clamp value to min/max
    if (value < param->min_value) value = param->min_value;
    if (value > param->max_value) value = param->max_value;
    
    // Handle special cases
    if (param_id == PARAM_MAG_THRESH_LOW) {
        set_mag_value(0, value);
        return;
    }
    if (param_id == PARAM_MAG_THRESH_MED) {
        set_mag_value(1, value);
        return;
    }
    if (param_id == PARAM_MAG_THRESH_HIGH) {
        set_mag_value(2, value);
        return;
    }
    if (param_id == PARAM_SEQUENCE_TIME) {
        set_sequence_time(value);
        return;
    }
    
    // Normal float parameter
    if (param->value_ptr != NULL) {
        *(param->value_ptr) = value;
    }
}

/* Find next parameter for current game */
static param_id_t get_next_param(menu_handle_t *menu, int8_t direction) {
    param_id_t current = menu->current_param;
    param_id_t next = current;
    
    // Search in the specified direction
    for (int i = 0; i < PARAM_COUNT; i++) {
        next = (param_id_t)((next + direction + PARAM_COUNT) % PARAM_COUNT);
        if (params[next].game_id == menu->current_game) {
            return next;
        }
    }
    
    return current;  // Stay at current if no other param found
}

/* Process menu input and update state */
bool Menu_Process(menu_handle_t *menu) {
    if (menu == NULL || menu->switch_handle == NULL) return false;
    
    ButtonEvent_t *event = GroveMultiSwitch_GetEvent(menu->switch_handle);
    if (event == NULL) return (menu->state != MENU_CLOSED);
    
    uint32_t now = HAL_GetTick();
    
    // Debounce - only process every 200ms
    if ((now - menu->last_update_ms) < 200) {
        return (menu->state != MENU_CLOSED);
    }
    
    // Check for button presses (level changed + currently pressed)
    bool down_pressed = (event->button[4] & BTN_EV_LEVEL_CHANGED) && 
                       ((event->button[4] & BTN_EV_RAW_STATUS) == RAW_DIGITAL_BTN_PRESSED);
    bool up_pressed = (event->button[0] & BTN_EV_LEVEL_CHANGED) && 
                     ((event->button[0] & BTN_EV_RAW_STATUS) == RAW_DIGITAL_BTN_PRESSED);
    bool left_pressed = (event->button[2] & BTN_EV_LEVEL_CHANGED) && 
                       ((event->button[2] & BTN_EV_RAW_STATUS) == RAW_DIGITAL_BTN_PRESSED);
    bool right_pressed = (event->button[3] & BTN_EV_LEVEL_CHANGED) && 
                        ((event->button[3] & BTN_EV_RAW_STATUS) == RAW_DIGITAL_BTN_PRESSED);
    
    // State machine
    switch (menu->state) {
        case MENU_CLOSED:
            // DOWN button opens menu
            if (down_pressed) {
                menu->state = MENU_VARIABLE_SELECT;
                menu->last_update_ms = now;
                printu("\r\n=== MENU OPENED ===\r\n");
                printu("Use LEFT/RIGHT to select parameter\r\n");
                printu("Press DOWN to adjust value\r\n");
                printu("Current: %s = %.1f\r\n", 
                       params[menu->current_param].name,
                       get_param_value(menu->current_param));
            }
            break;
            
        case MENU_VARIABLE_SELECT:
            if (left_pressed) {
                // Navigate to previous parameter
                menu->current_param = get_next_param(menu, -1);
                menu->last_update_ms = now;
                printu("Selected: %s = %.1f\r\n", 
                       params[menu->current_param].name,
                       get_param_value(menu->current_param));
            }
            else if (right_pressed) {
                // Check if this is the last parameter for current game
                param_id_t next = get_next_param(menu, 1);
                if (next == menu->current_param) {
                    // Exit menu (we're at the last parameter)
                    menu->state = MENU_CLOSED;
                    menu->last_update_ms = now;
                    printu("=== MENU CLOSED ===\r\n\r\n");
                } else {
                    // Navigate to next parameter
                    menu->current_param = next;
                    menu->last_update_ms = now;
                    printu("Selected: %s = %.1f\r\n", 
                           params[menu->current_param].name,
                           get_param_value(menu->current_param));
                }
            }
            else if (down_pressed) {
                // Enter value adjustment mode
                menu->state = MENU_VALUE_ADJUST;
                menu->last_update_ms = now;
                printu(">>> Adjusting: %s\r\n", params[menu->current_param].name);
                printu("Use UP/DOWN to change value\r\n");
                printu("Press DOWN (center) to confirm\r\n");
            }
            break;
            
        case MENU_VALUE_ADJUST:
            if (up_pressed) {
                // Increase value
                float current = get_param_value(menu->current_param);
                float step = params[menu->current_param].step;
                set_param_value(menu->current_param, current + step);
                menu->last_update_ms = now;
                printu("%s = %.1f\r\n", 
                       params[menu->current_param].name,
                       get_param_value(menu->current_param));
            }
            else if (down_pressed) {
                // Decrease value (we use down for both decrease and confirm)
                float current = get_param_value(menu->current_param);
                float step = params[menu->current_param].step;
                float new_val = current - step;
                
                // If we're at minimum, treat as confirm instead
                if (new_val < params[menu->current_param].min_value) {
                    menu->state = MENU_VARIABLE_SELECT;
                    menu->last_update_ms = now;
                    printu(">>> Value confirmed: %.1f\r\n", current);
                    printu("Use LEFT/RIGHT to select parameter\r\n");
                } else {
                    set_param_value(menu->current_param, new_val);
                    menu->last_update_ms = now;
                    printu("%s = %.1f\r\n", 
                           params[menu->current_param].name,
                           get_param_value(menu->current_param));
                }
            }
            else if (left_pressed || right_pressed) {
                // Cancel adjustment and go back
                menu->state = MENU_VARIABLE_SELECT;
                menu->last_update_ms = now;
                printu(">>> Value adjustment cancelled\r\n");
            }
            break;
    }
    
    return (menu->state != MENU_CLOSED);
}

/* Get current menu state */
menu_state_t Menu_GetState(menu_handle_t *menu) {
    if (menu == NULL) return MENU_CLOSED;
    return menu->state;
}

/* Get display string for current menu state */
void Menu_GetDisplayString(menu_handle_t *menu, char *buffer, size_t buffer_size) {
    if (menu == NULL || buffer == NULL || buffer_size == 0) return;
    
    if (menu->state == MENU_CLOSED) {
        snprintf(buffer, buffer_size, "Press DOWN for menu");
        return;
    }
    
    param_def_t *param = &params[menu->current_param];
    float value = get_param_value(menu->current_param);
    
    if (menu->state == MENU_VARIABLE_SELECT) {
        snprintf(buffer, buffer_size, "[SEL] %s: %.1f", param->name, value);
    } else {
        snprintf(buffer, buffer_size, "[ADJ] %s: %.1f", param->name, value);
    }
}

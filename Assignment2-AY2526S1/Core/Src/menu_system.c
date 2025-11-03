/**
 * @file    menu_system.c
 * @brief   Menu system implementation for adjusting game parameters
 */

#include "menu_system.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern void printu(const char *fmt, ...);
extern void Menu_RenderStatus(const char *line1, const char *line2);

/* External variable references - these should match your main.c */
extern float ACCEL_THRESHOLD_MS2;
extern float GYRO_THRESHOLD_DPS;
extern int MAG_THRESH[3];
extern uint8_t g_arrow_length_setting;
extern uint32_t g_arrow_time_setting_ms;

/* Parameter definitions array */
static param_def_t params[PARAM_COUNT] = {
    // Game 1: RLGL
    {"Accel Thresh", &ACCEL_THRESHOLD_MS2, 5.0f, 50.0f, 1.0f, 0.2f, 0},
    {"Gyro Thresh", &GYRO_THRESHOLD_DPS, 50.0f, 500.0f, 10.0f, 2.0f, 0},
    
    // Game 2: Catch & Run (using float pointers to int array)
    {"Mag Low", NULL, 100.0f, 2000.0f, 50.0f, 10.0f, 1},
    {"Mag Med", NULL, 500.0f, 5000.0f, 100.0f, 20.0f, 1},
    {"Mag High", NULL, 2000.0f, 20000.0f, 500.0f, 100.0f, 1},
    
    // Game 3: Arrow
    {"Arrow Count", NULL, 1.0f, 32.0f, 1.0f, 0.0f, 2},
    {"Seq Time(s)", NULL, 1.0f, 9.0f, 1.0f, 0.0f, 2},
    
    // Exit option (available for all games)
    {"Exit Menu", NULL, 0.0f, 0.0f, 0.0f, 0.0f, 255}
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

static float get_arrow_count(void) {
    extern uint8_t g_arrow_length_setting;
    return (float)g_arrow_length_setting;
}

static void set_arrow_count(float value) {
    extern uint8_t g_arrow_length_setting;
    if (value < 1.0f) value = 1.0f;
    if (value > 32.0f) value = 32.0f;
    g_arrow_length_setting = (uint8_t)(value + 0.5f);
    if (g_arrow_length_setting < 1U) g_arrow_length_setting = 1U;
}

/* Helper function to get sequence time */
static float get_sequence_time(void) {
    extern uint32_t g_arrow_time_setting_ms;
    return ((float)g_arrow_time_setting_ms) / 1000.0f;
}

/* Helper function to set sequence time */
static void set_sequence_time(float value) {
    extern uint32_t g_arrow_time_setting_ms;
    if (value < 1.0f) value = 1.0f;
    if (value > 9.0f) value = 9.0f;
    value = roundf(value);
    g_arrow_time_setting_ms = (uint32_t)(value * 1000.0f);
}

/* Initialize menu system */
void Menu_Init(menu_handle_t *menu, Grove5Way_Handle *switch_handle, uint8_t game_id) {
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
    if (param_id == PARAM_ARROW_COUNT) return get_arrow_count();
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
    if (param_id == PARAM_ARROW_COUNT) {
        set_arrow_count(value);
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
        if ((params[next].game_id == menu->current_game) ||
            (params[next].game_id == 255U)) {
            return next;
        }
    }
    
    return current;  // Stay at current if no other param found
}

static void menu_render_selected(menu_handle_t *menu)
{
    char line1[32];
    char line2[32];
    snprintf(line1, sizeof(line1), "Menu");

    const char *name = params[menu->current_param].name;
    if (menu->current_param == PARAM_EXIT) {
        printu("Selected: Exit Menu\r\n");
        snprintf(line2, sizeof(line2), "Exit");
    } else {
        float value = get_param_value(menu->current_param);
        if ((menu->current_param == PARAM_ARROW_COUNT) ||
            (menu->current_param == PARAM_SEQUENCE_TIME)) {
            printu("Selected: %s = %.0f\r\n", name, value);
        } else {
            printu("Selected: %s = %.1f\r\n", name, value);
        }
        snprintf(line2, sizeof(line2), "%s", name);
    }

    Menu_RenderStatus(line1, line2);
}

static void menu_render_adjust(menu_handle_t *menu, const char *header)
{
    if (menu->current_param == PARAM_EXIT) {
        menu_render_selected(menu);
        return;
    }

    if (header == NULL) {
        header = "Adjusting:";
    }

    float value = get_param_value(menu->current_param);
    const char *param_name = params[menu->current_param].name;
    if ((menu->current_param == PARAM_ARROW_COUNT) || (menu->current_param == PARAM_SEQUENCE_TIME)) {
        printu("%s %s = %.0f\r\n", header, param_name, value);
    } else {
        printu("%s %s = %.1f\r\n", header, param_name, value);
    }

    char line1[32];
    char line2[32];
    snprintf(line1, sizeof(line1), "%s", param_name);
    if ((menu->current_param == PARAM_ARROW_COUNT) || (menu->current_param == PARAM_SEQUENCE_TIME)) {
        snprintf(line2, sizeof(line2), "%.0f", value);
    } else {
        snprintf(line2, sizeof(line2), "%.1f", value);
    }
    Menu_RenderStatus(line1, line2);
}

/* Process menu input and update state */
bool Menu_Process(menu_handle_t *menu) {
    if (menu == NULL || menu->switch_handle == NULL) return false;
    
    Grove5Way_Event evt;
    if (!Grove5Way_Poll(menu->switch_handle, &evt)) {
        return (menu->state != MENU_CLOSED);
    }
    
    uint8_t pressed_edges = evt.pressed & evt.changed;
    if (pressed_edges == 0U) {
        return (menu->state != MENU_CLOSED);
    }
    
    uint32_t now = HAL_GetTick();
    
    // Debounce - only process every 200ms
    if ((now - menu->last_update_ms) < 200) {
        return (menu->state != MENU_CLOSED);
    }
    
    if (pressed_edges != 0U) {
        char msg[64];
        size_t off = 0;
        off += snprintf(msg + off, sizeof(msg) - off, "5-way pressed:");
        if (pressed_edges & GROVE5WAY_BTN_UP)     off += snprintf(msg + off, sizeof(msg) - off, " UP");
        if (pressed_edges & GROVE5WAY_BTN_DOWN)   off += snprintf(msg + off, sizeof(msg) - off, " DOWN");
        if (pressed_edges & GROVE5WAY_BTN_LEFT)   off += snprintf(msg + off, sizeof(msg) - off, " LEFT");
        if (pressed_edges & GROVE5WAY_BTN_RIGHT)  off += snprintf(msg + off, sizeof(msg) - off, " RIGHT");
        if (pressed_edges & GROVE5WAY_BTN_CENTER) off += snprintf(msg + off, sizeof(msg) - off, " CENTER");
        printu("%s\r\n", msg);
    }
    
    bool down_pressed  = (pressed_edges & GROVE5WAY_BTN_DOWN)   != 0U;
    bool up_pressed    = (pressed_edges & GROVE5WAY_BTN_UP)     != 0U;
    bool left_pressed  = (pressed_edges & GROVE5WAY_BTN_LEFT)   != 0U;
    bool right_pressed = (pressed_edges & GROVE5WAY_BTN_RIGHT)  != 0U;
    bool center_pressed = (pressed_edges & GROVE5WAY_BTN_CENTER) != 0U;
    
    // State machine
    switch (menu->state) {
        case MENU_CLOSED: {
            // CENTER button opens menu
            if (center_pressed) {
                menu->state = MENU_VARIABLE_SELECT;
                menu->last_update_ms = now;
                printu("\r\n=== MENU OPENED ===\r\n");
                printu("Use LEFT/RIGHT to select\r\n");
                printu("CENTER adjusts, BLUE exits\r\n");
                menu_render_selected(menu);
            }
            break;
        }
            
        case MENU_VARIABLE_SELECT: {
            if (center_pressed) {
                if (menu->current_param == PARAM_EXIT) {
                    Menu_Close(menu);
                } else {
                    menu->state = MENU_VALUE_ADJUST;
                    menu->last_update_ms = now;
                    printu(">>> Adjusting: %s\r\n", params[menu->current_param].name);
                    printu("UP/DOWN coarse, LEFT/RIGHT fine\r\n");
                    printu("Press CENTER to apply, BLUE to exit\r\n");
                    menu_render_adjust(menu, "Adjusting:");
                }
            } else if (left_pressed) {
                menu->current_param = get_next_param(menu, -1);
                menu->last_update_ms = now;
                menu_render_selected(menu);
            } else if (right_pressed) {
                menu->current_param = get_next_param(menu, 1);
                menu->last_update_ms = now;
                menu_render_selected(menu);
            }
            break;
        }
            
        case MENU_VALUE_ADJUST: {
            if (menu->current_param == PARAM_EXIT) {
                menu->state = MENU_VARIABLE_SELECT;
                menu->last_update_ms = now;
                menu_render_selected(menu);
                break;
            }

            if (center_pressed) {
                menu->state = MENU_VARIABLE_SELECT;
                menu->last_update_ms = now;
                menu_render_selected(menu);
                break;
            }

            float current = get_param_value(menu->current_param);
            float coarse = params[menu->current_param].coarse_step;
            float fine   = params[menu->current_param].fine_step;
            bool updated = false;

            if (coarse > 0.0f) {
                if (up_pressed)   { current += coarse; updated = true; }
                if (down_pressed) { current -= coarse; updated = true; }
            }
            if (fine > 0.0f) {
                if (right_pressed) { current += fine; updated = true; }
                if (left_pressed)  { current -= fine; updated = true; }
            }

            if (updated) {
                set_param_value(menu->current_param, current);
                menu->last_update_ms = now;
                menu_render_adjust(menu, "Adjusting:");
            }
            break;
        }
    }
    
    return (menu->state != MENU_CLOSED);
}

/* Get current menu state */
menu_state_t Menu_GetState(menu_handle_t *menu) {
    if (menu == NULL) return MENU_CLOSED;
    return menu->state;
}

void Menu_Open(menu_handle_t *menu) {
    if (menu == NULL) {
        return;
    }
    if (menu->state != MENU_CLOSED) {
        return;
    }

    menu->state = MENU_VARIABLE_SELECT;
    menu->last_update_ms = HAL_GetTick();
    printu("\r\n=== MENU OPENED ===\r\n");
    printu("Use LEFT/RIGHT to select\r\n");
    printu("CENTER adjusts, BLUE exits\r\n");
    menu_render_selected(menu);
}

void Menu_Close(menu_handle_t *menu) {
    if (menu == NULL) {
        return;
    }
    if (menu->state == MENU_CLOSED) {
        return;
    }

    menu->state = MENU_CLOSED;
    menu->last_update_ms = HAL_GetTick();
    printu("=== MENU CLOSED ===\r\n\r\n");
    Menu_RenderStatus("MENU CLOSED", "Press CENTER to open");
    Menu_SetGame(menu, menu->current_game);
}

/* Get display string for current menu state */
void Menu_GetDisplayString(menu_handle_t *menu, char *buffer, size_t buffer_size) {
    if (menu == NULL || buffer == NULL || buffer_size == 0) return;
    
    if (menu->state == MENU_CLOSED) {
        snprintf(buffer, buffer_size, "CENTER=open menu");
        return;
    }
    
    param_def_t *param = &params[menu->current_param];
    if (menu->current_param == PARAM_EXIT) {
        if (menu->state == MENU_VARIABLE_SELECT) {
            snprintf(buffer, buffer_size, "[SEL] Exit (BLUE)");
        } else {
            snprintf(buffer, buffer_size, "[ADJ] Exit (CENTER)");
        }
        return;
    }
    float value = get_param_value(menu->current_param);
    
    if (menu->current_param == PARAM_ARROW_COUNT || menu->current_param == PARAM_SEQUENCE_TIME) {
        if (menu->state == MENU_VARIABLE_SELECT) {
            snprintf(buffer, buffer_size, "[SEL] %s: %.0f", param->name, value);
        } else {
            snprintf(buffer, buffer_size, "[ADJ] %s: %.0f", param->name, value);
        }
    } else {
        if (menu->state == MENU_VARIABLE_SELECT) {
            snprintf(buffer, buffer_size, "[SEL] %s: %.1f", param->name, value);
        } else {
            snprintf(buffer, buffer_size, "[ADJ] %s: %.1f", param->name, value);
        }
    }
}

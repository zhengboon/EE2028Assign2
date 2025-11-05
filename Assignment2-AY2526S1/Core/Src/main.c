
#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>   /* for abs() */
#include "stm32l4xx_hal.h"
#include <stdlib.h>
#include <stdint.h>
#include "images.h"
#include "buzzer.h"
#include "animations.h"

/* BSP drivers */
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_accelero.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_gyro.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_magneto.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_hsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_psensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_nfctag.h"
#include "ssd1306.h"
#include "ht16k33.h"

#include "fonts.h"
#include "bitmap.h"
#include "horse_anim.h"
#include "grove_5way.h"
#include "menu_system.h"
/* ========= UART ========= */
UART_HandleTypeDef huart1;
static void UART1_Init(void);
static void MX_GPIO_Init(void);
void MX_I2C1_Init(void);
static void I2C_Scan(I2C_HandleTypeDef *hi2c);
static void NFC_PrintDetected(void);

I2C_HandleTypeDef hi2c1;
static HT16K33_HandleTypeDef hmatrix;
void printu(const char *fmt, ...)
{
    char buf[256];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) HAL_UART_Transmit(&huart1, (uint8_t*)buf, (uint16_t)n, HAL_MAX_DELAY);
}

/* ========= Game / Role defs ========= */
typedef enum { GAME_RLGL = 0, GAME_CATCH = 1, GAME_ARROW = 2} game_t;
typedef enum { ROLE_PLAYER = 1, ROLE_ENFORCER = 2 } role_t;
typedef enum { PHASE_GREEN = 0, PHASE_RED = 1 } rlgl_phase_t;

static void GameOver_Trigger(game_t game, const char *message);
static uint8_t GameReset_Attempt(game_t game, uint32_t now, int catch_level);
#define ARROW_MAX_SEQUENCE 32U

static void Arrow_ShowDigit(uint8_t digit);
static void Arrow_InitDigits(void);
static void Arrow_StartGame(void);

static void Arrow_Stop(void);
static void Arrow_RenderSequence(void);
static void Arrow_UpdateMatrix(uint32_t now);
static void Arrow_HandleInput(uint8_t edges);
static void Arrow_Fail(const char *message);
static void Arrow_Success(void);

/* Thresholds */
float ACCEL_THRESHOLD_MS2   = 15.0f;
float GYRO_THRESHOLD_DPS    = 150.0f;
float TEMP_THRESH_C         = 30.0f;
float HUMID_THRESH_PCT      = 70.0f;
float PRESS_THRESH_HPA      = 1013.0f;

int MAG_THRESH[3] = { 500, 2000, 10000 };
uint8_t g_arrow_length_setting = 5U;
uint32_t g_arrow_time_setting_ms = 5000U;

/* ================= Calibration Variables ================= */
float gyro_offset_x = 0.0f;
float gyro_offset_y = 0.0f;
float gyro_offset_z = 0.0f;

/* ========= Global state ========= */
static volatile game_t g_game = GAME_RLGL;
static role_t  g_role = ROLE_PLAYER;

/* RLGL */
static rlgl_phase_t g_phase = PHASE_GREEN;
static uint8_t  g_gameOver = 0;
static uint32_t t_phaseSwitch = 0;
static uint32_t t_envRLGL = 0;
static uint32_t t_ledHB = 0;
static uint32_t t_motionRLGL = 0;

/* Catch & Run */
static int      g_mag_baseline = 0;
static uint32_t t_envCatch = 0;
static uint8_t was_temp_high=0, was_hum_high=0, was_press_high=0;

typedef enum {
    CATCH_IDLE = 0,
    CATCH_ALERT,
    CATCH_LOCKOUT,
    CATCH_WAIT_RESET
} catch_state_t;

static catch_state_t g_catch_state = CATCH_IDLE;
static uint32_t      g_catch_event_start = 0;
static int8_t        g_catch_blink_level = -1;

/* Menu / Grove switch */
static Grove5Way_Handle   g_switch;
static menu_handle_t      g_menu;
static uint8_t            g_switch_ready = 0;
static uint32_t           t_menuPoll = 0;

static uint8_t            g_oled_ready = 0;
static uint8_t            g_matrix_ready = 0;
static uint8_t            g_game_reset_pending = 0;
static game_t             g_reset_target = GAME_RLGL;
static uint8_t            g_menu_display_active = 0;
static char               g_menu_line1[32] = {0};
static char               g_menu_line2[32] = {0};
static uint64_t           g_digit_bitmaps[10] = {0};
static uint8_t            g_digit_initialized = 0U;
static uint8_t            g_arrow_sequence[ARROW_MAX_SEQUENCE] = {0};
static uint8_t            g_arrow_length_active = 0U;
static uint8_t            g_arrow_index = 0U;
static uint8_t            g_arrow_game_running = 0U;
static uint32_t           g_arrow_start_tick = 0U;
static uint32_t           g_arrow_last_matrix_update = 0U;
static uint8_t            g_arrow_last_remaining = 0xFFU;
static uint32_t           g_arrow_time_limit_ms = 0U;

#define ARROW_MAX_VISIBLE   4U
#define ARROW_SYMBOL_SPACING  2
#define ARROW_SYMBOL_Y        4

static const char ARROW_SYMBOLS[4] = { '^', 'v', '<', '>' };

typedef struct {
    uint8_t  active;
    uint32_t expiry_tick;
    char line1[32];
    char line2[32];
} oled_message_t;

static oled_message_t g_oled_temp_message = {0U, 0U, "", ""};
static char g_oled_status_line1[32] = "";
static char g_oled_status_line2[32] = "";

static void OLED_RenderLines(const char *line1, const char *line2);
static void OLED_SetTemporaryMessage(const char *line1, const char *line2, uint32_t duration_ms);
static void OLED_ResetStatus(void);
static void OLED_UpdateGameplayDisplay(uint32_t now);

/* ========= LED alert blinker (Catch mode) ========= */
typedef struct {
    uint8_t enabled;
    uint32_t period_ms;
    uint32_t last_toggle_ms;
} led_blink_t;

static led_blink_t alert_blink = {0, 0, 0};

static void led_set_blink(int level) /* -1 off, 0 slow, 1 med, 2 fast */
{
    if (level < 0) {
        g_catch_blink_level = -1;
        alert_blink.enabled = 0;
        BSP_LED_Off(LED2);
        return;
    }
    if (alert_blink.enabled && (level == g_catch_blink_level)) {
        return;
    }
    g_catch_blink_level = level;
    alert_blink.enabled = 1;
    alert_blink.last_toggle_ms = HAL_GetTick();
    switch (level) {
        case 2: alert_blink.period_ms = 160U;  break; /* fastest blink */
        case 1: alert_blink.period_ms = 400U;  break; /* medium blink */
        default:alert_blink.period_ms = 800U;  break; /* slowest blink */
    }
}
static void led_blink_process(uint32_t now)
{
    if (!alert_blink.enabled) return;
    if ((now - alert_blink.last_toggle_ms) >= (alert_blink.period_ms >> 1)) {
        alert_blink.last_toggle_ms = now;
        BSP_LED_Toggle(LED2);
    }
}

/* ========= Double-press (no debouncing) ========= */
#define CLICK_WINDOW_MS 600U
static volatile uint8_t  click_window_active = 0;
static volatile uint8_t  click_count = 0;
static volatile uint32_t click_window_start = 0;
static volatile uint8_t  single_press_event = 0;  /* used in Catch window */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != BUTTON_EXTI13_Pin) return;
    uint32_t now = HAL_GetTick();
    if (!click_window_active) { click_window_active = 1; click_window_start = now; click_count = 1; }
    else { click_count++; }
}

static void process_clicks(uint32_t now)
{
    if (!click_window_active) return;
    if ((now - click_window_start) >= CLICK_WINDOW_MS) {
        uint8_t n = click_count;
        click_window_active = 0; click_count = 0;
		

        if (g_game_reset_pending) {
            single_press_event = 1;
            return;
        }

        if ((n == 1U) && g_switch_ready && (Menu_GetState(&g_menu) != MENU_CLOSED)) {
            printu("Blue button: closing menu\r\n");
            Menu_Close(&g_menu);
            return;
        }

     	 if (n == 2) {
            if (g_game == GAME_RLGL) {
                g_game = GAME_CATCH;
                OLED_ResetStatus();
                g_game_reset_pending = 0U;
                if (g_switch_ready) { Menu_SetGame(&g_menu, (uint8_t)g_game); }
                printu("Entering Catch And Run as %s\r\n",
                       (g_role == ROLE_PLAYER) ? "Player" : "Enforcer");
                led_set_blink(-1);
                int16_t m[3]; BSP_MAGNETO_GetXYZ(m);
                g_mag_baseline = abs(m[0]) + abs(m[1]) + abs(m[2]);
                g_catch_state = CATCH_IDLE;
                g_catch_event_start = 0;
                t_envCatch = now;
                single_press_event = 0;
                was_temp_high=was_hum_high=was_press_high=0;
            } else if (g_game == GAME_CATCH){
                g_game = GAME_RLGL;
                OLED_ResetStatus();
                g_game_reset_pending = 0U;
                if (g_switch_ready) { Menu_SetGame(&g_menu, (uint8_t)g_game); }
                printu("Entering Red Light, Green Light as %s\r\n",
                       (g_role == ROLE_PLAYER) ? "Player" : "Enforcer");
                g_phase = PHASE_GREEN; g_gameOver = 0;
                t_phaseSwitch = now;
                t_envRLGL = t_motionRLGL = t_ledHB = 0;
                led_set_blink(-1);
                g_catch_state = CATCH_IDLE;
                single_press_event = 0;
                BSP_LED_On(LED2);
                printu("Green Light!\r\n");
            }
		
        }
        else if (n == 3U) {
            if (g_role == ROLE_PLAYER) {
                g_role = ROLE_ENFORCER;
                printu("Role switched to Enforcer\r\n");
            } else {
                g_role = ROLE_PLAYER;
                printu("Role switched to Player\r\n");
            }
            return;
        }
        else if ((n == 1) && (g_game == GAME_RLGL) && !g_game_reset_pending && !g_gameOver) {
            /* single press in Catch mode */
            if (g_role == ROLE_PLAYER) {
					g_role = ROLE_ENFORCER;
					printu("Role switched to Enforcer\r\n");
				} else {
					g_role = ROLE_PLAYER;
					printu("Role switched to Player\r\n");
				}

        }
            /* single press */
        
		
        
		else {
            single_press_event = 1;
        }
    }
}

static void GameOver_Trigger(game_t game, const char *message)
{
    if (g_game_reset_pending && (g_reset_target == game)) {
        return;
    }

    if (message != NULL && message[0] != '\0') {
        printu("%s\r\n", message);
    }

    switch (game) {
        case GAME_RLGL:
            printu("Press PB once to restart Red Light, Green Light.\r\n");
            break;
        case GAME_CATCH:
            printu("Press PB once to restart Catch & Run.\r\n");
            break;
        case GAME_ARROW:
            printu("Press PB once to restart Audition: Sotong Edition.\r\n");
            break;
        default:
            break;
    }
    Buzzer_PlayFailureTune();

    if (g_oled_ready) {
        SSD1306_Stopscroll();
        SSD1306_Clear();
        SSD1306_DrawBitmap(0, 0, gameoveranimation, 128, 64, 1);
        SSD1306_UpdateScreen();
    }

    if (g_matrix_ready) {
        HT16K33_PlayFrames(&hmatrix,
                           HT16K33_IMAGES_GAMEOVER,
                           HT16K33_IMAGES_GAMEOVER_LEN,
                           200U,
                           1U);
    }

    

    g_game_reset_pending = 1U;
    g_reset_target = game;
    single_press_event = 0;
    click_window_active = 0;
    click_count = 0;

    switch (game) {
        case GAME_RLGL:
            g_gameOver = 1;
            BSP_LED_Off(LED2);
            break;
        case GAME_CATCH:
            g_catch_state = CATCH_WAIT_RESET;
            g_catch_event_start = 0;
            led_set_blink(-1);
            break;
        case GAME_ARROW:
            Arrow_Stop();
            break;
        default:
            break;
    }
}

static uint8_t GameReset_Attempt(game_t game, uint32_t now, int catch_level)
{
    if (!g_game_reset_pending || (g_reset_target != game) || !single_press_event) {
        return 0U;
    }

    single_press_event = 0;
    g_game_reset_pending = 0;

    switch (game) {
        case GAME_RLGL:
            g_phase = PHASE_GREEN;
            g_gameOver = 0;
            t_phaseSwitch = now;
            t_envRLGL = 0;
            t_motionRLGL = 0;
            t_ledHB = 0;
            BSP_LED_On(LED2);
            printu("RLGL reset. Green Light!\r\n");
            break;
        case GAME_CATCH:
            if (catch_level >= 0) {
                g_catch_state = CATCH_LOCKOUT;
            } else {
                g_catch_state = CATCH_IDLE;
            }
            g_catch_event_start = now;
            led_set_blink(-1);
            printu("Catch & Run reset. Stay alert!\r\n");
            break;
        case GAME_ARROW:
            Arrow_StartGame();
            break;
        default:
            break;
    }

    return 1U;
}

/* ========= Gyroscope Calibration ========= */
void CalibrateGyroscope(void)
{
    const int NUM_SAMPLES = 2000;
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;

    printu("\r\n========================================\r\n");
    printu("     GYROSCOPE CALIBRATION\r\n");
    printu("========================================\r\n\r\n");
    printu("IMPORTANT: Keep board COMPLETELY STILL!\r\n");
    printu("Starting calibration in 1 seconds...\r\n\r\n");
    HAL_Delay(1000);

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float gyro_raw[3];
        BSP_GYRO_GetXYZ(gyro_raw);
        sum_x += gyro_raw[0];
        sum_y += gyro_raw[1];
        sum_z += gyro_raw[2];
        HAL_Delay(2);
    }

    gyro_offset_x = sum_x / NUM_SAMPLES;
    gyro_offset_y = sum_y / NUM_SAMPLES;
    gyro_offset_z = sum_z / NUM_SAMPLES;

    printu("Calibration Complete!\r\nOffsets: X=%.2f Y=%.2f Z=%.2f\r\n",
           gyro_offset_x, gyro_offset_y, gyro_offset_z);
}

/* ========= Init ========= */
void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.Timing          = 0x00702681; /* ~100 kHz with 80 MHz sysclk */
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.OwnAddress2Masks= I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) { Error_Handler(); }
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) { Error_Handler(); }
}

static void I2C_Scan(I2C_HandleTypeDef *hi2c)
{
    printu("\r\nStarting I2C scan...\r\n");
    for (uint8_t addr = 1; addr < 128; ++addr) {
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 3, 5) == HAL_OK) {
            printu("I2C device at 0x%02X\r\n", addr);
        }
        Buzzer_Service(HAL_GetTick());
    }
    printu("Scan complete.\r\n");
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin  = BUTTON_EXTI13_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void UART1_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7; /* PB6=TX, PB7=RX */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart1.Instance            = USART1;
    huart1.Init.BaudRate       = 115200;
    huart1.Init.WordLength     = UART_WORDLENGTH_8B;
    huart1.Init.StopBits       = UART_STOPBITS_1;
    huart1.Init.Parity         = UART_PARITY_NONE;
    huart1.Init.Mode           = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling   = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) { while (1) {} }
}

void NFC_Init(void) {
    if (BSP_NFCTAG_Init(0) == NFCTAG_OK)
        printu("NFC Tag initialized successfully.\r\n");
    else
        printu("NFC Tag initialization failed.\r\n");
}

void NFC_BlinkIfCard(void) {
    ST25DV_FIELD_STATUS rf_field;

    if (BSP_NFCTAG_GetRFField_Dyn(0, &rf_field) == NFCTAG_OK) {
        if (rf_field == ST25DV_FIELD_ON) {
            BSP_LED_On(LED2);
            printu(".");
        } else {
            BSP_LED_Off(LED2);
        }
    }
}



/* ========= MAIN ========= */
int main(void)
{
    HAL_Init();
    MX_GPIO_Init();
    UART1_Init();
    Buzzer_Init();

    MX_I2C1_Init();
    NFC_Init();
    BSP_LED_Init(LED2);
    BSP_ACCELERO_Init();
    BSP_GYRO_Init();
    BSP_MAGNETO_Init();
    BSP_TSENSOR_Init();
    BSP_HSENSOR_Init();
    BSP_PSENSOR_Init();

    /* Gyro calibration before starting game */
    CalibrateGyroscope();
    srand((unsigned int)HAL_GetTick());

    //SSD1306_Init();
    //Default: RLGL as Player
    g_role = ROLE_PLAYER;
    g_game = GAME_RLGL;
    g_phase = PHASE_GREEN;
    g_gameOver = 0;

    printu("Entering Red Light, Green Light as %s\r\n",
           (g_role == ROLE_PLAYER) ? "Player" : "Enforcer");
    BSP_LED_On(LED2);
    printu("Green Light!\r\n");

    uint32_t now = HAL_GetTick();
    t_phaseSwitch = now;
    t_envRLGL = t_motionRLGL = t_ledHB = 0;

    int16_t m0[3]; BSP_MAGNETO_GetXYZ(m0);
    g_mag_baseline = abs(m0[0]) + abs(m0[1]) + abs(m0[2]);


    I2C_Scan(&hi2c1);

    if (Grove5Way_Init(&g_switch, &hi2c1, GROVE5WAY_DEFAULT_ADDR)) {
        g_switch_ready = 1U;
        Menu_Init(&g_menu, &g_switch, (uint8_t)g_game);
        Menu_SetGame(&g_menu, (uint8_t)g_game);
        t_menuPoll = HAL_GetTick();
        printu("Grove 5-way switch ready at 0x%02X\r\n", GROVE5WAY_DEFAULT_ADDR);
    } else {
        g_switch_ready = 0U;
        printu("Grove 5-way switch not found at 0x%02X\r\n", GROVE5WAY_DEFAULT_ADDR);
    }

    if (HT16K33_Init(&hmatrix, &hi2c1, HT16K33_I2C_ADDR_DEFAULT) == HAL_OK) {
        g_matrix_ready = 1U;
        printu("HT16K33 matrix ready\r\n");
        HT16K33_SetBrightness(&hmatrix, 8U);
        HT16K33_SetBlinkRate(&hmatrix, 0U);
        /* Show intro animation once before main loop */
        HT16K33_DisplayBitmap64(&hmatrix, HT16K33_IMAGES_INTRO[0]);
        HT16K33_PlayFrames(&hmatrix,
                           HT16K33_IMAGES_INTRO,
                           HT16K33_IMAGES_INTRO_LEN,
                           120U,
                           1U);
        HT16K33_Clear(&hmatrix);
        HT16K33_Update(&hmatrix);
    } else {
        g_matrix_ready = 0U;
        printu("HT16K33 not detected\r\n");
    }


        uint32_t tickstart2 = HAL_GetTick();
        const uint32_t wait2 = 100U;

        while ((HAL_GetTick() - tickstart2) < wait2){
            Buzzer_Service(HAL_GetTick());
        }
    if (SSD1306_Init()) {
        g_oled_ready = 1U;
        printu("SSD1306 initialized on I2C1\r\n");
        SSD1306_Fill(SSD1306_COLOR_BLACK);
        SSD1306_UpdateScreen();
        SSD1306_GotoXY(0, 0);
        SSD1306_Puts("OLED OK", &Font_16x26, SSD1306_COLOR_WHITE);
        SSD1306_UpdateScreen();
        printu("SSD1306 sanity draw done\r\n");
    } else {
        g_oled_ready = 0U;
        printu("SSD1306 not found on I2C1\r\n");
    }

    printu("Scheduling buzzer A4->A5 startup sweep...\r\n");
    Buzzer_TestPattern();

    while (1)
    {//
        uint32_t tickstart = HAL_GetTick();
        const uint32_t wait = 1000U;

        while ((HAL_GetTick() - tickstart) < wait)
        {
            now = HAL_GetTick();

            process_clicks(now);
            led_blink_process(now);
            NFC_PrintDetected();
            Buzzer_Service(now);

            bool menu_active = false;
            if (g_switch_ready) {
                if ((now - t_menuPoll) >= 50U) {
                    t_menuPoll = now;
                    if ((Menu_GetState(&g_menu) != MENU_CLOSED) || (g_game != GAME_ARROW)) {
                        Menu_Process(&g_menu);
                    } else {
                        Grove5Way_Event evt;
                        if (Grove5Way_Poll(&g_switch, &evt)) {
                            uint8_t edges = (uint8_t)(evt.pressed & evt.changed);
                            if (edges != 0U) {
                                if ((edges & GROVE5WAY_BTN_CENTER) != 0U) {
                                    Menu_Open(&g_menu);
                                    continue;
                                }
                                Arrow_HandleInput(edges);
                            }
                        }
                    }
                }
                menu_active = (Menu_GetState(&g_menu) != MENU_CLOSED);
            }

            if (menu_active) {
                g_menu_display_active = 1U;
                continue;
            } else if (g_menu_display_active) {
                if (g_oled_ready) {
                    if ((g_game == GAME_ARROW) && g_arrow_game_running && !g_game_reset_pending) {
                        Arrow_RenderSequence();
                    } else {
                        SSD1306_Fill(SSD1306_COLOR_BLACK);
                        SSD1306_UpdateScreen();
                    }
                }
                g_menu_display_active = 0U;
            }

            /* ---- Game 1: RLGL ---- */
            if (g_game == GAME_RLGL) {
                GameReset_Attempt(GAME_RLGL, now, -1);
                if (!g_gameOver) {
                    if ((now - t_phaseSwitch) >= 10000U) {
                        t_phaseSwitch = now;
                        if (g_phase == PHASE_GREEN) {
                            g_phase = PHASE_RED;
                            printu("Red Light!\r\n");
                            t_motionRLGL = 0; t_ledHB = 0;
                        } else {
                            g_phase = PHASE_GREEN;
                            printu("Green Light!\r\n");
                            t_envRLGL = 0; g_gameOver = 0; BSP_LED_On(LED2);
                        }
                    }

                    if (g_phase == PHASE_GREEN) {
                        BSP_LED_On(LED2);
                        if ((now - t_envRLGL) >= 2000U) {
                            t_envRLGL = now;
                            float t = BSP_TSENSOR_ReadTemp();
                            float p = BSP_PSENSOR_ReadPressure();
                            float h = BSP_HSENSOR_ReadHumidity();
                            printu("Temp=%.2fC Pressure=%.2fhPa Humidity=%.2f%%\r\n", t, p, h);
                        }
                    } else {
                        if ((now - t_ledHB) >= 500U) { t_ledHB = now; BSP_LED_Toggle(LED2); }
                        if ((now - t_motionRLGL) >= 2000U) {
                            t_motionRLGL = now;

                            int16_t ar[3] = {0}; BSP_ACCELERO_AccGetXYZ(ar);
                            float ax = ar[0] * (9.8f / 1000.0f);
                            float ay = ar[1] * (9.8f / 1000.0f);
                            float az = ar[2] * (9.8f / 1000.0f);
                            float a_mag = sqrtf(ax*ax + ay*ay + az*az);

                            float g[3] = {0.f, 0.f, 0.f};
                            BSP_GYRO_GetXYZ(g);
                            float gx = g[0] - gyro_offset_x;
                            float gy = g[1] - gyro_offset_y;
                            float gz = g[2] - gyro_offset_z;
                            float g_mag = sqrtf(gx*gx + gy*gy + gz*gz);

                            printu("Acceleration[%.2f,%.2f,%.2f] GyroscopeRaw[%.2f,%.2f,%.2f] GyroscopeAdj[%.2f,%.2f,%.2f]\r\n",
                                   ax, ay, az, g[0], g[1], g[2], gx, gy, gz);

                            if ((a_mag > ACCEL_THRESHOLD_MS2) || (g_mag > GYRO_THRESHOLD_DPS)) {
                                if (g_role == ROLE_PLAYER) {
                                    GameOver_Trigger(GAME_RLGL, "Game Over!");
                                } else {
                                    printu("Player Out!\r\n");
                                }
                            }
                        }
                    }
                } else {
                    BSP_LED_Off(LED2);
                }
                OLED_UpdateGameplayDisplay(now);
            }
            /* ---- Game 2: Catch & Run ---- */
            else if (g_game == GAME_CATCH) {
                int16_t mag_raw[3]; BSP_MAGNETO_GetXYZ(mag_raw);
                int sum = abs(mag_raw[0]) + abs(mag_raw[1]) + abs(mag_raw[2]);
                int diff = sum - g_mag_baseline; if (diff < 0) diff = -diff;

                int level = -1;
                if (diff > MAG_THRESH[2])      level = 2;
                else if (diff > MAG_THRESH[1]) level = 1;
                else if (diff > MAG_THRESH[0]) level = 0;

                GameReset_Attempt(GAME_CATCH, now, level);

                switch (g_catch_state) {
                    case CATCH_IDLE:
                        if (level >= 0) {
                            g_catch_state = CATCH_ALERT;
                            g_catch_event_start = now;
                            single_press_event = 0;
                            if (g_role == ROLE_PLAYER) {
                                printu("Enforcer nearby! Be careful.\r\n");
                            } else {
                                printu("Player is Nearby! Move faster.\r\n");
                            }
                            led_set_blink(level);
                        } else {
                            led_set_blink(-1);
                        }
                        break;

                    case CATCH_ALERT:
                        if (level >= 0) {
                            led_set_blink(level);
                        }
                        if (single_press_event) {
                            single_press_event = 0;
                            g_catch_event_start = 0;
                            led_set_blink(-1);
                            if (g_role == ROLE_PLAYER) {
                                printu("Player escaped, good job!\r\n");
                                OLED_SetTemporaryMessage("Player escaped", "Good job!", 1000U);
                            } else {
                                printu("Player captured, good job!\r\n");
                                OLED_SetTemporaryMessage("Player captured", "Good job!", 1000U);
                            }
                            g_catch_state = CATCH_LOCKOUT;
                        } else if ((now - g_catch_event_start) >= 3000U) {
                            single_press_event = 0;
                            if (g_role == ROLE_PLAYER) {
                                GameOver_Trigger(GAME_CATCH, "Game Over!");
                            } else {
                                printu("Player escaped! Keep trying.\r\n");
                                OLED_SetTemporaryMessage("Player escaped!", "Keep trying!", 1000U);
                                g_catch_state = CATCH_LOCKOUT;
                            }
                        }
                        break;

                    case CATCH_LOCKOUT:
                        if (level < 0) {
                            g_catch_state = CATCH_IDLE;
                            led_set_blink(-1);
                        }
                        break;

                    case CATCH_WAIT_RESET:
                        led_set_blink(-1);
                        break;

                    default:
                        g_catch_state = CATCH_IDLE;
                        led_set_blink(-1);
                        break;
                }

                if ((now - t_envCatch) >= 1000U) {
                    t_envCatch = now;
                    float t = BSP_TSENSOR_ReadTemp();
                    float h = BSP_HSENSOR_ReadHumidity();
                    float p = BSP_PSENSOR_ReadPressure();

                    uint8_t th = (t > TEMP_THRESH_C);
                    uint8_t hh = (h > HUMID_THRESH_PCT);
                    uint8_t ph = (p > PRESS_THRESH_HPA);

                    if (th) {
                        printu("Temperature spike detected! T:%.2fC. Dangerous environment!\r\n", t);
                    } else if (was_temp_high) {
                        printu("Temperature back to normal: %.2fC\r\n", t);
                    }

                    if (hh) {
                        printu("Humidity spike detected! H:%.2f%%.\r\n", h);
                    } else if (was_hum_high) {
                        printu("Humidity back to normal: %.2f%%\r\n", h);
                    }

                    if (ph) {
                        printu("Pressure spike detected! P:%.2fhPa.\r\n", p);
                    } else if (was_press_high) {
                        printu("Pressure back to normal: %.2fhPa\r\n", p);
                    }

                    was_temp_high = th; was_hum_high = hh; was_press_high = ph;
                }
                OLED_UpdateGameplayDisplay(now);
            }
            else if(g_game == GAME_ARROW){
                GameReset_Attempt(GAME_ARROW, now, -1);
                if (!g_game_reset_pending && !g_arrow_game_running) {
                    Arrow_StartGame();
                }
                if (!g_game_reset_pending) {
                    Arrow_UpdateMatrix(now);
                }
            }
        }
    }
}

static void Arrow_InitDigits(void)
{
    if (g_digit_initialized) {
        return;
    }
    static const uint64_t digit_src[10] = {
        0x3C6666766E663C00ULL, /* 0 */
        0x7E18181838181800ULL, /* 1 */
        0x7E60300C06663C00ULL, /* 2 */
        0x3C66061C06663C00ULL, /* 3 */
        0x0C0C7E4C2C1C0C00ULL, /* 4 */
        0x3C6606067C607E00ULL, /* 5 */
        0x3C66667C60663C00ULL, /* 6 */
        0x1818180C0C667E00ULL, /* 7 */
        0x3C66663C66663C00ULL, /* 8 */
        0x3C66063E66663C00ULL  /* 9 */
    };

    for (uint8_t i = 0U; i < 10U; ++i) {
        g_digit_bitmaps[i] = digit_src[i];
    }
    g_digit_initialized = 1U;
}

static void Arrow_ShowDigit(uint8_t digit)
{
    if (!g_matrix_ready) {
        return;
    }
    if (digit > 9U) {
        digit = 9U;
    }
    Arrow_InitDigits();
    uint64_t bitmap = g_digit_bitmaps[digit];
    for (uint8_t row = 0U; row < 8U; ++row) {
        uint8_t pattern = (uint8_t)((bitmap >> (row * 8U)) & 0xFFU);
        HT16K33_SetRow(&hmatrix, row, pattern);
    }
    HT16K33_Update(&hmatrix);
}

static void Arrow_Stop(void)
{
    g_arrow_game_running = 0U;
    g_arrow_length_active = 0U;
    g_arrow_index = 0U;
    g_arrow_last_remaining = 0xFFU;
    g_arrow_last_matrix_update = 0U;

    if (g_matrix_ready) {
        HT16K33_Clear(&hmatrix);
        HT16K33_Update(&hmatrix);
    }
}

static void Arrow_RenderSequence(void)
{
    if (!g_oled_ready) {
        return;
    }

    SSD1306_Stopscroll();
    SSD1306_Fill(SSD1306_COLOR_BLACK);
    FontDef_t *arrow_font = &Font_16x26;
    uint8_t symbol_width = arrow_font->FontWidth;
    const uint8_t max_visible = ARROW_MAX_VISIBLE;

    if (g_arrow_index < g_arrow_length_active) {
        uint8_t remaining = (uint8_t)(g_arrow_length_active - g_arrow_index);
        uint8_t visible = remaining;
        if (visible > max_visible) {
            visible = max_visible;
        }
        int16_t total_width = (int16_t)(visible * symbol_width +
                                         (visible > 0U ? (visible - 1U) * ARROW_SYMBOL_SPACING : 0U));
        int16_t base_x = (int16_t)((128 - total_width) / 2);
        if (base_x < 0) {
            base_x = 0;
        }
        for (uint8_t i = 0U; i < visible; ++i) {
            uint8_t dir = g_arrow_sequence[g_arrow_index + i];
            if (dir > 3U) {
                dir = 0U;
            }
            char glyph[2] = { ARROW_SYMBOLS[dir], '\0' };
            int16_t x = (int16_t)(base_x + i * (symbol_width + ARROW_SYMBOL_SPACING));
            SSD1306_GotoXY(x, ARROW_SYMBOL_Y);
            SSD1306_Puts(glyph, arrow_font, SSD1306_COLOR_WHITE);
        }
        char info[20];
        snprintf(info, sizeof(info), "Left %02u",
                 (unsigned)(g_arrow_length_active - g_arrow_index));
        size_t info_len = strlen(info);
        int16_t text_width = (int16_t)(info_len * (size_t)Font_16x26.FontWidth);
        int16_t text_x = (int16_t)((128 - text_width) / 2);
        if (text_x < 0) {
            text_x = 0;
        }
        SSD1306_GotoXY(text_x, 34);
        SSD1306_Puts(info, &Font_16x26, SSD1306_COLOR_WHITE);
    } else {
        char msg[] = "Ready!";
        size_t msg_len = strlen(msg);
        int16_t text_width = (int16_t)(msg_len * (size_t)Font_16x26.FontWidth);
        int16_t text_x = (int16_t)((128 - text_width) / 2);
        if (text_x < 0) {
            text_x = 0;
        }
        SSD1306_GotoXY(text_x, 20);
        SSD1306_Puts(msg, &Font_16x26, SSD1306_COLOR_WHITE);
    }

    SSD1306_UpdateScreen();
}

static void Arrow_StartGame(void)
{
    if (g_game != GAME_ARROW) {
        return;
    }

    OLED_ResetStatus();
    Arrow_InitDigits();

    uint8_t length = g_arrow_length_setting;
    if (length < 1U) length = 1U;
    if (length > ARROW_MAX_SEQUENCE) length = ARROW_MAX_SEQUENCE;
    g_arrow_length_active = length;

    for (uint8_t i = 0U; i < g_arrow_length_active; ++i) {
        g_arrow_sequence[i] = (uint8_t)(rand() % 4);
    }

    g_arrow_index = 0U;
    g_arrow_time_limit_ms = g_arrow_time_setting_ms;
    if (g_arrow_time_limit_ms < 1000U) {
        g_arrow_time_limit_ms = 1000U;
    }

    g_arrow_start_tick = HAL_GetTick();
    g_arrow_last_matrix_update = 0U;
    g_arrow_last_remaining = 0xFFU;
    g_arrow_game_running = 1U;

    printu("Audition start: %u arrows, %lus timer.\r\n",
           (unsigned)g_arrow_length_active,
           (unsigned long)(g_arrow_time_limit_ms / 1000U));

    Arrow_RenderSequence();
    Arrow_UpdateMatrix(g_arrow_start_tick);
}

static void Arrow_UpdateMatrix(uint32_t now)
{
    if (!g_arrow_game_running || g_game_reset_pending) {
        return;
    }
    Arrow_InitDigits();
    if (!g_matrix_ready) {
        return;
    }

    if (now < g_arrow_start_tick) {
        g_arrow_start_tick = now;
    }

    uint32_t elapsed = now - g_arrow_start_tick;
    if (elapsed >= g_arrow_time_limit_ms) {
        Arrow_Fail("Time's up!");
        return;
    }

    uint32_t remaining_ms = g_arrow_time_limit_ms - elapsed;
    uint8_t remaining_s = (uint8_t)(remaining_ms / 1000U);
    if (remaining_s > 9U) {
        remaining_s = 9U;
    }

    if ((remaining_s != g_arrow_last_remaining) ||
        ((now - g_arrow_last_matrix_update) >= 200U)) {
        Arrow_ShowDigit(remaining_s);
        g_arrow_last_remaining = remaining_s;
        g_arrow_last_matrix_update = now;
    }
}

static void Arrow_Fail(const char *message)
{
    if (!g_arrow_game_running) {
        return;
    }
    Arrow_Stop();
    GameOver_Trigger(GAME_ARROW, message);
}

static void Arrow_Success(void)
{
    if (!g_arrow_game_running) {
        return;
    }

    uint8_t total = g_arrow_length_active;
    Arrow_Stop();

    if (g_oled_ready) {
        SSD1306_Fill(SSD1306_COLOR_BLACK);
        SSD1306_GotoXY(0, 8);
        SSD1306_Puts("AUDITION", &Font_11x18, SSD1306_COLOR_WHITE);
        SSD1306_GotoXY(0, 32);
        SSD1306_Puts("CLEAR!", &Font_16x26, SSD1306_COLOR_WHITE);
        SSD1306_UpdateScreen();
    }

    Buzzer_PlaySuccessTune();
    printu("Audition cleared! Sequence length %u.\r\n", (unsigned)total);

    g_game_reset_pending = 1U;
    g_reset_target = GAME_ARROW;
    single_press_event = 0;
    click_window_active = 0;
    click_count = 0;
}

static void Arrow_HandleInput(uint8_t edges)
{
    if (!g_arrow_game_running || g_game_reset_pending) {
        return;
    }
    if (g_arrow_index >= g_arrow_length_active) {
        return;
    }

    if (edges & GROVE5WAY_BTN_CENTER) {
        Menu_Open(&g_menu);
        return;
    }

    const uint8_t directional_mask =
        GROVE5WAY_BTN_UP | GROVE5WAY_BTN_DOWN |
        GROVE5WAY_BTN_LEFT | GROVE5WAY_BTN_RIGHT;

    uint8_t expected_mask;
    switch (g_arrow_sequence[g_arrow_index]) {
        case 0: expected_mask = GROVE5WAY_BTN_UP; break;
        case 1: expected_mask = GROVE5WAY_BTN_DOWN; break;
        case 2: expected_mask = GROVE5WAY_BTN_LEFT; break;
        default: expected_mask = GROVE5WAY_BTN_RIGHT; break;
    }

    if ((edges & expected_mask) != 0U) {
        if (edges & (directional_mask & (uint8_t)~expected_mask)) {
            Arrow_Fail("Wrong direction!");
            return;
        }

        g_arrow_index++;
        if (g_arrow_index >= g_arrow_length_active) {
            Arrow_Success();
        } else {
            Arrow_RenderSequence();
        }
    } else if (edges & directional_mask) {
        Arrow_Fail("Wrong direction!");
    }
}

static void NFC_PrintDetected(void)
{
    static uint8_t was_on = 0;
    ST25DV_FIELD_STATUS rf_field;
    if (BSP_NFCTAG_GetRFField_Dyn(0, &rf_field) != NFCTAG_OK) {
        return;
    }
    if (rf_field == ST25DV_FIELD_ON) {
        if (!was_on) {
            if (g_game != GAME_ARROW){
                printu("NFC detected! Switching to Audition:Sotong Edition!\r\n");
                uint32_t tickstart4 = HAL_GetTick();
                const uint32_t wait4 = 1000U;

                while ((HAL_GetTick() - tickstart4) < wait4) {
                    Buzzer_Service(HAL_GetTick());
                }
                led_set_blink(-1);
                g_catch_state = CATCH_IDLE;
                single_press_event = 0;
                g_catch_event_start = 0;
                g_game = GAME_ARROW;
                OLED_ResetStatus();
                if (g_switch_ready) { Menu_SetGame(&g_menu, (uint8_t)g_game); }
                g_game_reset_pending = 0U;
                Arrow_StartGame();
            }
            else
            {
                printu("NFC detected! Switching to RLGL!\r\n");
                uint32_t tickstart4 = HAL_GetTick();
                const uint32_t wait4 = 1000U;

                while ((HAL_GetTick() - tickstart4) < wait4) {
                    Buzzer_Service(HAL_GetTick());
                }
                led_set_blink(-1);
                g_catch_state = CATCH_IDLE;
                single_press_event = 0;
                g_catch_event_start = 0;
                Arrow_Stop();
                g_game = GAME_RLGL;
                OLED_ResetStatus();
                if (g_switch_ready) { Menu_SetGame(&g_menu, (uint8_t)g_game); }
                g_game_reset_pending = 0U;
                g_phase = PHASE_GREEN;
                g_gameOver = 0;
                t_phaseSwitch = HAL_GetTick();
                t_envRLGL = 0;
                t_motionRLGL = 0;
                t_ledHB = 0;
                BSP_LED_On(LED2);
                printu("Green Light!\r\n");
            }
        }
        was_on = 1;
    } else {
        was_on = 0;
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
void Menu_RenderStatus(const char *line1, const char *line2)
{
    if (!g_oled_ready) return;
    if (line1 == NULL) line1 = "";
    if (line2 == NULL) line2 = "";
    strncpy(g_menu_line1, line1, sizeof(g_menu_line1) - 1);
    g_menu_line1[sizeof(g_menu_line1) - 1] = '\0';
    strncpy(g_menu_line2, line2, sizeof(g_menu_line2) - 1);
    g_menu_line2[sizeof(g_menu_line2) - 1] = '\0';

    SSD1306_Stopscroll();
    SSD1306_Fill(SSD1306_COLOR_BLACK);

    FontDef_t *large_font = &Font_11x18;
    FontDef_t *small_font = &Font_7x10;

    char *texts[2] = { g_menu_line1, g_menu_line2 };
    const uint8_t y_positions[2] = { 6U, 32U };

    for (uint8_t i = 0U; i < 2U; ++i) {
        char *text = texts[i];
        if (text == NULL) {
            continue;
        }
        size_t len = strlen(text);
        if (len == 0U) {
            continue;
        }
        FontDef_t *font = large_font;
        if ((len * (size_t)large_font->FontWidth) > 128U) {
            font = small_font;
        }
        size_t width = len * (size_t)font->FontWidth;
        if (width > 128U) {
            width = 128U;
        }
        int16_t x = (int16_t)((128U - width) / 2U);
        if (x < 0) {
            x = 0;
        }
        SSD1306_GotoXY(x, y_positions[i]);
    SSD1306_Puts(text, font, SSD1306_COLOR_WHITE);
    }
    SSD1306_UpdateScreen();
}

static void OLED_ResetStatus(void)
{
    g_oled_status_line1[0] = '\0';
    g_oled_status_line2[0] = '\0';
    g_oled_temp_message.active = 0U;
    g_oled_temp_message.expiry_tick = 0U;
    g_oled_temp_message.line1[0] = '\0';
    g_oled_temp_message.line2[0] = '\0';
}

static void OLED_RenderLines(const char *line1, const char *line2)
{
    if (!g_oled_ready) {
        return;
    }
    if (g_game_reset_pending) {
        return;
    }
    if (g_game == GAME_ARROW) {
        return;
    }
    if (Menu_GetState(&g_menu) != MENU_CLOSED) {
        return;
    }

    const char *l1 = (line1 != NULL) ? line1 : "";
    const char *l2 = (line2 != NULL) ? line2 : "";

    if ((strcmp(l1, g_oled_status_line1) == 0) &&
        (strcmp(l2, g_oled_status_line2) == 0)) {
        return;
    }

    SSD1306_Stopscroll();
    SSD1306_Fill(SSD1306_COLOR_BLACK);

    const char *lines[2] = { l1, l2 };
    const uint8_t y_positions[2] = { 6U, 32U };

    for (uint8_t i = 0U; i < 2U; ++i) {
        char buffer[32];
        strncpy(buffer, lines[i], sizeof(buffer) - 1U);
        buffer[sizeof(buffer) - 1U] = '\0';
        size_t len = strlen(buffer);
        if (len == 0U) {
            continue;
        }
        FontDef_t *font = &Font_11x18;
        if ((len * (size_t)font->FontWidth) > 128U) {
            font = &Font_7x10;
        }
        size_t width = len * (size_t)font->FontWidth;
        if (width > 128U) {
            width = 128U;
        }
        int16_t x = (int16_t)((128U - width) / 2U);
        if (x < 0) {
            x = 0;
        }
        SSD1306_GotoXY(x, y_positions[i]);
        SSD1306_Puts(buffer, font, SSD1306_COLOR_WHITE);
    }
    SSD1306_UpdateScreen();

    strncpy(g_oled_status_line1, l1, sizeof(g_oled_status_line1) - 1U);
    g_oled_status_line1[sizeof(g_oled_status_line1) - 1U] = '\0';
    strncpy(g_oled_status_line2, l2, sizeof(g_oled_status_line2) - 1U);
    g_oled_status_line2[sizeof(g_oled_status_line2) - 1U] = '\0';
}

static void OLED_SetTemporaryMessage(const char *line1, const char *line2, uint32_t duration_ms)
{
    if (!g_oled_ready) {
        return;
    }
    uint32_t now = HAL_GetTick();
    g_oled_temp_message.active = 1U;
    g_oled_temp_message.expiry_tick = now + duration_ms;
    strncpy(g_oled_temp_message.line1,
            (line1 != NULL) ? line1 : "",
            sizeof(g_oled_temp_message.line1) - 1U);
    g_oled_temp_message.line1[sizeof(g_oled_temp_message.line1) - 1U] = '\0';
    strncpy(g_oled_temp_message.line2,
            (line2 != NULL) ? line2 : "",
            sizeof(g_oled_temp_message.line2) - 1U);
    g_oled_temp_message.line2[sizeof(g_oled_temp_message.line2) - 1U] = '\0';

    g_oled_status_line1[0] = '\0';
    g_oled_status_line2[0] = '\0';
    OLED_RenderLines(g_oled_temp_message.line1, g_oled_temp_message.line2);
}

static void OLED_UpdateGameplayDisplay(uint32_t now)
{
    if (!g_oled_ready) {
        return;
    }
    if (g_game_reset_pending) {
        return;
    }
    if (Menu_GetState(&g_menu) != MENU_CLOSED) {
        return;
    }
    if (g_game == GAME_ARROW) {
        return;
    }

    if (g_oled_temp_message.active) {
        if ((int32_t)(g_oled_temp_message.expiry_tick - now) <= 0) {
            OLED_ResetStatus();
        } else {
            OLED_RenderLines(g_oled_temp_message.line1, g_oled_temp_message.line2);
            return;
        }
    }

    if (g_game == GAME_RLGL) {
        const char *mode_line = (g_role == ROLE_PLAYER) ? "Mode: Player" : "Mode: Enforcer";
        const char *status_line = (g_phase == PHASE_GREEN) ? "Green Light!" : "Red Light!";
        OLED_RenderLines(mode_line, status_line);
    } else if (g_game == GAME_CATCH) {
        const char *mode_line = (g_role == ROLE_PLAYER) ? "Mode: Player" : "Mode: Enforcer";
        if (g_catch_state == CATCH_ALERT) {
            const char *alert_line = (g_role == ROLE_PLAYER) ? "!!Enforcer Nearby!!" : "!!Player Nearby!!";
            uint32_t elapsed = (g_catch_event_start == 0U) ? 0U : (now - g_catch_event_start);
            char countdown_line[32];
            if (elapsed >= 3000U) {
                snprintf(countdown_line, sizeof(countdown_line), "Countdown: 0");
            } else {
                uint32_t remaining = 3U - (elapsed / 1000U);
                if (remaining > 3U) {
                    remaining = 3U;
                }
                if (remaining == 0U) {
                    remaining = 1U;
                }
                snprintf(countdown_line, sizeof(countdown_line), "Countdown: %u", (unsigned)remaining);
            }
            OLED_RenderLines(alert_line, countdown_line);
        } else {
            OLED_RenderLines(mode_line, "Be Alert");
        }
    }
}

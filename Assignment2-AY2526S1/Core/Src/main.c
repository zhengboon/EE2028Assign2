/******************************************************************************
 * @file    : main.c
 * @brief   : Sotong Game (RLGL + Catch & Run) — non-blocking (HAL_GetTick only)
 ******************************************************************************/

#include "main.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>   /* for abs() */

/* BSP drivers */
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_accelero.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_gyro.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_magneto.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_hsensor.h"
#include "../../Drivers/BSP/B-L4S5I-IOT01/stm32l4s5i_iot01_psensor.h"

/* ========= UART ========= */
UART_HandleTypeDef huart1;
static void UART1_Init(void);
static void MX_GPIO_Init(void);

static void printu(const char *fmt, ...)
{
    char buf[256];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) HAL_UART_Transmit(&huart1, (uint8_t*)buf, (uint16_t)n, HAL_MAX_DELAY);
}

/* ========= Game / Role defs ========= */
typedef enum { GAME_RLGL = 0, GAME_CATCH = 1 } game_t;
typedef enum { ROLE_PLAYER = 1, ROLE_ENFORCER = 2 } role_t;
typedef enum { PHASE_GREEN = 0, PHASE_RED = 1 } rlgl_phase_t;

/* Thresholds */
#define ACCEL_THRESHOLD_MS2   2.0f
#define GYRO_THRESHOLD_DPS    50.0f
#define TEMP_THRESH_C         30.0f
#define HUMID_THRESH_PCT      70.0f
#define PRESS_THRESH_HPA      63.0f

static int MAG_THRESH[3] = { 500, 2000, 10000 };

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
static uint8_t  g_prox_flag = 0;
static uint8_t  g_escape_active = 0;
static uint32_t g_escape_start = 0;
static uint32_t t_envCatch = 0;
static uint8_t was_temp_high=0, was_hum_high=0, was_press_high=0;

/* ========= LED alert blinker (Catch mode) ========= */
typedef struct {
    uint8_t enabled;
    uint32_t period_ms;
    uint32_t last_toggle_ms;
} led_blink_t;

static led_blink_t alert_blink = {0, 0, 0};

static void led_set_blink(int level) /* -1 off, 0 slow, 1 med, 2 fast */
{
    if (level < 0) { alert_blink.enabled = 0; BSP_LED_Off(LED2); return; }
    alert_blink.enabled = 1;
    alert_blink.last_toggle_ms = HAL_GetTick();
    switch (level) {
        case 2: alert_blink.period_ms = 20U;  break;
        case 1: alert_blink.period_ms = 40U;  break;
        default:alert_blink.period_ms = 80U;  break;
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
		

     	 if (n == 2) {
            if (g_game == GAME_RLGL) {
                g_game = GAME_CATCH;
                printu("Entering Catch And Run as %s\r\n",
                       (g_role == ROLE_PLAYER) ? "Player" : "Enforcer");
                alert_blink.enabled = 0; BSP_LED_Off(LED2);
                int16_t m[3]; BSP_MAGNETO_GetXYZ(m);
                g_mag_baseline = abs(m[0]) + abs(m[1]) + abs(m[2]);
                g_prox_flag = 0; g_escape_active = 0;
                t_envCatch = now;
                was_temp_high=was_hum_high=was_press_high=0;
            } else if (g_game == GAME_CATCH){
                g_game = GAME_RLGL;
                printu("Entering Red Light, Green Light as %s\r\n",
                       (g_role == ROLE_PLAYER) ? "Player" : "Enforcer");
                g_phase = PHASE_GREEN; g_gameOver = 0;
                t_phaseSwitch = now;
                t_envRLGL = t_motionRLGL = t_ledHB = 0;
                alert_blink.enabled = 0; BSP_LED_On(LED2);
                printu("Green Light!\r\n");
            }
		
        }
		else if(n ==3){
			if(g_game == GAME_CATCH){
				if (g_role == ROLE_PLAYER) {
					g_role = ROLE_ENFORCER;
					printu("Role switched to Enforcer\r\n");
				} else {
					g_role = ROLE_PLAYER;
					printu("Role switched to Player\r\n");
				}
			}

				
			
		}
		
        
		else {
            single_press_event = 1;
        }
    }
}

/* ========= Init ========= */
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

/* ========= MAIN ========= */
int main(void)
{
    HAL_Init();
    MX_GPIO_Init();
    UART1_Init();

    BSP_LED_Init(LED2);
    BSP_ACCELERO_Init();
    BSP_GYRO_Init();
    BSP_MAGNETO_Init();
    BSP_TSENSOR_Init();
    BSP_HSENSOR_Init();
    BSP_PSENSOR_Init();

    /* Default: RLGL as Player */
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

    while (1)
    {
        uint32_t tickstart = HAL_GetTick();
        const uint32_t wait = 1000U;

        while ((HAL_GetTick() - tickstart) < wait)
        {
            now = HAL_GetTick();

            process_clicks(now);
            led_blink_process(now);

            /* ---- Game 1: RLGL ---- */
            if (g_game == GAME_RLGL) {
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
                }//

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
                    if (!g_gameOver && (now - t_motionRLGL) >= 2000U) {
                        t_motionRLGL = now;

                        int16_t ar[3] = {0}; BSP_ACCELERO_AccGetXYZ(ar);
                        float ax = ar[0] * (9.8f / 1000.0f);
                        float ay = ar[1] * (9.8f / 1000.0f);
                        float az = ar[2] * (9.8f / 1000.0f);
                        float a_mag = sqrtf(ax*ax + ay*ay + az*az);

                        float g[3] = {0.f, 0.f, 0.f}; BSP_GYRO_GetXYZ(g);
                        float g_mag = sqrtf(g[0]*g[0] + g[1]*g[1] + g[2]*g[2]);

                        printu("Acceleration[%.2f,%.2f,%.2f] Gyroscope[%.2f,%.2f,%.2f]\r\n",
                               ax, ay, az, g[0], g[1], g[2]);

                        if ((a_mag > ACCEL_THRESHOLD_MS2) || (g_mag > GYRO_THRESHOLD_DPS)) {
                            if (g_role == ROLE_PLAYER) { printu("Game Over\r\n"); g_gameOver = 1; BSP_LED_Off(LED2); }
                            else { printu("Player Out!\r\n"); }
                        }
                    }
                }
            }
            /* ---- Game 2: Catch & Run ---- */
            else {
                int16_t x[3]; BSP_MAGNETO_GetXYZ(x);
                int sum = abs(x[0]) + abs(x[1]) + abs(x[2]);
                int diff = sum - g_mag_baseline; if (diff < 0) diff = -diff;

                int level = -1;
                if (diff > MAG_THRESH[2])      level = 2;
                else if (diff > MAG_THRESH[1]) level = 1;
                else if (diff > MAG_THRESH[0]) level = 0;

                if (level >= 0) {
                    if (!g_prox_flag) {
                        g_prox_flag = 1;
                        g_escape_active = 1; g_escape_start = now;
                        single_press_event = 0;
                        if (g_role == ROLE_PLAYER) printu("Enforcer nearby! Be careful.\r\n");
                        else                       printu("Player is Nearby! Move faster.\r\n");
                    }
                    led_set_blink(level);
                } else {
                    g_prox_flag = 0; g_escape_active = 0; led_set_blink(-1);
                }

                if (g_escape_active) {
                    if (single_press_event) {
                        single_press_event = 0; g_escape_active = 0; led_set_blink(-1);
                        if (g_role == ROLE_PLAYER) printu("Player escaped, good job!\r\n");
                        else                       printu("Player captured, good job!\r\n");
                    } else if ((now - g_escape_start) >= 3000U) {
                        g_escape_active = 0; led_set_blink(-1);
                        if (g_role == ROLE_PLAYER) printu("Game Over!\r\n");
                        else                       printu("Player escaped! Keep trying.\r\n");
                    }
                }

                if ((now - t_envCatch) >= 1000U) {
                    t_envCatch = now;
                    float t = BSP_TSENSOR_ReadTemp();
                    float h = BSP_HSENSOR_ReadHumidity();
                    float p = BSP_PSENSOR_ReadPressure();

                    uint8_t th = (t > TEMP_THRESH_C);
                    uint8_t hh = (h > HUMID_THRESH_PCT);
                    uint8_t ph = (p > PRESS_THRESH_HPA);

                    if (th && !was_temp_high)  printu("Temperature spike detected! T:%.2fC. Dangerous environment!\r\n", t);
                    if (hh && !was_hum_high)   printu("Humidity spike detected! H:%.2f%%.\r\n", h);
                    if (ph && !was_press_high) printu("Pressure spike detected! P:%.2fhPa.\r\n", p);

                    if (!th && was_temp_high)  printu("Temperature back to normal: %.2fC\r\n", t);
                    if (!hh && was_hum_high)   printu("Humidity back to normal: %.2f%%\r\n", h);
                    if (!ph && was_press_high) printu("Pressure back to normal: %.2fhPa\r\n", p);

                    was_temp_high = th; was_hum_high = hh; was_press_high = ph;
                }
            }
        }
    }
}



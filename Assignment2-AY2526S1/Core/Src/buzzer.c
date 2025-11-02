#include "buzzer.h"

#include "buzzer.h"
#include "main.h"

#define BUZZER_TIMER_CLOCK_HZ 1000000UL

typedef struct {
    float     frequency_hz;
    uint32_t  duration_ms;
    uint32_t  gap_ms;
} buzzer_step_t;

typedef struct {
    uint8_t   active;
    uint32_t  duration_ms;
    uint32_t  start_tick_ms;
    uint32_t  gap_ms;
} buzzer_play_state_t;

typedef struct {
    const buzzer_step_t *steps;
    uint8_t              count;
    uint8_t              index;
    uint8_t              running;
    uint8_t              waiting_gap;
    uint32_t             next_event_tick_ms;
} buzzer_sequence_state_t;

static TIM_HandleTypeDef htim2;
static buzzer_play_state_t     g_buzzer_play = {0};
static buzzer_sequence_state_t g_buzzer_seq  = {0};

static void Buzzer_TIM_Init(void);
static void Buzzer_Set(uint8_t on);
static void Buzzer_PlayToneInternal(float frequency_hz,
                                    uint32_t duration_ms,
                                    uint32_t gap_ms_after);
static void Buzzer_SequenceStart(const buzzer_step_t *steps, uint8_t count);
static void Buzzer_SequenceAdvance(uint32_t now_ms);
static void Buzzer_Stop(void);

void Buzzer_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    HAL_GPIO_WritePin(ARD_D4_GPIO_Port, ARD_D4_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = ARD_D4_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ARD_D4_GPIO_Port, &GPIO_InitStruct);

    Buzzer_TIM_Init();
    Buzzer_Stop();

    g_buzzer_seq.steps             = NULL;
    g_buzzer_seq.count             = 0U;
    g_buzzer_seq.index             = 0U;
    g_buzzer_seq.running           = 0U;
    g_buzzer_seq.waiting_gap       = 0U;
    g_buzzer_seq.next_event_tick_ms= 0U;
}

void Buzzer_PlayToneAsync(float frequency_hz, uint32_t duration_ms)
{
    g_buzzer_seq.running = 0U;
    g_buzzer_seq.waiting_gap = 0U;
    Buzzer_PlayToneInternal(frequency_hz, duration_ms, 0U);
}

void Buzzer_PlayNoteAsync(buzzer_note_t note, uint32_t duration_ms)
{
    if (note >= BUZZER_NOTE_COUNT) {
        return;
    }
    Buzzer_PlayToneAsync(buzzer_note_frequency(note), duration_ms);
}

void Buzzer_Service(uint32_t now_ms)
{
    if (g_buzzer_play.active) {
        if ((now_ms - g_buzzer_play.start_tick_ms) >= g_buzzer_play.duration_ms) {
            uint32_t gap_ms = g_buzzer_play.gap_ms;
            Buzzer_Stop();
            if (g_buzzer_seq.running) {
                if (gap_ms == 0U) {
                    Buzzer_SequenceAdvance(now_ms);
                } else {
                    g_buzzer_seq.waiting_gap = 1U;
                    g_buzzer_seq.next_event_tick_ms = now_ms + gap_ms;
                }
            }
        }
    } else if (g_buzzer_seq.running && g_buzzer_seq.waiting_gap) {
        if ((int32_t)(now_ms - g_buzzer_seq.next_event_tick_ms) >= 0) {
            g_buzzer_seq.waiting_gap = 0U;
            Buzzer_SequenceAdvance(now_ms);
        }
    }
}

void Buzzer_TestPattern(void)
{
    static const buzzer_note_t notes[] = {
        BUZZER_NOTE_A4,
        BUZZER_NOTE_AS4,
        BUZZER_NOTE_B4,
        BUZZER_NOTE_C5,
        BUZZER_NOTE_CS5,
        BUZZER_NOTE_D5,
        BUZZER_NOTE_DS5,
        BUZZER_NOTE_E5,
        BUZZER_NOTE_F5,
        BUZZER_NOTE_FS5,
        BUZZER_NOTE_G5,
        BUZZER_NOTE_GS5,
        BUZZER_NOTE_A5
    };
    enum { PATTERN_COUNT = sizeof(notes) / sizeof(notes[0]) };
    static buzzer_step_t pattern[PATTERN_COUNT];
    static uint8_t       initialized = 0U;

    if (!initialized) {
        for (uint8_t i = 0U; i < PATTERN_COUNT; ++i) {
            pattern[i].frequency_hz = buzzer_note_frequency(notes[i]);
            pattern[i].duration_ms  = 180U;
            pattern[i].gap_ms       = 40U;
        }
        initialized = 1U;
    }

    Buzzer_SequenceStart(pattern, PATTERN_COUNT);
}

void Buzzer_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        if (g_buzzer_play.active) {
            HAL_GPIO_TogglePin(ARD_D4_GPIO_Port, ARD_D4_Pin);
        }
    }
}

static void Buzzer_TIM_Init(void)
{
    uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
    RCC_ClkInitTypeDef clk_config;
    uint32_t flash_latency;

    HAL_RCC_GetClockConfig(&clk_config, &flash_latency);
    if (clk_config.APB1CLKDivider != RCC_HCLK_DIV1) {
        tim_clk *= 2U;
    }

    uint32_t prescaler = (tim_clk + (BUZZER_TIMER_CLOCK_HZ - 1U)) / BUZZER_TIMER_CLOCK_HZ;
    if (prescaler == 0U) {
        prescaler = 1U;
    }

    htim2.Instance = TIM2;
    htim2.Init.Prescaler         = prescaler - 1U;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = (BUZZER_TIMER_CLOCK_HZ / 1000U) - 1U;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }
}

static void Buzzer_Set(uint8_t on)
{
    HAL_GPIO_WritePin(ARD_D4_GPIO_Port, ARD_D4_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void Buzzer_Stop(void)
{
    g_buzzer_play.active        = 0U;
    g_buzzer_play.duration_ms   = 0U;
    g_buzzer_play.gap_ms        = 0U;
    g_buzzer_play.start_tick_ms = 0U;
    HAL_TIM_Base_Stop_IT(&htim2);
    Buzzer_Set(0U);
}

static void Buzzer_PlayToneInternal(float frequency_hz,
                                    uint32_t duration_ms,
                                    uint32_t gap_ms_after)
{
    if ((frequency_hz <= 0.0f) || (duration_ms == 0U)) {
        Buzzer_Stop();
        return;
    }

    uint32_t reload = buzzer_compute_toggle_ticks(BUZZER_TIMER_CLOCK_HZ,
                                                  frequency_hz);
    if (reload == 0U) {
        Buzzer_Stop();
        return;
    }

    HAL_TIM_Base_Stop_IT(&htim2);
    Buzzer_Set(0U);

    __HAL_TIM_SET_AUTORELOAD(&htim2, (reload > 0U) ? (reload - 1U) : 0U);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);

    g_buzzer_play.active        = 1U;
    g_buzzer_play.duration_ms   = duration_ms;
    g_buzzer_play.start_tick_ms = HAL_GetTick();
    g_buzzer_play.gap_ms        = gap_ms_after;

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
        Buzzer_Stop();
        return;
    }
}

static void Buzzer_SequenceAdvance(uint32_t now_ms)
{
    if (!g_buzzer_seq.running) {
        return;
    }

    if ((g_buzzer_seq.index + 1U) >= g_buzzer_seq.count) {
        g_buzzer_seq.running     = 0U;
        g_buzzer_seq.waiting_gap = 0U;
        return;
    }

    g_buzzer_seq.index++;
    const buzzer_step_t *step = &g_buzzer_seq.steps[g_buzzer_seq.index];
    Buzzer_PlayToneInternal(step->frequency_hz, step->duration_ms, step->gap_ms);
    (void)now_ms;
}

static void Buzzer_SequenceStart(const buzzer_step_t *steps, uint8_t count)
{
    if ((steps == NULL) || (count == 0U)) {
        return;
    }

    Buzzer_Stop();

    g_buzzer_seq.steps       = steps;
    g_buzzer_seq.count       = count;
    g_buzzer_seq.index       = 0U;
    g_buzzer_seq.running     = 1U;
    g_buzzer_seq.waiting_gap = 0U;
    g_buzzer_seq.next_event_tick_ms = 0U;

    const buzzer_step_t *first = &steps[0];
    Buzzer_PlayToneInternal(first->frequency_hz, first->duration_ms, first->gap_ms);
}

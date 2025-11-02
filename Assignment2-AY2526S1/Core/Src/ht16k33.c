#include "ht16k33.h"

#define HT16K33_CMD_SYSTEM_SETUP 0x20U
#define HT16K33_CMD_DISPLAY      0x80U
#define HT16K33_CMD_DIMMING      0xE0U

static HAL_StatusTypeDef ht16k33_write_cmd(const HT16K33_HandleTypeDef *h, uint8_t cmd)
{
    return HAL_I2C_Master_Transmit(h->hi2c, h->i2c_addr, &cmd, 1u, HAL_MAX_DELAY);
}

HAL_StatusTypeDef HT16K33_Init(HT16K33_HandleTypeDef *h, I2C_HandleTypeDef *hi2c, uint8_t addr7bit)
{
    if (h == NULL || hi2c == NULL) return HAL_ERROR;

    h->hi2c     = hi2c;
    h->i2c_addr = (uint16_t)(addr7bit << 1);
    HT16K33_Clear(h);

    /* Turn on oscillator */
    HAL_StatusTypeDef res = ht16k33_write_cmd(h, HT16K33_CMD_SYSTEM_SETUP | 0x01U);
    if (res != HAL_OK) return res;

    /* Display on, no blink by default */
    res = HT16K33_SetBlinkRate(h, 0U);
    if (res != HAL_OK) return res;

    /* Max brightness by default */
    return HT16K33_SetBrightness(h, 15U);
}

HAL_StatusTypeDef HT16K33_SetBrightness(HT16K33_HandleTypeDef *h, uint8_t level)
{
    if (h == NULL) return HAL_ERROR;
    level &= 0x0FU;
    return ht16k33_write_cmd(h, (uint8_t)(HT16K33_CMD_DIMMING | level));
}

HAL_StatusTypeDef HT16K33_SetBlinkRate(HT16K33_HandleTypeDef *h, uint8_t rate)
{
    if (h == NULL) return HAL_ERROR;
    rate &= 0x03U;
    /* Display on + blink rate */
    return ht16k33_write_cmd(h, (uint8_t)(HT16K33_CMD_DISPLAY | 0x01U | (rate << 1)));
}

HAL_StatusTypeDef HT16K33_Update(const HT16K33_HandleTypeDef *h)
{
    if (h == NULL) return HAL_ERROR;
    uint8_t payload[17];
    payload[0] = 0x00U; /* RAM address pointer */
    for (uint8_t i = 0; i < 16; ++i) {
        payload[i + 1U] = h->frame[i];
    }
    return HAL_I2C_Master_Transmit(h->hi2c, h->i2c_addr, payload, sizeof payload, HAL_MAX_DELAY);
}

void HT16K33_Clear(HT16K33_HandleTypeDef *h)
{
    if (h == NULL) return;
    for (uint8_t i = 0; i < 16; ++i) {
        h->frame[i] = 0U;
    }
}

void HT16K33_SetRow(HT16K33_HandleTypeDef *h, uint8_t row, uint8_t pattern)
{
    if (h == NULL || row >= 8U) return;
    const uint8_t index = (uint8_t)(row << 1);
    h->frame[index]     = pattern;
    h->frame[index + 1] = 0U; /* upper bits unused for 8x8 matrix */
}

void HT16K33_DrawBitmap64(HT16K33_HandleTypeDef *h, uint64_t bitmap)
{
    if (h == NULL) return;
    for (uint8_t row = 0U; row < 8U; ++row) {
        uint8_t pattern = (uint8_t)((bitmap >> (row * 8U)) & 0xFFU);
        HT16K33_SetRow(h, row, pattern);
    }
}

HAL_StatusTypeDef HT16K33_DisplayBitmap64(HT16K33_HandleTypeDef *h, uint64_t bitmap)
{
    if (h == NULL) return HAL_ERROR;
    HT16K33_DrawBitmap64(h, bitmap);
    return HT16K33_Update(h);
}

#ifndef HT16K33_H
#define HT16K33_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HT16K33_I2C_ADDR_DEFAULT 0x70U

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t i2c_addr;           /* 8-bit address expected by HAL */
    uint8_t frame[16];           /* HT16K33 RAM shadow (16 bytes) */
} HT16K33_HandleTypeDef;

HAL_StatusTypeDef HT16K33_Init(HT16K33_HandleTypeDef *h, I2C_HandleTypeDef *hi2c, uint8_t addr7bit);
HAL_StatusTypeDef HT16K33_SetBrightness(HT16K33_HandleTypeDef *h, uint8_t level);
HAL_StatusTypeDef HT16K33_SetBlinkRate(HT16K33_HandleTypeDef *h, uint8_t rate);
HAL_StatusTypeDef HT16K33_Update(const HT16K33_HandleTypeDef *h);
void HT16K33_Clear(HT16K33_HandleTypeDef *h);
void HT16K33_SetRow(HT16K33_HandleTypeDef *h, uint8_t row, uint8_t pattern);
void HT16K33_DrawBitmap64(HT16K33_HandleTypeDef *h, uint64_t bitmap);
HAL_StatusTypeDef HT16K33_DisplayBitmap64(HT16K33_HandleTypeDef *h, uint64_t bitmap);

#ifdef __cplusplus
}
#endif

#endif /* HT16K33_H */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include "buzzer_scale.h"

void Buzzer_Init(void);
void Buzzer_PlayToneAsync(float frequency_hz, uint32_t duration_ms);
void Buzzer_PlayNoteAsync(buzzer_note_t note, uint32_t duration_ms);
void Buzzer_Service(uint32_t now_ms);
void Buzzer_TestPattern(void);
void Buzzer_IRQHandler(void);

#endif /* BUZZER_H */

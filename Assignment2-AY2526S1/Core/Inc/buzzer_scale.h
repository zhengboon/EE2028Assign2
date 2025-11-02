#ifndef BUZZER_SCALE_H
#define BUZZER_SCALE_H

#include <stdint.h>

typedef struct {
    const char *name;
    float       frequency_hz;
} buzzer_tone_t;

#define BUZZER_CHROMATIC_A3_A6_COUNT 37U

static const buzzer_tone_t buzzer_chromatic_a3_a6[BUZZER_CHROMATIC_A3_A6_COUNT] = {
    {"A3", 220.00f},
    {"A#3/Bb3", 233.08f},
    {"B3", 246.94f},
    {"C4", 261.63f},
    {"C#4/Db4", 277.18f},
    {"D4", 293.66f},
    {"D#4/Eb4", 311.13f},
    {"E4", 329.63f},
    {"F4", 349.23f},
    {"F#4/Gb4", 369.99f},
    {"G4", 392.00f},
    {"G#4/Ab4", 415.30f},
    {"A4", 440.00f},
    {"A#4/Bb4", 466.16f},
    {"B4", 493.88f},
    {"C5", 523.25f},
    {"C#5/Db5", 554.37f},
    {"D5", 587.33f},
    {"D#5/Eb5", 622.25f},
    {"E5", 659.26f},
    {"F5", 698.46f},
    {"F#5/Gb5", 739.99f},
    {"G5", 783.99f},
    {"G#5/Ab5", 830.61f},
    {"A5", 880.00f},
    {"A#5/Bb5", 932.33f},
    {"B5", 987.77f},
    {"C6", 1046.50f},
    {"C#6/Db6", 1108.73f},
    {"D6", 1174.66f},
    {"D#6/Eb6", 1244.51f},
    {"E6", 1318.51f},
    {"F6", 1396.91f},
    {"F#6/Gb6", 1479.98f},
    {"G6", 1567.98f},
    {"G#6/Ab6", 1661.22f},
    {"A6", 1760.00f}
};

typedef enum {
    BUZZER_NOTE_A3 = 0,
    BUZZER_NOTE_AS3 = 1,
    BUZZER_NOTE_B3 = 2,
    BUZZER_NOTE_C4 = 3,
    BUZZER_NOTE_CS4 = 4,
    BUZZER_NOTE_D4 = 5,
    BUZZER_NOTE_DS4 = 6,
    BUZZER_NOTE_E4 = 7,
    BUZZER_NOTE_F4 = 8,
    BUZZER_NOTE_FS4 = 9,
    BUZZER_NOTE_G4 = 10,
    BUZZER_NOTE_GS4 = 11,
    BUZZER_NOTE_A4 = 12,
    BUZZER_NOTE_AS4 = 13,
    BUZZER_NOTE_B4 = 14,
    BUZZER_NOTE_C5 = 15,
    BUZZER_NOTE_CS5 = 16,
    BUZZER_NOTE_D5 = 17,
    BUZZER_NOTE_DS5 = 18,
    BUZZER_NOTE_E5 = 19,
    BUZZER_NOTE_F5 = 20,
    BUZZER_NOTE_FS5 = 21,
    BUZZER_NOTE_G5 = 22,
    BUZZER_NOTE_GS5 = 23,
    BUZZER_NOTE_A5 = 24,
    BUZZER_NOTE_AS5 = 25,
    BUZZER_NOTE_B5 = 26,
    BUZZER_NOTE_C6 = 27,
    BUZZER_NOTE_CS6 = 28,
    BUZZER_NOTE_D6 = 29,
    BUZZER_NOTE_DS6 = 30,
    BUZZER_NOTE_E6 = 31,
    BUZZER_NOTE_F6 = 32,
    BUZZER_NOTE_FS6 = 33,
    BUZZER_NOTE_G6 = 34,
    BUZZER_NOTE_GS6 = 35,
    BUZZER_NOTE_A6 = 36,
    BUZZER_NOTE_COUNT = BUZZER_CHROMATIC_A3_A6_COUNT
} buzzer_note_t;

static inline uint32_t buzzer_compute_toggle_ticks(uint32_t timer_clock_hz,
                                                   float frequency_hz)
{
    if (frequency_hz <= 0.0f || timer_clock_hz == 0U) {
        return 0U;
    }
    float half_period = (float)timer_clock_hz / (frequency_hz * 2.0f);
    if (half_period < 1.0f) {
        return 1U;
    }
    return (uint32_t)(half_period + 0.5f);
}

static inline float buzzer_note_frequency(buzzer_note_t note)
{
    if (note >= BUZZER_NOTE_COUNT) {
        return 0.0f;
    }
    return buzzer_chromatic_a3_a6[note].frequency_hz;
}

static inline const char *buzzer_note_name(buzzer_note_t note)
{
    if (note >= BUZZER_NOTE_COUNT) {
        return "";
    }
    return buzzer_chromatic_a3_a6[note].name;
}

#endif /* BUZZER_SCALE_H */

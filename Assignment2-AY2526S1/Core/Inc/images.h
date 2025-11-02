#ifndef IMAGES_H
#define IMAGES_H

#include <stdint.h>
#include <stddef.h>

/* Intro animation frames shown during system start-up */
static const uint64_t HT16K33_IMAGES_INTRO[] = {
    0x3c66760606663c00ULL,
    0x7c667c603c000000ULL,
    0xd6d6feeec6000000ULL,
    0x3c067e663c000000ULL,
    0x0000000000000000ULL
};
#define HT16K33_IMAGES_INTRO_LEN (sizeof(HT16K33_IMAGES_INTRO) / sizeof(HT16K33_IMAGES_INTRO[0]))

/* Game-over animation frames reused by both roles */
static const uint64_t HT16K33_IMAGES_GAMEOVER[] = {
    0x3c66760606663c00,
  0x7c667c603c000000,
  0xd6d6feeec6000000,
  0x3c067e663c000000,
  0x0000000000000000,
  0x3c66666666663c00,
  0x183c666600000000,
  0x3c067e663c000000,
  0x060666663e000000,
  0x00c3e77e00240000,
  0x00c3e77e00240000,
  0x00c3e77e00240000,
  0x0000000000000000
};
#define HT16K33_IMAGES_GAMEOVER_LEN (sizeof(HT16K33_IMAGES_GAMEOVER) / sizeof(HT16K33_IMAGES_GAMEOVER[0]))

#endif /* IMAGES_H */

/*
 * ESP-IDF Random Number Compatibility — PC Simulator
 * ======================================================
 * Maps esp_random() to standard rand() on PC.
 * Uses two rand() calls to cover 32 bits, since Windows
 * MSVC rand() only returns 15 bits (RAND_MAX=32767).
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Random Number Functions
 * ============================================================ */

/**
 * @brief Get a random 32-bit value (PC: uses two rand() calls)
 *
 * Windows MSVC rand() only returns 15 bits (RAND_MAX=32767),
 * so we combine two calls to produce a full 32-bit value.
 * This ensures the blink pattern randomization in emotion_engine.c
 * has sufficient entropy.
 */
static inline uint32_t esp_random(void)
{
    uint32_t r = (uint32_t)rand();
    r = (r << 16) ^ ((uint32_t)rand() & 0xFFFF);
    return r;
}

/**
 * @brief Seed the random number generator
 * Call this once at program startup.
 */
static inline void esp_random_seed(void)
{
    srand((unsigned int)time(NULL));
}

#ifdef __cplusplus
}
#endif
/*
 * ESP-IDF Timer Compatibility — PC Simulator Implementation
 * =============================================================
 * Uses clock_gettime(CLOCK_MONOTONIC) on POSIX and
 * QueryPerformanceCounter on Windows for microsecond timing.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "esp_timer.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

int64_t esp_timer_get_time(void)
{
#ifdef _WIN32
    /* Windows: use high-resolution performance counter */
    static LARGE_INTEGER s_freq = {0};
    static int s_freq_initialized = 0;

    if (!s_freq_initialized) {
        QueryPerformanceFrequency(&s_freq);
        s_freq_initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    /* Convert to microseconds: (counter * 1000000) / freq */
    return (int64_t)((counter.QuadPart * 1000000LL) / s_freq.QuadPart);
#else
    /* POSIX: use monotonic clock */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec) * 1000000LL + (int64_t)(ts.tv_nsec) / 1000LL;
#endif
}
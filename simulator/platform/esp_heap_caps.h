/*
 * ESP-IDF Heap Capabilities Compatibility — PC Simulator
 * ==========================================================
 * Maps heap_caps_malloc/free to standard malloc/free on PC.
 * Memory capability flags are ignored — PC has unified memory.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Memory Capability Flags (ignored on PC)
 * ============================================================ */

#define MALLOC_CAP_SPIRAM       0x01
#define MALLOC_CAP_DMA          0x02
#define MALLOC_CAP_INTERNAL     0x04
#define MALLOC_CAP_DEFAULT      0x08
#define MALLOC_CAP_8BIT         0x10
#define MALLOC_CAP_32BIT        0x20

/* ============================================================
 * Memory Allocation Functions
 * ============================================================ */

/**
 * @brief Allocate memory with capability flags (PC: standard malloc)
 *
 * On PC, all memory flags are ignored — malloc always succeeds
 * if sufficient heap memory is available.
 */
static inline void *heap_caps_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    return malloc(size);
}

/**
 * @brief Free memory allocated by heap_caps_malloc
 */
static inline void heap_caps_free(void *ptr)
{
    free(ptr);
}

/**
 * @brief Get free heap size (PC: returns a large constant)
 */
static inline size_t heap_caps_get_free_size(uint32_t caps)
{
    (void)caps;
    return (size_t)(100 * 1024 * 1024);  /* Report 100MB available */
}

/**
 * @brief Get minimum free heap ever (PC: returns same as free)
 */
static inline size_t heap_caps_get_minimum_free_size(uint32_t caps)
{
    return heap_caps_get_free_size(caps);
}

#ifdef __cplusplus
}
#endif
/*
 * Display Simulator Extra API — PC Only
 * =======================================
 * Additional functions only available in the PC simulator,
 * not part of the shared display_manager.h interface.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration — avoids leaking SDL2 dependency to
 * translation units that only need the window handle type.
 * Include <SDL.h> only in display_sim.c where SDL is used.
 */
typedef struct SDL_Window SDL_Window;

/**
 * @brief Get the SDL window handle
 * @return SDL_Window pointer, or NULL if not initialized
 */
SDL_Window *display_get_window(void);

/**
 * @brief Get the current window scale factor
 * @return Scale factor (1-4)
 */
int display_get_scale(void);

/**
 * @brief Set the window scale factor
 * @param scale New scale factor (1-4), clamped if out of range
 */
void display_set_scale(int scale);

#ifdef __cplusplus
}
#endif
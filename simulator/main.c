/*
 * RobotBuddy — Emotion Simulator (PC)
 * =====================================
 * Runs the emotion engine in an SDL2 window for development
 * and debugging without ESP32 hardware.
 *
 * Keys:
 *   1-0:  Switch emotion (IDLE through SLEEP)
 *   Q:    EXCITED emotion
 *   Space: Force blink (resets blink timer)
 *   +/-:  Zoom window in/out
 *   ESC:  Quit
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Shared firmware headers (included via platform compat layer) ---- */
#include "display_manager.h"
#include "display_sim.h"          /* PC-specific: SDL window & zoom API */
#include "emotion_engine.h"
#include "robot_events.h"

/* ---- Platform compatibility ---- */
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "simulator";

/* ============================================================
 * Key Mapping Table
 * ============================================================ */

typedef struct {
    SDL_Keycode key;
    emotion_id_t emotion;
    const char *name;
} key_mapping_t;

static const key_mapping_t s_key_map[] = {
    { SDLK_1, EMOTION_IDLE,       "IDLE" },
    { SDLK_2, EMOTION_LISTENING,  "LISTENING" },
    { SDLK_3, EMOTION_THINKING,  "THINKING" },
    { SDLK_4, EMOTION_ANSWERING, "ANSWERING" },
    { SDLK_5, EMOTION_HAPPY,     "HAPPY" },
    { SDLK_6, EMOTION_CONFUSED,  "CONFUSED" },
    { SDLK_7, EMOTION_WARNING,   "WARNING" },
    { SDLK_8, EMOTION_ERROR,     "ERROR" },
    { SDLK_9, EMOTION_FOCUS,     "FOCUS" },
    { SDLK_0, EMOTION_SLEEP,     "SLEEP" },
    { SDLK_q, EMOTION_EXCITED,   "EXCITED" },
};

#define KEY_MAP_SIZE (sizeof(s_key_map) / sizeof(s_key_map[0]))

/* ============================================================
 * Window Title Update
 * ============================================================ */

static void update_window_title(emotion_id_t emotion, float fps)
{
    SDL_Window *window = display_get_window();
    if (window == NULL) return;

    char title[256];
    snprintf(title, sizeof(title),
             "RobotBuddy Simulator | %s | FPS: %.1f | "
             "1-0/Q:emotion  Space:blink  +/-:zoom  ESC:quit",
             emotion_get_name(emotion), fps);
    SDL_SetWindowTitle(window, title);
}

/* ============================================================
 * Help Text
 * ============================================================ */

static void print_help(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║       RobotBuddy Emotion Simulator v1.0         ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Key  │ Emotion      │ Visual                   ║\n");
    printf("╠═══════╪══════════════╪══════════════════════════╣\n");
    printf("║  1    │ IDLE         │ Cyan eyes, subtle motion ║\n");
    printf("║  2    │ LISTENING    │ Wide eyes, focused        ║\n");
    printf("║  3    │ THINKING     │ Blue eyes, side-to-side   ║\n");
    printf("║  4    │ ANSWERING    │ Cyan eyes, rhythmic move  ║\n");
    printf("║  5    │ HAPPY        │ Green crescent moon eyes  ║\n");
    printf("║  6    │ CONFUSED     │ Orange eyes, tilted       ║\n");
    printf("║  7    │ WARNING      │ Yellow/orange flashing    ║\n");
    printf("║  8    │ ERROR        │ Red angry squished eyes   ║\n");
    printf("║  9    │ FOCUS         │ Cyan half-closed eyes    ║\n");
    printf("║  0    │ SLEEP         │ Dark closed eyes, breath  ║\n");
    printf("║  Q    │ EXCITED      │ Cyan big bouncing eyes   ║\n");
    printf("╠═══════╪══════════════╪══════════════════════════╣\n");
    printf("║  Space │ Log message  │ (blink is automatic)     ║\n");
    printf("║  +/-   │ Zoom in/out  │                          ║\n");
    printf("║  H     │ Show help    │                          ║\n");
    printf("║  ESC   │ Quit         │                          ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("RobotBuddy Emotion Simulator v1.0\n");
    printf("(c) 2026 RobotBuddy Project\n\n");

    /* Seed random number generator */
    esp_random_seed();

    /* Initialize display manager (creates SDL2 window) */
    display_config_t display_cfg = {
        .width = 240,
        .height = 240,
        .target_fps = 30,
    };

    esp_err_t ret = display_manager_init(&display_cfg);
    if (ret != ESP_OK) {
        fprintf(stderr, "FATAL: Display manager init failed: %s\n",
                esp_err_to_name(ret));
        return 1;
    }
    ESP_LOGI(TAG, "Display initialized");

    /* Initialize emotion engine with same parameters as firmware */
    emotion_engine_config_t emotion_cfg = {
        .display_width = 240,
        .display_height = 240,
        .left_eye_cx = 80,
        .left_eye_cy = 120,
        .right_eye_cx = 160,
        .right_eye_cy = 120,
        .eye_radius = 35,
    };

    ret = emotion_engine_init(&emotion_cfg);
    if (ret != ESP_OK) {
        fprintf(stderr, "FATAL: Emotion engine init failed: %s\n",
                esp_err_to_name(ret));
        display_manager_deinit();
        return 1;
    }

    /* Start with IDLE emotion */
    emotion_set_state(EMOTION_IDLE);
    ESP_LOGI(TAG, "Emotion engine initialized, starting IDLE");

    print_help();

    /* ---- Main loop ---- */
    bool running = true;
    uint32_t last_frame_time = SDL_GetTicks();
    uint32_t frame_interval = 1000 / DISPLAY_TARGET_FPS;  /* ~33ms for 30 FPS */
    emotion_id_t current_emotion = EMOTION_IDLE;
    int title_update_counter = 0;

    while (running) {
        /* Process SDL events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                ESP_LOGI(TAG, "Window closed, exiting...");
            }
            else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;

                switch (key) {
                    case SDLK_ESCAPE:
                        running = false;
                        ESP_LOGI(TAG, "ESC pressed, exiting...");
                        break;

                    case SDLK_SPACE:
                        /* Force a blink by toggling emotion (quick reset) */
                        ESP_LOGI(TAG, "Blink triggered");
                        /* Blink is automatic — Space just logs */
                        break;

                    case SDLK_PLUS:
                    case SDLK_EQUALS:
                        /* Zoom in */
                        display_set_scale(display_get_scale() + 1);
                        ESP_LOGI(TAG, "Zoom: %dx", display_get_scale());
                        break;

                    case SDLK_MINUS:
                        /* Zoom out */
                        display_set_scale(display_get_scale() - 1);
                        ESP_LOGI(TAG, "Zoom: %dx", display_get_scale());
                        break;

                    case SDLK_h:
                        print_help();
                        break;

                    default:
                        /* Check emotion key mappings */
                        for (size_t i = 0; i < KEY_MAP_SIZE; i++) {
                            if (key == s_key_map[i].key) {
                                current_emotion = s_key_map[i].emotion;
                                emotion_set_state(current_emotion);
                                ESP_LOGI(TAG, "Emotion: %s", s_key_map[i].name);
                                break;
                            }
                        }
                        break;
                }
            }
        }

        /* Frame rate limiting */
        uint32_t now = SDL_GetTicks();
        if (now - last_frame_time >= frame_interval) {
            /* Render emotion frame */
            uint16_t *fb = display_get_framebuffer();
            if (fb != NULL) {
                emotion_render_frame(fb, DISPLAY_WIDTH, DISPLAY_HEIGHT);
                display_commit_frame();
            }

            /* Update window title every 30 frames (~1 second) */
            title_update_counter++;
            if (title_update_counter >= 30) {
                update_window_title(current_emotion, display_get_fps());
                title_update_counter = 0;
            }

            last_frame_time = now;
        }

        /* Yield CPU — sleep 1ms to avoid busy loop */
        SDL_Delay(1);
    }

    /* Cleanup */
    ESP_LOGI(TAG, "Shutting down...");
    emotion_engine_deinit();
    display_manager_deinit();

    printf("\nRobotBuddy Simulator stopped.\n");
    return 0;
}
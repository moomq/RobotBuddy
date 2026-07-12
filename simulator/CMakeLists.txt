# RobotBuddy Emotion Simulator — CMakeLists.txt
# ================================================
# Builds the PC simulator that renders RobotBuddy's
# emotion engine in an SDL2 window without ESP32 hardware.
#
# The shared emotion_engine.c is compiled directly from
# the firmware source tree (zero-copy, zero-modification).
#
# Copyright (c) 2026 RobotBuddy Project
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)
project(robotbuddy_simulator C)

# ── C Standard ──────────────────────────────────────────────
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Disable MinGW import library generation for executables ──
# MinGW's ld exits with code 5 when --out-implib is passed for
# an executable that exports no symbols.  Override the linker
# rule so that --out-implib is never injected.
set(CMAKE_GNUtoMS OFF CACHE BOOL "" FORCE)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_LINK_EXECUTABLE
        "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
    )
endif()

# ── Compiler Warnings ───────────────────────────────────────
add_compile_options(-Wall -Wextra -Wshadow -Wformat=2)

# ── Find SDL2 ──────────────────────────────────────────────
# Strategy: try find_package(SDL2) first (works with vcpkg,
# conan, and system packages on most platforms), then fall
# back to PkgConfig.
set(SDL2_FOUND FALSE)

# Try modern CMake SDL2 config first (vcpkg, conan, brew)
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
    message(STATUS "SDL2 found via CONFIG (vcpkg/conan/brew)")
    set(SDL2_INCLUDE_DIRS "")
    set(SDL2_LIBRARIES SDL2::SDL2)
else()
    # Try find_package with module mode
    find_package(SDL2 MODULE QUIET)
    if(SDL2_FOUND)
        message(STATUS "SDL2 found via MODULE find_package")
    else()
        # Try PkgConfig as last resort
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET sdl2)
            if(SDL2_FOUND)
                message(STATUS "SDL2 found via PkgConfig")
            endif()
        endif()
    endif()
endif()

if(NOT SDL2_FOUND)
    message(FATAL_ERROR
        "SDL2 not found! Install it using one of:\n"
        "  Ubuntu/Debian: sudo apt-get install libsdl2-dev\n"
        "  macOS:          brew install sdl2\n"
        "  Windows/MSYS2:  pacman -S mingw-w64-x86_64-SDL2\n"
        "  Windows/vcpkg:  vcpkg install sdl2:x64-windows\n"
        "  Then re-run cmake with -DSDL2_DIR=<path> if needed."
    )
endif()

# ── Path Constants ──────────────────────────────────────────
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platform)
set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../firmware)

# ── Source Files ────────────────────────────────────────────
set(SOURCES
    # Simulator entry point
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c

    # Display manager PC implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/display_sim.c

    # ESP timer compatibility implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/esp_timer.c

    # FreeRTOS task compatibility implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/freertos/task.c

    # ── Shared firmware code (zero-copy from source tree) ──
    # emotion_engine.c compiles UNCHANGED — the compatibility
    # headers in platform/ provide all ESP-IDF equivalents.
    ${FIRMWARE_DIR}/components/services/emotion_engine/emotion_engine.c
)

# ── Executable ──────────────────────────────────────────────
add_executable(robotbuddy_sim ${SOURCES})

# ── Include Paths (ORDER MATTERS!) ─────────────────────────
# platform/ must come FIRST so its esp_err.h, esp_log.h, etc.
# are found before any ESP-IDF headers that firmware code expects.
target_include_directories(robotbuddy_sim PRIVATE
    # ┌──────────────────────────────────────────────────────┐
    # │ HIGHEST PRIORITY: Platform compatibility headers    │
    # │ These override ESP-IDF headers of the same name.    │
    # └──────────────────────────────────────────────────────┘
    ${PLATFORM_DIR}

    # Simulator driver headers
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers

    # ┌──────────────────────────────────────────────────────┐
    # │ FIRMWARE SHARED HEADERS                            │
    # │ Same files used by ESP32 firmware.                 │
    # └──────────────────────────────────────────────────────┘
    ${FIRMWARE_DIR}/components/services/emotion_engine/include
    ${FIRMWARE_DIR}/components/services/display_manager/include
    ${FIRMWARE_DIR}/components/framework/include
)

# ── SDL2 Linking ─────────────────────────────────────────────
# On Windows (MinGW), SDL2 and SDL2main have circular dependencies:
# SDL2main needs SDL2 symbols (SDL_strlen, SDL_SetMainReady, etc.)
# while the CRT calls WinMain which is in SDL2main.  We use
# --start-group/--end-group to resolve this circular reference.
# The flags must be passed via target_link_libraries (not
# target_link_options) so CMake interleaves them with the libs.
if(TARGET SDL2::SDL2)
    if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # MinGW: use linker group to resolve SDL2 ↔ SDL2main cycle
        target_link_libraries(robotbuddy_sim PRIVATE
            "-Wl,--start-group"
            SDL2::SDL2 SDL2::SDL2main mingw32 shell32
            "-Wl,--end-group"
        )
    else()
        target_link_libraries(robotbuddy_sim PRIVATE SDL2::SDL2)
        if(TARGET SDL2::SDL2main)
            target_link_libraries(robotbuddy_sim PRIVATE SDL2::SDL2main)
        endif()
    endif()
elseif(SDL2_LIBRARIES)
    target_link_libraries(robotbuddy_sim PRIVATE ${SDL2_LIBRARIES})
    target_include_directories(robotbuddy_sim PRIVATE ${SDL2_INCLUDE_DIRS})
    if(SDL2_LIBRARY_DIRS)
        link_directories(${SDL2_LIBRARY_DIRS})
    endif()
else()
    message(FATAL_ERROR "SDL2 libraries not resolved")
endif()

# ── Math Library ────────────────────────────────────────────
# sin() in emotion_engine.c requires -lm on Linux
if(NOT WIN32)
    target_link_libraries(robotbuddy_sim PRIVATE m)
endif()

# ── Platform-specific Settings (non-Windows) ─────────────────
# Windows libraries are already included in the SDL2 linker group above.

# macOS: CoreVideo framework needed by SDL2
if(APPLE)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    if(COREVIDEO_FRAMEWORK)
        target_link_libraries(robotbuddy_sim PRIVATE ${COREVIDEO_FRAMEWORK})
    endif()
endif()

# ── Compile Definitions ────────────────────────────────────
target_compile_definitions(robotbuddy_sim PRIVATE
    SIMULATOR_BUILD
)

# ── MSVC Compatibility ──────────────────────────────────────
if(MSVC)
    set_property(TARGET robotbuddy_sim PROPERTY C_STANDARD 11)
    target_compile_options(robotbuddy_sim PRIVATE /W4)
    target_compile_definitions(robotbuddy_sim PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    # Remove GCC-specific flags that MSVC doesn't understand
    string(REPLACE "-Wall" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Wextra" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Wshadow" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Wformat=2" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()
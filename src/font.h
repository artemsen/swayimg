// SPDX-License-Identifier: MIT
// Font renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Configuration section
#define FONT_CONFIG_SECTION "font"

/** Text surface: array of alpha pixels. */
struct text_surface {
    size_t width;  ///< Width (px)
    size_t height; ///< Height (px)
    uint8_t* data; ///< Pixel data
};

/**
 * Create font renderer.
 */
void font_create(void);

/**
 * Initialize (load font).
 */
void font_init(void);

/**
 * Free font resources.
 */
void font_destroy(void);

/**
 * Render single text line.
 * @param text string to print
 * @param surface text surface to reallocate
 * @return true if operation completed successfully
 */
bool font_render(const char* text, struct text_surface* surface);

// SPDX-License-Identifier: MIT
// Text renderer.
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
 * Create text renderer.
 */
void font_create(void);

/**
 * Initialize (load font).
 */
void font_init(void);

/**
 * Free font resources.
 */
void font_free(void);

/**
 * Set font scaling in HiDPI mode.
 * @param scale scale factor
 */
void font_set_scale(size_t scale);

/**
 * Render single text line.
 * @param text string to print
 * @param surface array of alpha pixels
 * @return false on error
 */
bool font_render(const char* text, struct text_surface* surface);

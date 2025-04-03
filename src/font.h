// SPDX-License-Identifier: MIT
// Font renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"

/** Text surface: array of alpha pixels. */
struct text_surface {
    size_t width;  ///< Width (px)
    size_t height; ///< Height (px)
    uint8_t* data; ///< Pixel data
};

/**
 * Initialize global font context.
 * @param cfg config instance
 */
void font_init(const struct config* cfg);

/**
 * Set font scale based on Wayland scale
 * @param scale the scale to multiply by
 */
void font_set_scale(double scale);

/**
 * Destroy global font context.
 */
void font_destroy(void);

/**
 * Render single text line.
 * @param text string to print
 * @param surface text surface to reallocate
 * @return true if operation completed successfully
 */
bool font_render(const char* text, struct text_surface* surface);

/**
 * Print surface line on the window.
 * @param wnd destination window
 * @param x,y text position
 * @param text text surface to draw
 */
void font_print(struct pixmap* wnd, ssize_t x, ssize_t y,
                const struct text_surface* text);

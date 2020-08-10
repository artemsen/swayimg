// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <cairo/cairo.h>

/**
 * Redraw handler.
 * @param[in] window surface to draw on
 */
typedef void (*on_redraw)(cairo_surface_t* window);

/**
 * Key press handler.
 * @param[in] key code of key pressed
 * @return true if state was changed and window must be redrawn
 */
typedef bool (*on_keyboard)(uint32_t key);

/** Window properties */
struct window {
    on_redraw redraw;
    on_keyboard keyboard;
    size_t width;
    size_t height;
    const char* app_id;
    const char* title;
};

/**
 * Create window and run event handler loop.
 * @param[in] wnd window properties
 * @return true if operation completed successfully
 */
bool show_window(const struct window* wnd);

/**
 * Close window.
 */
void close_window(void);

// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

/** Window buffer. */
struct window {
    const size_t width;  ///< Window width in pixels
    const size_t height; ///< Window height in pixels
    uint32_t* data;      ///< Pointer to pixel data (ARGB)
};

/** UI event handlers. */
struct handlers {
    /**
     * Redraw handler.
     * @param[in] wnd target window description
     */
    void (*on_redraw)(struct window* wnd);

    /**
     * Window resize handler.
     * @param[in] window surface to draw on
     */
    void (*on_resize)(void);

    /**
     * Key press handler.
     * @param[in] key code of key pressed
     * @return true if state was changed and window must be redrawn
     */
    bool (*on_keyboard)(xkb_keysym_t key);
};

/**
 * Create window.
 * @param[in] handlers event handlers
 * @param[in] width window width
 * @param[in] height window height
 * @param[in] app_id application id
 * @return true if operation completed successfully
 */
bool create_window(const struct handlers* handlers, size_t width, size_t height,
                   const char* app_id);

/**
 * Show window and run event handler loop.
 */
void show_window(void);

/**
 * Destroy window.
 */
void destroy_window(void);

/**
 * Close window.
 */
void close_window(void);

/**
 * Get window width.
 * @return window width
 */
size_t get_window_width(void);

/**
 * Get window height.
 * @return window height
 */
size_t get_window_height(void);

/**
 * Set window title.
 * @param[in] title new window title
 */
void set_window_title(const char* title);

/**
 * Enable or disable full screen mode.
 * @param[in] enable mode
 */
void enable_fullscreen(bool enable);

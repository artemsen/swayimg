// SPDX-License-Identifier: MIT
// Wayland window.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

#include <xkbcommon/xkbcommon.h>

/** UI event handlers. */
struct wnd_handlers {
    /**
     * Redraw handler.
     * @param data pointer to pixel data
     */
    void (*on_redraw)(argb_t* data);

    /**
     * Window resize handler.
     * @param width,height window size in pixels
     */
    void (*on_resize)(size_t width, size_t height);

    /**
     * Key press handler.
     * @param key code of key pressed
     * @return true if state was changed and window must be redrawn
     */
    bool (*on_keyboard)(xkb_keysym_t key);
};

/**
 * Create window.
 * @param handlers event handlers
 * @param width window width
 * @param height window height
 * @param app_id application id
 * @return true if operation completed successfully
 */
bool create_window(const struct wnd_handlers* handlers, size_t width,
                   size_t height, const char* app_id);

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
 * Set window title.
 * @param file name of the displayed file for window title
 */
void set_window_title(const char* file);

/**
 * Enable or disable full screen mode.
 * @param enable mode
 */
void enable_fullscreen(bool enable);

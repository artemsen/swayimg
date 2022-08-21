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
     * @param data callback data pointer
     * @param window pointer to pixel data of the window
     */
    void (*on_redraw)(void* data, argb_t* window);

    /**
     * Window resize handler.
     * @param data callback data pointer
     * @param width,height window size in pixels
     */
    void (*on_resize)(void* data, size_t width, size_t height);

    /**
     * Key press handler.
     * @param data callback data pointer
     * @param key code of key pressed
     * @return true if state has changed and window should be redrawn
     */
    bool (*on_keyboard)(void* data, xkb_keysym_t key);

    /**
     * Timer event handler.
     * @param data callback data pointer
     */
    void (*on_timer)(void* data);

    /**
     * Pointer used for callback data.
     */
    void* data;
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

/**
 * Add time callback.
 * @param ms timeout in milliseconds
 */
void add_callback(size_t ms);

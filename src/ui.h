// SPDX-License-Identifier: MIT
// User interface: Window managment, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

#include <xkbcommon/xkbcommon.h>

/** UI context */
struct ui;

/** Available timers. */
enum ui_timer { ui_timer_animation, ui_timer_slideshow };

/** UI event handlers. */
struct ui_handlers {
    /**
     * Redraw handler.
     * @param data callback data pointer
     * @param window pointer to window's pixel data
     */
    void (*on_redraw)(void* data, argb_t* window);

    /**
     * Window resize handler.
     * @param data callback data pointer
     * @param width,height new window size
     * @param scale window scale factor
     */
    void (*on_resize)(void* data, size_t width, size_t height, size_t scale);

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
     * @param timer timer type
     */
    void (*on_timer)(void* data, enum ui_timer timer);

    /**
     * Pointer used for callback data.
     */
    void* data;
};

/**
 * Create User Interface context.
 * @param handlers event handlers
 * @return false on errors
 */
bool ui_create(const struct ui_handlers* handlers);

/**
 * Free UI context.
 */
void ui_free(void);

/**
 * Run event handler loop.
 * @return true if window was closed by user (not an error)
 */
bool ui_run(void);

/**
 * Stop event handler loop.
 */
void ui_stop(void);

/**
 * Set window title.
 * @param title window title to set
 */
void ui_set_title(const char* title);

/**
 * Enable or disable full screen mode.
 * @param enable fullscreen mode
 */
void ui_set_fullscreen(bool enable);

/**
 * Set timer callback.
 * @param timer type of the timer to set
 * @param ms timeout in milliseconds
 */
void ui_set_timer(enum ui_timer timer, size_t ms);

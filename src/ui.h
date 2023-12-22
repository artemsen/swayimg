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
     * @param ctx UI context
     * @param width,height new window size
     * @param scale window scale factor
     */
    void (*on_resize)(void* data, struct ui* ctx, size_t width, size_t height,
                      size_t scale);

    /**
     * Key press handler.
     * @param data callback data pointer
     * @param ctx UI context
     * @param key code of key pressed
     * @return true if state has changed and window should be redrawn
     */
    bool (*on_keyboard)(void* data, struct ui* ctx, xkb_keysym_t key);

    /**
     * Timer event handler.
     * @param data callback data pointer
     * @param ctx UI context
     * @param timer timer type
     */
    void (*on_timer)(void* data, enum ui_timer timer, struct ui* ctx);

    /**
     * Pointer used for callback data.
     */
    void* data;
};

/**
 * Create User Interface context.
 * @param handlers event handlers
 * @return ui context or NULL on errors
 */
struct ui* ui_create(const struct ui_handlers* handlers);

/**
 * Free UI context.
 * @param ctx UI context
 */
void ui_free(struct ui* ctx);

/**
 * Run event handler loop.
 * @param ctx UI context
 * @return true if window was closed by user (not an error)
 */
bool ui_run(struct ui* ctx);

/**
 * Stop event handler loop.
 * @param ctx UI context
 */
void ui_stop(struct ui* ctx);

/**
 * Set window title.
 * @param ctx UI context
 * @param title window title to set
 */
void ui_set_title(struct ui* ctx, const char* title);

/**
 * Enable or disable full screen mode.
 * @param ctx UI context
 * @param enable fullscreen mode
 */
void ui_set_fullscreen(struct ui* ctx, bool enable);

/**
 * Set timer callback.
 * @param ctx UI context
 * @param timer type of the timer to set
 * @param ms timeout in milliseconds
 */
void ui_set_timer(struct ui* ctx, enum ui_timer timer, size_t ms);

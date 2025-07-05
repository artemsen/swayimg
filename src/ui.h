// SPDX-License-Identifier: MIT
// User interface: Window management, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "image.h"

// Window size
#define UI_WINDOW_DEFAULT_WIDTH  1280
#define UI_WINDOW_DEFAULT_HEIGHT 720
#define UI_WINDOW_FULLSCREEN     0
#define UI_WINDOW_MIN            10
#define UI_WINDOW_MAX            20000

/** Cursor shapes. */
enum ui_cursor {
    ui_cursor_default,
    ui_cursor_drag,
    ui_cursor_hide,
};

/** Content types. */
enum ui_ctype {
    ui_ctype_image,
    ui_ctype_animation,
};

/**
 * Initialize global UI context: create window, register handlers etc.
 * @param cfg config instance
 * @param img first image instance
 * @return true if UI initialized
 */
bool ui_init(const struct config* cfg, const struct image* img);

/**
 * Destroy global UI context.
 */
void ui_destroy(void);

/**
 * Prepare the window system to read events.
 */
void ui_event_prepare(void);

/**
 * Notify window system that events were read.
 */
void ui_event_done(void);

/**
 * Begin window redraw procedure.
 * @return window pixmap
 */
struct pixmap* ui_draw_begin(void);

/**
 * Finish window redraw procedure.
 */
void ui_draw_commit(void);

/**
 * Set window title.
 * @param name file name of the current image
 */
void ui_set_title(const char* name);

/**
 * Set mouse pointer shape.
 * @param shape cursor shape to set
 */
void ui_set_cursor(enum ui_cursor shape);

/**
 * Set surface content type.
 * @param ctype content type to set
 */
void ui_set_ctype(enum ui_ctype ctype);

/**
 * Get window width.
 * @return window width in pixels
 */
size_t ui_get_width(void);

/**
 * Get window height.
 * @return window height in pixels
 */
size_t ui_get_height(void);

/**
 * Toggle full screen mode.
 */
void ui_toggle_fullscreen(void);

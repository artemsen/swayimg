// SPDX-License-Identifier: MIT
// User interface: Window management, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.h"

#include <limits.h>

// Configuration parameters
#define UI_CFG_FULLSCREEN "fullscreen"
#define UI_CFG_SIZE       "size"
#define UI_CFG_POSITION   "position"

// Special ids for windows size and position
#define SIZE_FROM_IMAGE  0
#define SIZE_FROM_PARENT 1
#define POS_FROM_PARENT  SSIZE_MAX

/**
 * Create global UI context.
 */
void ui_create(void);

/**
 * Initialize global UI context: create window, register handlers etc.
 * @param app_id application id, used as window class
 * @return true if window created
 */
bool ui_init(const char* app_id);

/**
 * Destroy global UI context.
 */
void ui_destroy(void);

/**
 * Prepare the window system to read events.
 */
void ui_event_prepare(void);

/**
 * Event handler complete notification.
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
 * Set window position.
 * @param x,y new window coordinates
 */
void ui_set_position(ssize_t x, ssize_t y);

/**
 * Get window x position.
 * @return window x position
 */
ssize_t ui_get_x(void);

/**
 * Get window y position.
 * @return window y position
 */
ssize_t ui_get_y(void);

/**
 * Set window size.
 * @param width,height window size in pixels
 */
void ui_set_size(size_t width, size_t height);

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
 * Get window scale factor.
 * @return window scale factor
 */
size_t ui_get_scale(void);

/**
 * Toggle full screen mode.
 */
void ui_toggle_fullscreen(void);

/**
 * Check if full screen mode is active.
 * @return current mode
 */
bool ui_get_fullscreen(void);

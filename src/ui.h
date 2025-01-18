// SPDX-License-Identifier: MIT
// User interface: Window management, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.h"

/**
 * Create global UI context.
 */
void ui_create(void);

/**
 * Initialize global UI context: create window, register handlers etc.
 * @param app_id application id, used as window class
 * @param width,height initial window size in pixels
 * @param decor flag to use server-side window decoration
 * @return true if window created
 */
bool ui_init(const char* app_id, size_t width, size_t height, bool decor);

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
 * Change surface content type to photo or animation
 */
void ui_set_content_type_animated(bool animated);

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

// SPDX-License-Identifier: MIT
// User interface: Window managment, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

/** Available timers. */
enum ui_timer { ui_timer_animation, ui_timer_slideshow };

/**
 * Create User Interface context.
 * @return false on errors
 */
bool ui_init(void);

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

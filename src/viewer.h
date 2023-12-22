// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

#include <xkbcommon/xkbcommon.h>

/**
 * Initialize viewer context.
 */
void viewer_init(void);

/**
 * Free viewer context.
 */
void viewer_free(void);

/**
 * Redraw handler.
 * @param window pointer to window's pixel data
 */
void viewer_on_redraw(argb_t* window);

/**
 * Window resize handler.
 * @param width,height new window size
 * @param scale window scale factor
 */
void viewer_on_resize(size_t width, size_t height, size_t scale);

/**
 * Key press handler.
 * @param key code of key pressed
 * @return true if state has changed and window should be redrawn
 */
bool viewer_on_keyboard(xkb_keysym_t key);

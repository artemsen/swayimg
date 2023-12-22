// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "imagelist.h"
#include "ui.h"

/** Viewer context. */
struct viewer;

/**
 * Create User Interface context.
 * @return viewer context or NULL on errors
 */
struct viewer* viewer_create(void);

/**
 * Free viewer context.
 * @param ctx viewer context
 */
void viewer_free(struct viewer* ctx);

// UI callbacks, see ui_handlers for details
void viewer_on_redraw(void* data, argb_t* window);
void viewer_on_resize(void* data, size_t width, size_t height, size_t scale);
bool viewer_on_keyboard(void* data, xkb_keysym_t key);
void viewer_on_timer(void* data, enum ui_timer timer);

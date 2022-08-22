// SPDX-License-Identifier: MIT
// Business logic of application.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "imagelist.h"
#include "ui.h"

/** Viewer context. */
struct viewer;

/**
 * Create User Interface context.
 * @param cfg configuration instance
 * @param list list of images to view
 * @return viewer context or NULL on errors
 */
struct viewer* viewer_create(struct config* cfg, struct image_list* list);

/**
 * Free viewer context.
 * @param ctx viewer context
 */
void viewer_free(struct viewer* ctx);

// UI callbacks, see ui_handlers for details
void viewer_on_redraw(void* data, argb_t* window);
void viewer_on_resize(void* data, struct ui* ui, size_t width, size_t height,
                      size_t scale);
bool viewer_on_keyboard(void* data, struct ui* ui, xkb_keysym_t key);
void viewer_on_timer(void* data, struct ui* ui);

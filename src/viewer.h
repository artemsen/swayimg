// SPDX-License-Identifier: MIT
// Business logic of application and UI event handlers.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "event.h"
#include "image.h"

/**
 * Initialize global viewer context.
 * @param cfg config instance
 * @param image initial image to open
 */
void viewer_init(const struct config* cfg, struct image* image);

/**
 * Destroy global viewer context.
 */
void viewer_destroy(void);

/**
 * Event handler, see `event_handler` for details.
 */
void viewer_handle(const struct event* event);

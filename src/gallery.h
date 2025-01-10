// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "event.h"

/**
 * Initialize global gallery context.
 * @param cfg config instance
 * @param image initial image to open
 */
void gallery_init(const struct config* cfg, struct image* image);

/**
 * Destroy global gallery context.
 */
void gallery_destroy(void);

/**
 * Event handler, see `event_handler` for details.
 */
void gallery_handle(const struct event* event);

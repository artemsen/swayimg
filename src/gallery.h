// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "event.h"

/**
 * Create global gallery context.
 */
void gallery_create(void);

/**
 * Initialize global gallery context.
 */
void gallery_init(void);

/**
 * Destroy global gallery context.
 */
void gallery_destroy(void);

/**
 * Event handler, see `event_handler` for details.
 */
void gallery_handle(const struct event* event);

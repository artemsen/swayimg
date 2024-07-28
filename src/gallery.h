// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "event.h"
#include "image.h"

/**
 * Create global gallery context.
 */
void gallery_create(void);

/**
 * Initialize global gallery context.
 * @param image initial image to open
 * @param index index of the image in the image list
 */
void gallery_init(struct image* image, size_t index);

/**
 * Destroy global gallery context.
 */
void gallery_destroy(void);

/**
 * Event handler, see `event_handler` for details.
 */
void gallery_handle(const struct event* event);

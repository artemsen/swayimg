// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "text.h"

/**
 * Create global info context.
 */
void info_create(void);

/**
 * Initialize global info context.
 */
void info_init(void);

/**
 * Destroy global info context.
 */
void info_destroy(void);

/**
 * Set the display mode.
 * @param mode display mode name
 */
void info_set_mode(const char* mode);

/**
 * Refresh info data.
 * @param frame_idx index of the current frame
 * @param scale current scale factor
 */
void info_update(size_t frame_idx, float scale);

/**
 * Set status text.
 * @param fmt message format description
 */
void info_set_status(const char* fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * Get number of lines in the specified block.
 * @param pos block position
 * @return number of lines
 */
size_t info_height(enum text_position pos);

/**
 * Get list of key/value lines for specified text block.
 * @param pos block position
 * @return pointer to the lines array
 */
const struct text_keyval* info_lines(enum text_position pos);

/**
 * Get info display timeout.
 * @return 0 if timeout disabled, positive number for absolute time in second,
 * or negative number for slideshow relative percents.
 */
int info_timeout(void);

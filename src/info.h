// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "font.h"

/** Info block position. */
enum info_position {
    info_top_left,
    info_top_right,
    info_bottom_left,
    info_bottom_right,
};
#define INFO_POSITION_NUM 4

/** Info line. */
struct info_line {
    struct text_surface key;
    struct text_surface value;
};

/**
 * Create info context.
 */
void info_create(void);

/**
 * Initialize info context.
 */
void info_init(void);

/**
 * Free info context.
 */
void info_free(void);

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
size_t info_height(enum info_position pos);

/**
 * Get list of text lines (key->val).
 * @param pos block position
 * @return pointer to the lines array
 */
const struct info_line* info_lines(enum info_position pos);

// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"
#include "text.h"

/** Available info fields. */
enum info_field {
    info_file_name,
    info_file_path,
    info_file_size,
    info_image_format,
    info_image_size,
    info_exif,
    info_frame,
    info_index,
    info_scale,
    info_status,
};

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
 * Compose info data from image.
 * @param image image instance
 */
void info_reset(const struct image* image);

/**
 * Update info text.
 * @param field info field id
 * @param fmt text string to set, NULL to clear
 */
void info_update(enum info_field field, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

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

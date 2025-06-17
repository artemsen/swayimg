// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"
#include "keybind.h"

/** Available info fields. */
enum info_field {
    info_file_name,
    info_file_dir,
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
 * Initialize global info context.
 * @param cfg config instance
 */
void info_init(const struct config* cfg);

/**
 * Reinitialize info (change scale).
 */
void info_reinit(void);

/**
 * Destroy global info context.
 */
void info_destroy(void);

/**
 * Switch display mode.
 * @param mode display mode name
 */
void info_switch(const char* mode);

/**
 * Check if info enabled.
 * @param true if info text layer is enabled
 */
bool info_enabled(void);

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
 * Update index and position in the image list.
 * @param field info field id
 * @param fmt text string to set, NULL to clear
 */
void info_update_index(size_t current, size_t total);

/**
 * Print info text.
 * @param window target window surface
 */
void info_print(struct pixmap* window);

/**
 * Show help layer.
 * @param kb head of keybinding list
 */
void help_show(const struct keybind* kb);

/**
 * Hide help layer.
 */
void help_hide(void);

/**
 * Check if help layer is displayed.
 * @return true if help layer is displayed
 */
bool help_active(void);

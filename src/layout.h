// SPDX-License-Identifier: MIT
// Thumbnail layout for gallery mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/** Thumbnail instance with coordinates. */
struct layout_thumb {
    struct image* img;
    size_t x;
    size_t y;
};

/** Thumbnail layout scheme. */
struct layout {
    size_t columns; ///< Number of thumbnails horizontally
    size_t rows;    ///< Number of thumbnails vertically
    size_t padding; ///< Padding between thumbnails

    struct image* current; ///< Currently selected image
    size_t current_col;    ///< Currently selected column
    size_t current_row;    ///< Currently selected row

    size_t thumb_size;           ///< Size of thumbnail (in pixels)
    size_t thumb_total;          ///< Total number of showed thumbnails
    struct layout_thumb* thumbs; ///< Visible thumbnails array
};

/** Movement direction. */
enum layout_dir {
    layout_up,
    layout_down,
    layout_left,
    layout_right,
    layout_first,
    layout_last,
    layout_pgup,
    layout_pgdown,
};

/**
 * Create new layout.
 * @param lo pointer to the thumbnail layout for initialization
 * @param thumb_size thumbnail size in pixels
 */
void layout_init(struct layout* lo, size_t thumb_size);

/**
 * Free layout resources.
 * @param lo pointer to the thumbnail layout
 */
void layout_free(struct layout* lo);

/**
 * Update layout: recalculate thumbnails scheme.
 * @param lo pointer to the thumbnail layout
 */
void layout_update(struct layout* lo);

/**
 * Resize layout.
 * @param lo pointer to the thumbnail layout
 * @param width,height size of the output window
 */
void layout_resize(struct layout* lo, size_t width, size_t height);

/**
 * Move selection to the next image.
 * @param lo pointer to the thumbnail layout
 * @param dir movement direction
 * @return true if new image was selected
 */
bool layout_select(struct layout* lo, enum layout_dir dir);

/**
 * Get currently selected thumbnail from layout.
 * @param lo pointer to the thumbnail layout
 * @return pointer to the currently selected thumbnail
 */
struct layout_thumb* layout_current(struct layout* lo);

/**
 * Create loading queue: ordered list of images to load.
 * @param lo pointer to the thumbnail layout
 * @return pointer to the list head, caller should free the list
 */
struct image* layout_ldqueue(struct layout* lo);

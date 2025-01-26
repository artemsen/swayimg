// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"

// Invalid index of the entry
#define IMGLIST_INVALID SIZE_MAX

/** Order of file list. */
enum list_order {
    order_none,    ///< Unsorted (system depended)
    order_alpha,   ///< Alphanumeric sort
    order_reverse, ///< Reversed alphanumeric sort
    order_random   ///< Random order
};

/**
 * Initialize global image list context.
 * @param cfg config instance
 */
void image_list_init(const struct config* cfg);

/**
 * Destroy global image list context.
 */
void image_list_destroy(void);

/**
 * Add image source to the list.
 * @param source image source to add (file path or special prefix)
 */
void image_list_add(const char* source);

/**
 * Reorder image list (sort/random/...).
 */
void image_list_reorder(void);

/**
 * Get image list size.
 * @return total number of entries in the list include non-image files
 */
size_t image_list_size(void);

/**
 * Get image source for specified index.
 * @param index index of the image list entry
 * @return image data source description (path, ...) or NULL if no source
 */
const char* image_list_get(size_t index);

/**
 * Find index for specified source.
 * @param source image data source
 * @return index of the entry or IMGLIST_INVALID if not found
 */
size_t image_list_find(const char* source);

/**
 * Get index of nearest entry in the list.
 * @param start start position
 * @param forward direction(forward/backward)
 * @param loop enable/disable loop mode
 * @return index of the entry or IMGLIST_INVALID
 */
size_t image_list_nearest(size_t start, bool forward, bool loop);

/**
 * Get distance between two indexes.
 * @param start,end entry indexes
 * @return number of image entries between indexes
 */
size_t image_list_distance(size_t start, size_t end);

/**
 * Get index of the entry in specified distance from start.
 * @param start start position
 * @param distance number entries to skip
 * @param forward direction(forward/backward)
 * @return index of the entry or IMGLIST_INVALID
 */
size_t image_list_jump(size_t start, size_t distance, bool forward);

/**
 * Get next entry index.
 * @param start index of the start position
 * @return index of the entry or IMGLIST_INVALID if not found
 */
size_t image_list_next_file(size_t start);

/**
 * Get previous entry index.
 * @param start index of the start position
 * @return index of the entry or IMGLIST_INVALID if not found
 */
size_t image_list_prev_file(size_t start);

/**
 * Get random entry index.
 * @param exclude index of the excluded position (currently viewed image)
 * @return index of the entry or IMGLIST_INVALID if not found
 */
size_t image_list_rand_file(size_t exclude);

/**
 * Get next directory entry index (works only for paths as source).
 * @param start index of the start position
 * @return index of the entry or IMGLIST_INVALID if not found
 */
size_t image_list_next_dir(size_t start);

/**
 * Get previous directory entry index (works only for paths as source).
 * @param start index of the start position
 * @return index of the entry or IMGLIST_INVALID if not found
 */
size_t image_list_prev_dir(size_t start);

/**
 * Get the first entry index.
 * @return index of the entry or IMGLIST_INVALID if image list is empty
 */
size_t image_list_first(void);

/**
 * Get the first entry index.
 * @return index of the entry or IMGLIST_INVALID if image list is empty
 */
size_t image_list_last(void);

/**
 * Skip entry (remove from the image list).
 * @param index entry to remove
 * @return next valid index of the entry or IMGLIST_INVALID if list is empty
 */
size_t image_list_skip(size_t index);

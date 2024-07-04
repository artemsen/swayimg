// SPDX-License-Identifier: MIT
// List of images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

// Configuration parameters
#define IMGLIST_CFG_SECTION   "list"
#define IMGLIST_CFG_ORDER     "order"
#define IMGLIST_CFG_LOOP      "loop"
#define IMGLIST_CFG_RECURSIVE "recursive"
#define IMGLIST_CFG_ALL       "all"
#define IMGLIST_CFG_CACHE     "chahe"
#define IMGLIST_CFG_PRELOAD   "preload"

// Invalid index of the entry
#define IMGLIST_INVALID SIZE_MAX

/** Order of file list. */
enum list_order {
    order_none,  ///< Unsorted (system depended)
    order_alpha, ///< Alphanumeric sort
    order_random ///< Random order
};

/**
 * Create the image list.
 */
void image_list_create(void);

/**
 * Free the image list.
 */
void image_list_free(void);

/**
 * Initialize the image list: scan directories and fill the image list.
 * @param sources list of sources
 * @param num number of sources in the list
 * @return size of the image list
 */
size_t image_list_init(const char** sources, size_t num);

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

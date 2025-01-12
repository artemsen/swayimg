// SPDX-License-Identifier: MIT
// Create/load/store thumbnails.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "image.h"
#include "pixmap_scale.h"

/** List of thumbnails. */
struct thumbnail {
    struct list list;     ///< Links to prev/next entry
    struct image* image;  ///< Thumbnail image
    size_t width, height; ///< Real image size
};

/**
 * Initialize global gallery context.
 * @param cfg config instance
 */
void thumbnail_init(const struct config* cfg);

/**
 * Destroy/reset global thumbnail context.
 */
void thumbnail_free(void);

/**
 * Get current anti-aliasing mode for thumbnails.
 * @return current mode
 */
enum pixmap_aa_mode thumbnail_get_aa(void);

/**
 * Switch anti-aliasing mode.
 * @return new mode
 */
enum pixmap_aa_mode thumbnail_switch_aa(void);

/**
 * Create new thumbnail from the image.
 * @param image original image, this instance will be replaced by thumbnail
 */
void thumbnail_add(struct image* image);

/**
 * Get thumbnail.
 * @param index image position in the image list
 * @return thumbnail instance or NULL if not found
 */
const struct thumbnail* thumbnail_get(size_t index);

/**
 * Remove thumbnail from the cache.
 * @param index image position in the image list
 */
void thumbnail_remove(size_t index);

/**
 * Clear memory cache.
 * @param min_id,max_id range of ids to save in cache
 */
void thumbnail_clear(size_t min_id, size_t max_id);

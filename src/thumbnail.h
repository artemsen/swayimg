// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>
// Copyright (C) 2024 Rentib <sbitner420@tutanota.com>

#include "image.h"
#include "pixmap.h"

struct thumbnail_params {
    size_t thumb_width, thumb_height;
    ssize_t offset_x, offset_y;
    bool fill;
    bool antialias;
    float scale;
};

/**
 * Sets up thumbnail parameters.
 * @param params thumbnail parameters
 * @param image original image
 * @param size thumbnail size in pixels
 * @param fill thumbnail scale mode (fill/fit)
 * @param antialias use antialiasing
 */
void thumbnail_params(struct thumbnail_params* params,
                      const struct image* image, size_t size, bool fill,
                      bool antialias);

/**
 * Create thumbnail from full size image.
 * @param thumbnail pixmap to store thumbnail
 * @param image original image
 * @param params thumbnail parameters
 * @return true if successful
 */
bool thumbnail_create(struct pixmap* thumbnail, const struct image* image,
                      const struct thumbnail_params* params);

/**
 * Load thumbnail from disk.
 * @param thumbnail pixmap to store thumbnail
 * @param source source of image
 * @param params thumbnail parameters
 * @return true if successful
 */
bool thumbnail_load(struct pixmap* thumbnail, const char* source,
                    const struct thumbnail_params* params);

/**
 * Save thumbnail on disk.
 * @param thumbnail pixmap with thumbnail
 * @param source source of image
 * @param params thumbnail parameters
 * @return true if successful
 */
bool thumbnail_save(const struct pixmap* thumbnail, const char* source,
                    const struct thumbnail_params* params);

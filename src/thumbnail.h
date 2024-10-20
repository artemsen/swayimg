// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>
// Copyright (C) 2024 Rentib <sbitner420@tutanota.com>

#include "image.h"
#include "pixmap.h"

/**
 * Create thumbnail from full size image.
 * @param thumbnail pixmap to store thumbnail
 * @param image original image
 * @param size thumbnail size in pixels
 * @param fill thumbnail scale mode (fill/fit)
 * @param antialias use antialiasing
 */
bool thumbnail_create(struct pixmap* thumbnail, const struct image* image,
                      size_t size, bool fill, bool antialias);

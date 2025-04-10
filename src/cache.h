// SPDX-License-Identifier: MIT
// Image cache to limit the number of simultaneously loaded images.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/** Cache queue. */
struct cache;

/**
 * Initialize cache queue.
 * @param capacity max size of the queue
 * @return cache context
 */
struct cache* cache_init(size_t capacity);

/**
 * Get cache capacity.
 * @return max size of the queue
 */
size_t cache_capacity(const struct cache* cache);

/**
 * Free cache queue, unload all images from the cache.
 * @param cache context
 */
void cache_free(struct cache* cache);

/**
 * Trim cache.
 * @param cache context
 * @param size number of entries to preserve
 */
void cache_trim(struct cache* cache, size_t size);

/**
 * Put image to the cache.
 * @param cache context
 * @param image pointer to image instance
 * @return false if image can not be put into cache
 */
bool cache_put(struct cache* cache, struct image* image);

/**
 * Remove image from the cache.
 * @param cache context
 * @param image image descriptor
 * @return false if image is not in the cache
 */
bool cache_out(struct cache* cache, const struct image* image);

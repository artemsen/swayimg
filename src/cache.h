// SPDX-License-Identifier: MIT
// Image cache.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

#include <pthread.h>

/** Cached image. */
struct cache_image {
    struct image* image; ///< Image handle
    size_t index;        ///< Index of the image in the image list
};

/** Cache queue. */
struct cache_queue {
    size_t capacity;           ///< Max length of the queue
    pthread_mutex_t lock;      ///< Cache access lock
    struct cache_image* queue; ///< Cache queue
};

/**
 * Initialize cache queue.
 * @param cache context
 * @param capacity max size of the queue
 */
void cache_init(struct cache_queue* cache, size_t capacity);

/**
 * Free cache queue.
 * @param cache context
 */
void cache_free(struct cache_queue* cache);

/**
 * Reset (clear) cache queue.
 * @param cache context
 */
void cache_reset(struct cache_queue* cache);

/**
 * Check if queue is full.
 * @param cache context
 * @return true if cache queue is full
 */
bool cache_full(const struct cache_queue* cache);

/**
 * Put image handle to cache queue.
 * @param cache context
 * @param image pointer to image instance
 * @param index index of the image in the image list
 */
void cache_put(struct cache_queue* cache, struct image* image, size_t index);

/**
 * Take out image handle from cache queue.
 * @param cache context
 * @param index index of the image in the image list
 * @return image instance or NULL if image not in cache
 */
struct image* cache_get(struct cache_queue* cache, size_t index);

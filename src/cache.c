// SPDX-License-Identifier: MIT
// Image cache.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "cache.h"

#include <stdlib.h>

void cache_init(struct cache_queue* cache, size_t capacity)
{
    cache->capacity = capacity;
    cache->queue = NULL;

    pthread_mutex_init(&cache->lock, NULL);

    if (capacity) {
        cache->queue = calloc(1, capacity * sizeof(*cache->queue));
        if (!cache->queue) {
            cache->capacity = 0;
        }
    }
}

void cache_free(struct cache_queue* cache)
{
    cache_reset(cache);
    free(cache->queue);
    pthread_mutex_destroy(&cache->lock);
}

void cache_reset(struct cache_queue* cache)
{
    for (size_t i = 0; i < cache->capacity; ++i) {
        struct cache_image* entry = &cache->queue[i];
        if (entry->image) {
            image_free(entry->image);
            entry->image = NULL;
        }
    }
}

bool cache_full(const struct cache_queue* cache)
{
    for (size_t i = 0; i < cache->capacity; ++i) {
        if (!cache->queue[i].image) {
            return false;
        }
    }
    return true;
}

void cache_put(struct cache_queue* cache, struct image* image, size_t index)
{
    struct cache_image* entry;

    if (cache->capacity == 0 || !image) {
        return;
    }

    pthread_mutex_lock(&cache->lock);

    // check for empty entry
    for (size_t i = 0; i < cache->capacity; ++i) {
        entry = &cache->queue[i];
        if (!entry->image) {
            entry->image = image;
            entry->index = index;
            pthread_mutex_unlock(&cache->lock);
            return;
        }
    }

    // remove oldest entry from head
    image_free(cache->queue[0].image);
    for (size_t i = 0; i < cache->capacity - 1; ++i) {
        cache->queue[i] = cache->queue[i + 1];
    }

    // add new entry to tail
    entry = &cache->queue[cache->capacity - 1];
    entry->image = image;
    entry->index = index;

    pthread_mutex_unlock(&cache->lock);
}

struct image* cache_get(struct cache_queue* cache, size_t index)
{
    struct image* img = NULL;

    pthread_mutex_lock(&cache->lock);

    for (size_t i = 0; i < cache->capacity; ++i) {
        const struct cache_image* entry = &cache->queue[i];

        if (!entry->image) {
            break; // last entry
        }

        if (entry->index == index) {
            img = entry->image;

            // move remaining entries
            for (; i < cache->capacity - 1; ++i) {
                cache->queue[i] = cache->queue[i + 1];
            }
            // remove last entry from queue
            cache->queue[cache->capacity - 1].image = NULL;

            break;
        }
    }

    pthread_mutex_unlock(&cache->lock);

    return img;
}

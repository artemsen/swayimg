// SPDX-License-Identifier: MIT
// Image cache to limit the number of simultaneously loaded images.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "cache.h"

#include <assert.h>
#include <stdlib.h>

/** Cache entry. */
struct cache_entry {
    struct list list;    ///< Links to prev/next entry
    struct image* image; ///< Image instance
};

/** Cache queue. */
struct cache {
    struct cache_entry* queue; ///< Cache queue
    size_t capacity;           ///< Max length of the queue
};

struct cache* cache_init(size_t capacity)
{
    struct cache* cache = NULL;

    if (capacity) {
        cache = calloc(1, sizeof(struct cache));
        if (cache) {
            cache->capacity = capacity;
        }
    }

    return cache;
}

size_t cache_capacity(const struct cache* cache)
{
    return cache ? cache->capacity : 0;
}

void cache_free(struct cache* cache)
{
    if (cache) {
        cache_trim(cache, 0);
        free(cache);
    }
}

void cache_trim(struct cache* cache, size_t size)
{
    if (cache) {
        list_for_each(cache->queue, struct cache_entry, it) {
            if (size) {
                --size;
            } else {
                cache->queue = list_remove(it);
                image_free(it->image, IMGFREE_FRAMES);
                free(it);
            }
        }
    }
}

bool cache_put(struct cache* cache, struct image* image)
{
    struct cache_entry* new_entry;
    struct cache_entry* last_entry;
    size_t size;

    if (!cache) {
        return false;
    }

    new_entry = malloc(sizeof(struct cache_entry));
    if (!new_entry) {
        return false;
    }
    new_entry->image = image;

    // get size and last entry
    size = 0;
    last_entry = NULL;
    list_for_each(cache->queue, struct cache_entry, it) {
        assert(it->image != image);
        if (list_is_last(it)) {
            last_entry = it;
        }
        ++size;
    }

    // restrict queue size
    if (size >= cache->capacity && last_entry) {
        cache->queue = list_remove(last_entry);
        image_free(last_entry->image, IMGFREE_FRAMES);
        free(last_entry);
    }

    cache->queue = list_add(cache->queue, new_entry);

    return true;
}

bool cache_out(struct cache* cache, struct image* image)
{
    bool found = false;

    if (cache) {
        list_for_each(cache->queue, struct cache_entry, it) {
            found = (image == it->image);
            if (found) {
                cache->queue = list_remove(it);
                free(it);
                break;
            }
        }
    }

    return found;
}

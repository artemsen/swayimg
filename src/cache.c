// SPDX-License-Identifier: MIT
// Image cache to limit the number of simultaneously loaded images.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "cache.h"

#include "imglist.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** Cache entry. */
struct cache_entry {
    struct list list; ///< Links to prev/next entry
    char image[1];    ///< Image source (variable length)
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
                struct image* img = imglist_find(it->image);
                if (img) {
                    image_free(img, IMGFREE_FRAMES);
                }
                cache->queue = list_remove(it);
                free(it);
            }
        }
    }
}

bool cache_put(struct cache* cache, struct image* image)
{
    struct cache_entry* entry;
    size_t size;

    if (!cache) {
        return false;
    }

    // create new entry
    size = strlen(image->source);
    entry = malloc(sizeof(struct cache_entry) + size);
    if (!entry) {
        return false;
    }
    memcpy(entry->image, image->source, size + 1 /* last null */);

    // remove last entry if queue size exceeds capacity
    size = 0;
    list_for_each(cache->queue, struct cache_entry, it) {
        assert(strcmp(it->image, image->source));
        if (++size >= cache->capacity) {
            struct image* img = imglist_find(it->image);
            if (img) {
                image_free(img, IMGFREE_FRAMES);
            }
            cache->queue = list_remove(it);
            free(it);
            break;
        }
    }

    cache->queue = list_add(cache->queue, entry);

    return true;
}

bool cache_out(struct cache* cache, const struct image* image)
{
    bool found = false;

    if (cache) {
        list_for_each(cache->queue, struct cache_entry, it) {
            found = strcmp(image->source, it->image) == 0;
            if (found) {
                const struct image* img = imglist_find(it->image);
                found = (img && image_has_frames(img));
                cache->queue = list_remove(it);
                free(it);
                break;
            }
        }
    }

    return found;
}

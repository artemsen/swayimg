// SPDX-License-Identifier: MIT
// Images origin for viewer mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "fetcher.h"

#include "application.h"
#include "buildcfg.h"
#include "imagelist.h"
#include "loader.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

/** Image cache queue. */
struct image_cache {
    size_t capacity;      ///< Max length of the queue
    struct image** queue; ///< Cache queue
};

/** Image fetch context. */
struct fetch {
    struct image* current;      ///< Current image
    struct image_cache history; ///< Least recently viewed images
    struct image_cache preload; ///< Preloaded images
#ifdef HAVE_INOTIFY
    int notify; ///< inotify file handler
    int watch;  ///< Current file watcher
#endif
};

/** Global image fetch context. */
static struct fetch ctx;

/**
 * Initialize cache queue.
 * @param cache context
 * @param capacity max size of the queue
 */
static void cache_init(struct image_cache* cache, size_t capacity)
{
    cache->capacity = capacity;
    cache->queue = NULL;

    if (capacity) {
        cache->queue = calloc(1, capacity * sizeof(*cache->queue));
        if (!cache->queue) {
            cache->capacity = 0;
        }
    }
}

/**
 * Reset (clear) cache queue.
 * @param cache context
 */
static void cache_reset(struct image_cache* cache)
{
    for (size_t i = 0; i < cache->capacity; ++i) {
        struct image* img = cache->queue[i];
        if (img) {
            image_free(img);
            cache->queue[i] = NULL;
        }
    }
}

/**
 * Free cache queue.
 * @param cache context
 */
static void cache_free(struct image_cache* cache)
{
    cache_reset(cache);
    free(cache->queue);
}

/**
 * Put image handle to cache queue.
 * @param cache context
 * @param image pointer to image instance
 */
static void cache_put(struct image_cache* cache, struct image* image)
{
    if (cache->capacity == 0 || !image) {
        return;
    }

    // check for empty entry
    for (size_t i = 0; i < cache->capacity; ++i) {
        if (!cache->queue[i]) {
            cache->queue[i] = image;
            return;
        }
    }

    // cache is full, remove the oldest entry from head
    image_free(cache->queue[0]);
    for (size_t i = 0; i < cache->capacity - 1; ++i) {
        cache->queue[i] = cache->queue[i + 1];
    }
    // add new entry to tail
    cache->queue[cache->capacity - 1] = image;
}

/**
 * Take out image handle from cache queue.
 * @param cache context
 * @param index index of the image in the image list
 * @return image instance or NULL if image not in cache
 */
static struct image* cache_take(struct image_cache* cache, size_t index)
{
    struct image* img = NULL;

    for (size_t i = 0; i < cache->capacity; ++i) {
        struct image* entry = cache->queue[i];
        if (!entry) {
            break; // last entry
        }
        if (entry->index == index) {
            img = entry;

            // move remaining entries
            for (; i < cache->capacity - 1; ++i) {
                cache->queue[i] = cache->queue[i + 1];
            }
            // remove last entry from queue
            cache->queue[cache->capacity - 1] = NULL;

            break;
        }
    }

    return img;
}

/**
 * Remove oldest entries from cache.
 * @param cache context
 * @param size number of entries to preserve
 */
static void cache_trim(struct image_cache* cache, size_t size)
{
    if (size == 0) {
        cache_reset(cache);
    } else {
        for (size_t i = cache->capacity - 1; i > size; ++i) {
            image_free(cache->queue[i]);
            cache->queue[i] = NULL;
        }
    }
}

#ifdef HAVE_INOTIFY
/** inotify handler. */
static void on_inotify(__attribute__((unused)) void* data)
{
    while (true) {
        bool updated = false;
        uint8_t buffer[1024];
        ssize_t pos = 0;
        const ssize_t len = read(ctx.notify, buffer, sizeof(buffer));

        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // something went wrong
        }

        while (pos + sizeof(struct inotify_event) <= (size_t)len) {
            const struct inotify_event* event =
                (struct inotify_event*)&buffer[pos];
            if (event->mask & IN_IGNORED) {
                ctx.watch = -1;
            } else {
                updated = true;
            }
            pos += sizeof(struct inotify_event) + event->len;
        }
        if (updated) {
            app_reload();
        }
    }
}
#endif // HAVE_INOTIFY

/** Reset preloader queue. */
static void reset_preloader(void)
{
    size_t* preload;
    size_t preload_num = 0;
    size_t found = 0;
    size_t next;

    if (ctx.preload.capacity == 0) {
        return;
    }

    loader_queue_reset();

    preload = malloc(ctx.preload.capacity * sizeof(*preload));
    if (!preload) {
        return;
    }

    // reorder preloads and create list of preloads
    next = ctx.current->index;
    for (size_t i = 0; i < ctx.preload.capacity; ++i) {
        struct image* img;

        next = image_list_next_file(next);
        if (next == IMGLIST_INVALID) {
            break;
        }

        img = cache_take(&ctx.preload, next);
        if (img) {
            cache_put(&ctx.preload, img);
            ++found;
        } else {
            preload[preload_num++] = next;
        }
    }

    // remove images that don't fit into cache size
    cache_trim(&ctx.preload, found);

    // add preloads to queue
    for (size_t i = 0; i < preload_num; ++i) {
        loader_queue_append(preload[i]);
    }

    free(preload);
}

/**
 * Set image as the current one.
 * @param image pointer to the image instance
 */
static void set_current(struct image* image)
{
    // put current image to history cache
    if (ctx.current) {
        if (ctx.history.capacity) {
            cache_put(&ctx.history, ctx.current);
        } else {
            image_free(ctx.current);
        }
    }

    ctx.current = image;
    reset_preloader();

#ifdef HAVE_INOTIFY
    // register inotify watcher
    if (ctx.notify >= 0) {
        if (ctx.watch != -1) {
            inotify_rm_watch(ctx.notify, ctx.watch);
        }
        ctx.watch = inotify_add_watch(ctx.notify, ctx.current->source,
                                      IN_CLOSE_WRITE | IN_MOVE_SELF);
    }
#endif
}

void fetcher_init(struct image* image, size_t history, size_t preload)
{
    cache_init(&ctx.history, history);
    cache_init(&ctx.preload, preload);

#ifdef HAVE_INOTIFY
    ctx.notify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (ctx.notify >= 0) {
        app_watch(ctx.notify, on_inotify, NULL);
        ctx.watch = -1;
    }
#endif // HAVE_INOTIFY

    if (image) {
        set_current(image);
    }
}

void fetcher_destroy(void)
{
    cache_free(&ctx.history);
    cache_free(&ctx.preload);
    image_free(ctx.current);
}

bool fetcher_reset(size_t index, bool force)
{
    loader_queue_reset();
    cache_reset(&ctx.history);
    cache_reset(&ctx.preload);
    image_free(ctx.current);
    ctx.current = NULL;

    if (force && index != IMGLIST_INVALID) {
        fetcher_open(index);
    } else {
        if (index == IMGLIST_INVALID) {
            index = image_list_first();
        }
        while (index != IMGLIST_INVALID && !fetcher_open(index)) {
            index = image_list_skip(index);
        }
    }

    return !!ctx.current;
}

bool fetcher_open(size_t index)
{
    struct image* img;

    if (ctx.current && ctx.current->index == index) {
        return true;
    }

    // check history and preload
    img = cache_take(&ctx.history, index);
    if (!img) {
        img = cache_take(&ctx.preload, index);
    }

    if (!img) {
        loader_from_index(index, &img);
    }
    if (img) {
        set_current(img);
    }

    return !!img;
}

void fetcher_attach(struct image* image, size_t index)
{
    if (image) {
        cache_put(&ctx.preload, image);
    } else {
        loader_queue_reset();
        image_list_skip(index);
        reset_preloader();
    }
}

struct image* fetcher_current(void)
{
    return ctx.current;
}

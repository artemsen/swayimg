// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "application.h"
#include "array.h"
#include "buildcfg.h"
#include "exif.h"
#include "imagelist.h"
#include "shellcmd.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/** Background thread loader queue. */
struct loader_queue {
    struct list list; ///< Links to prev/next entry
    size_t index;     ///< Index of the image to load
};

/** Loader context. */
struct loader {
    pthread_t tid;              ///< Background loader thread id
    struct loader_queue* queue; ///< Background thread loader queue
    pthread_mutex_t lock;       ///< Queue access lock
    pthread_cond_t signal;      ///< Queue notification
    pthread_cond_t ready;       ///< Thread ready signal
};

/** Global loader context instance. */
static struct loader ctx;

enum loader_status loader_from_source(const char* source, struct image** image)
{
        return ldr_ioerror;

    enum loader_status status;
    struct image* img;

    // create image instance
    img = image_create(source);
    if (!img) {
        return ldr_ioerror;
    }

    status = image_load(img);

    if (status == ldr_success) {
        *image = img;
    } else {
        image_deref(img);
    }

    return status;
}

enum loader_status loader_from_index(size_t index, struct image** image)
{
    enum loader_status status = ldr_ioerror;
    const char* source = image_list_get(index);

    if (source) {
        status = loader_from_source(source, image);
        if (status == ldr_success) {
            (*image)->index = index;
        }
    }

    return status;
}

/** Image loader executed in background thread. */
static void* loading_thread(__attribute__((unused)) void* data)
{
    struct loader_queue* entry;
    struct image* image;

    do {
        pthread_mutex_lock(&ctx.lock);
        pthread_cond_signal(&ctx.ready);
        while (!ctx.queue) {
            pthread_cond_wait(&ctx.signal, &ctx.lock);
            if (!ctx.queue) {
                pthread_cond_signal(&ctx.ready);
            }
        }
        entry = ctx.queue;
        ctx.queue = list_remove(entry);
        pthread_mutex_unlock(&ctx.lock);

        if (entry->index == IMGLIST_INVALID) {
            free(entry);
            return NULL;
        }

        image = NULL;
        loader_from_index(entry->index, &image);
        // app_on_load(image, entry->index);
        free(entry);
    } while (true);

    return NULL;
}

void loader_init(void)
{
    pthread_cond_init(&ctx.signal, NULL);
    pthread_cond_init(&ctx.ready, NULL);

    pthread_mutex_init(&ctx.lock, NULL);
    pthread_mutex_lock(&ctx.lock);

    pthread_create(&ctx.tid, NULL, loading_thread, NULL);

    pthread_cond_wait(&ctx.ready, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);
}

void loader_destroy(void)
{
    if (ctx.tid) {
        loader_queue_reset();
        loader_queue_append(IMGLIST_INVALID); // send stop signal
        pthread_join(ctx.tid, NULL);

        pthread_mutex_destroy(&ctx.lock);
        pthread_cond_destroy(&ctx.signal);
        pthread_cond_destroy(&ctx.ready);
    }
}

void loader_queue_append(size_t index)
{
    struct loader_queue* entry = malloc(sizeof(*entry));
    if (entry) {
        entry->index = index;
        pthread_mutex_lock(&ctx.lock);
        ctx.queue = list_append(ctx.queue, entry);
        pthread_cond_signal(&ctx.signal);
        pthread_mutex_unlock(&ctx.lock);
    }
}

void loader_queue_reset(void)
{
    pthread_mutex_lock(&ctx.lock);
    list_for_each(ctx.queue, struct loader_queue, it) {
        free(it);
    }
    ctx.queue = NULL;
    pthread_cond_signal(&ctx.signal);
    pthread_cond_wait(&ctx.ready, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);
}

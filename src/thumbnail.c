// SPDX-License-Identifier: MIT
// Create/load/store thumbnails.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "thumbnail.h"

#include "buildcfg.h"
#include "imagelist.h"

#include <stdlib.h>

#ifdef HAVE_LIBPNG
#define THUMBNAIL_PSTORE

#include "formats/png.h"
#include "loader.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif // HAVE_LIBPNG

/** Thumbnail context. */
struct thumbnail_context {
    size_t size;                 ///< Size of thumbnail
    bool fill;                   ///< Scale mode (fill/fit)
    enum pixmap_aa_mode aa_mode; ///< Anti-aliasing mode
    struct thumbnail* thumbs;    ///< List of thumbnails

    bool pstore;             ///< Use persistent storage for thumbnails
    pthread_t tid;           ///< Background loader thread id
    struct thumbnail* queue; ///< Background thread loader queue
    pthread_cond_t signal;   ///< Queue notification
    pthread_mutex_t lock;    ///< Queue access lock
};

/** Global thumbnail context. */
static struct thumbnail_context ctx;

/**
 * Allocate new new entry.
 * @param image thumbnail image
 * @param width,height real image size
 * @return created entry or NULL on error
 */
static struct thumbnail* allocate_entry(struct image* image, size_t width,
                                        size_t height)
{
    struct thumbnail* entry;

    entry = malloc(sizeof(*entry));
    if (entry) {
        entry->image = image;
        entry->width = width;
        entry->height = height;
    }

    return entry;
}

#ifdef THUMBNAIL_PSTORE
/**
 * Get path for the thumbnail on persistent storage.
 * @param source original image source
 * @return path or NULL if not applicable or in case of errors
 */
static char* pstore_path(const char* source)
{
    char* path = NULL;

    if (strcmp(source, LDRSRC_STDIN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        return NULL;
    }

    path = config_expand_path("XDG_CACHE_HOME", "/swayimg");
    if (!path) {
        path = config_expand_path("HOME", "/.cache/swayimg");
    }
    if (path) {
        char state[16];
        snprintf(state, sizeof(state), ".%04x%d%d", (uint16_t)ctx.size,
                 ctx.fill ? 1 : 0, ctx.aa_mode);
        str_append(source, 0, &path);
        str_append(state, 0, &path);
    }

    return path;
}

/**
 * Write thumbnail on persistent storage.
 * @param thumb thumbnail instance to save
 */
static void pstore_save(const struct thumbnail* thumb)
{
    char* th_path;
    uint8_t* th_data = NULL;
    size_t th_size = 0;
    char* delim;

    th_path = pstore_path(thumb->image->source);
    if (!th_path) {
        return;
    }

    // create path
    delim = th_path;
    while (true) {
        delim = strchr(delim + 1, '/');
        if (!delim) {
            break;
        }
        *delim = '\0';
        if (mkdir(th_path, S_IRWXU | S_IRWXG) && errno != EEXIST) {
            free(th_path);
            return;
        }
        *delim = '/';
    }

    // save thumbnail
    if (encode_png(thumb->image, &th_data, &th_size)) {
        const int fd = creat(th_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd != -1) {
            size_t pos = 0;
            while (pos < th_size) {
                const ssize_t written = write(fd, th_data + pos, th_size - pos);
                if (written == -1) {
                    break;
                }
                pos += written;
            }
            close(fd);
        }
        free(th_data);
    }

    free(th_path);
}

/**
 * Load thumbnail from persistent storage.
 * @param index image position in the image list
 * @return thumbnail instance or NULL if not found
 */
const struct thumbnail* pstore_load(size_t index)
{
    struct thumbnail* entry;
    struct image* thumb;
    struct stat st_origin;
    struct stat st_thumb;
    char* path_thumb;
    const char* path_origin;

    path_origin = image_list_get(index);
    if (!path_origin) {
        return NULL;
    }
    path_thumb = pstore_path(path_origin);
    if (!path_thumb) {
        return NULL;
    }

    // check modification time
    if (stat(path_origin, &st_origin) == -1 ||
        stat(path_thumb, &st_thumb) == -1 ||
        st_origin.st_mtim.tv_sec > st_thumb.st_mtim.tv_sec) {
        free(path_thumb);
        return NULL;
    }

    if (loader_from_source(path_thumb, &thumb) != ldr_success) {
        free(path_thumb);
        return NULL;
    }

    thumb->index = index;
    entry = allocate_entry(thumb, 0, 0);
    ctx.thumbs = list_append(ctx.thumbs, entry);

    free(path_thumb);

    return entry;
}

/**
 * Reset pstore saving queue.
 * @param stop flag to stop pstore thread
 */
static void pstore_reset(bool stop)
{
    pthread_mutex_lock(&ctx.lock);
    list_for_each(ctx.queue, struct thumbnail, it) {
        free(it);
    }
    if (stop) {
        ctx.queue = list_append(NULL, allocate_entry(NULL, 0, 0));
        pthread_cond_signal(&ctx.signal);
    } else {
        ctx.queue = NULL;
    }
    pthread_mutex_unlock(&ctx.lock);
}

/** Thumbnail saver executed in background thread. */
static void* pstore_saver_thread(__attribute__((unused)) void* data)
{
    struct thumbnail* entry;

    while (true) {
        pthread_mutex_lock(&ctx.lock);
        while (!ctx.queue) {
            pthread_cond_wait(&ctx.signal, &ctx.lock);
        }

        entry = ctx.queue;
        ctx.queue = list_remove(entry);
        if (!entry->image) {
            free(entry);
            pthread_mutex_unlock(&ctx.lock);
            break;
        }

        pstore_save(entry);

        free(entry);
        pthread_mutex_unlock(&ctx.lock);
    }

    return NULL;
}
#endif // THUMBNAIL_PSTORE

void thumbnail_init(const struct config* cfg)
{
    ctx.size = config_get_num(cfg, CFG_GALLERY, CFG_GLRY_SIZE, 1, 1024);
    ctx.fill = config_get_bool(cfg, CFG_GALLERY, CFG_GLRY_FILL);
    ctx.aa_mode =
        config_get_oneof(cfg, CFG_GALLERY, CFG_GLRY_AA, pixmap_aa_names,
                         ARRAY_SIZE(pixmap_aa_names));

#ifdef THUMBNAIL_PSTORE
    ctx.pstore = config_get_bool(cfg, CFG_GALLERY, CFG_GLRY_PSTORE);
    if (ctx.pstore) {
        pthread_mutex_init(&ctx.lock, NULL);
        pthread_cond_init(&ctx.signal, NULL);
        pthread_create(&ctx.tid, NULL, pstore_saver_thread, NULL);
    }
#endif // THUMBNAIL_PSTORE
}

void thumbnail_free(void)
{
#ifdef THUMBNAIL_PSTORE
    if (ctx.pstore) {
        if (ctx.tid) {
            pstore_reset(true);
            pthread_join(ctx.tid, NULL);
        }
        pthread_mutex_destroy(&ctx.lock);
        pthread_cond_destroy(&ctx.signal);
    }
#endif // THUMBNAIL_PSTORE

    list_for_each(ctx.thumbs, struct thumbnail, it) {
        image_free(it->image);
        free(it);
    }
}

enum pixmap_aa_mode thumbnail_get_aa(void)
{
    return ctx.aa_mode;
}

enum pixmap_aa_mode thumbnail_switch_aa(void)
{
    if (++ctx.aa_mode >= ARRAY_SIZE(pixmap_aa_names)) {
        ctx.aa_mode = 0;
    }
    return ctx.aa_mode;
}

void thumbnail_add(struct image* image)
{
    struct thumbnail* entry;
    struct pixmap thumb;
    struct image_frame* frame;
    ssize_t offset_x, offset_y;

    const struct pixmap* full = &image->frames[0].pm;
    const size_t real_width = full->width;
    const size_t real_height = full->height;
    const float scale_width = 1.0 / ((float)real_width / ctx.size);
    const float scale_height = 1.0 / ((float)real_height / ctx.size);
    const float scale = ctx.fill ? max(scale_width, scale_height)
                                 : min(scale_width, scale_height);
    size_t thumb_width = scale * real_width;
    size_t thumb_height = scale * real_height;

    if (ctx.fill) {
        offset_x = ctx.size / 2 - thumb_width / 2;
        offset_y = ctx.size / 2 - thumb_height / 2;
        thumb_width = ctx.size;
        thumb_height = ctx.size;
    } else {
        offset_x = 0;
        offset_y = 0;
    }

    // create thumbnail from image (replace the first frame)
    if (!pixmap_create(&thumb, thumb_width, thumb_height)) {
        image_free(image);
        return;
    }
    pixmap_scale(ctx.aa_mode, full, &thumb, offset_x, offset_y, scale,
                 image->alpha);
    image_free_frames(image);
    frame = image_create_frames(image, 1);
    if (!frame) {
        pixmap_free(&thumb);
        image_free(image);
        return;
    }
    frame->pm = thumb;

    // add entry to the list
    entry = allocate_entry(image, real_width, real_height);
    ctx.thumbs = list_append(ctx.thumbs, entry);

#ifdef THUMBNAIL_PSTORE
    if (entry && ctx.pstore &&
        (real_width > ctx.size || real_height > ctx.size)) {
        // save thumbnail to persistent storage
        struct thumbnail* save_entry =
            allocate_entry(entry->image, entry->width, entry->height);
        pthread_mutex_lock(&ctx.lock);
        ctx.queue = list_append(ctx.queue, save_entry);
        pthread_cond_signal(&ctx.signal);
        pthread_mutex_unlock(&ctx.lock);
    }
#endif // THUMBNAIL_PSTORE
}

const struct thumbnail* thumbnail_get(size_t index)
{
    const struct thumbnail* thumb = NULL;

    list_for_each(ctx.thumbs, struct thumbnail, it) {
        if (it->image->index == index) {
            thumb = it;
            break;
        }
    }

#ifdef THUMBNAIL_PSTORE
    if (!thumb && ctx.pstore) {
        thumb = pstore_load(index);
    }
#endif // THUMBNAIL_PSTORE

    return thumb;
}

void thumbnail_remove(size_t index)
{
    pstore_reset(false);

    list_for_each(ctx.thumbs, struct thumbnail, it) {
        if (it->image->index == index) {
            ctx.thumbs = list_remove(it);
            image_free(it->image);
            free(it);
            break;
        }
    }
}

void thumbnail_clear(size_t min_id, size_t max_id)
{
    pstore_reset(false);

    if (min_id == IMGLIST_INVALID && max_id == IMGLIST_INVALID) {
        list_for_each(ctx.thumbs, struct thumbnail, it) {
            ctx.thumbs = list_remove(it);
            image_free(it->image);
            free(it);
        }
    } else {
        list_for_each(ctx.thumbs, struct thumbnail, it) {
            if ((min_id != IMGLIST_INVALID && it->image->index < min_id) ||
                (max_id != IMGLIST_INVALID && it->image->index > max_id)) {
                ctx.thumbs = list_remove(it);
                image_free(it->image);
                free(it);
            }
        }
    }
}

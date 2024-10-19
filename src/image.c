// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/limits.h>
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
#define PATH_MAX 4096
#endif

struct image* image_create(void)
{
    return calloc(1, sizeof(struct image));
}

void image_free(struct image* ctx)
{
    if (ctx) {
        image_free_frames(ctx);
        free(ctx->source);
        free(ctx->format);

        while (ctx->num_info) {
            --ctx->num_info;
            free(ctx->info[ctx->num_info].value);
        }
        free(ctx->info);

        free(ctx);
    }
}

void image_flip_vertical(struct image* ctx)
{
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        pixmap_flip_vertical(&ctx->frames[i].pm);
    }
}

void image_flip_horizontal(struct image* ctx)
{
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        pixmap_flip_horizontal(&ctx->frames[i].pm);
    }
}

void image_rotate(struct image* ctx, size_t angle)
{
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        pixmap_rotate(&ctx->frames[i].pm, angle);
    }
}

// TODO: probably should be moved to config, to make use of expand_path()
/**
 * Get path to cached image thumbnail.
 * @param image image
 * @param path array in which the path should be stored
 * @return true if successful
 */
static bool get_image_thumb_path(struct image* image, char* path)
{
    static char* cache_dir = NULL;
    int r;
    if (!cache_dir) {
        cache_dir = getenv("XDG_CACHE_HOME");
        if (cache_dir) {
            cache_dir = strcat(cache_dir, "/swayimg");
        } else {
            cache_dir = getenv("HOME");
            if (!cache_dir) {
                return false;
            }
            cache_dir = strcat(cache_dir, "/.swayimg");
        }
    }
    r = snprintf(path, PATH_MAX, "%s%s", cache_dir, image->source);
    return r >= 0 && r < PATH_MAX;
}

// TODO: where should this function be?
/**
 * Makes directories like `mkdir -p`.
 * @param path absolute path to the directory to be created
 * @return true if successful
 */
static bool make_directories(char* path)
{
    char* slash;

    if (!path || !*path) {
        return false;
    }

    slash = path;
    while (true) {
        slash = strchr(slash + 1, '/');
        if (!slash) {
            break;
        }
        *slash = '\0';
        if (mkdir(path, 0755) && errno != EEXIST) {
            // TODO: do we want to print error message?
            return false;
        }
        *slash = '/';
    }

    return true;
}

void image_thumbnail(struct image* image, size_t size, bool fill,
                     bool antialias)
{
    struct pixmap thumb;
    struct image_frame* frame;
    const struct pixmap* full = &image->frames[0].pm;
    const float scale_width = 1.0 / ((float)full->width / size);
    const float scale_height = 1.0 / ((float)full->height / size);
    const float scale =
        fill ? max(scale_width, scale_height) : min(scale_width, scale_height);
    size_t thumb_width = scale * full->width;
    size_t thumb_height = scale * full->height;
    ssize_t offset_x, offset_y;
    enum pixmap_scale scaler;
    char thumb_path[PATH_MAX] = { 0 };
    struct stat attr_img, attr_thumb;

    // get thumbnail from cache
    if (get_image_thumb_path(image, thumb_path) &&
        !stat(image->source, &attr_img) && !stat(thumb_path, &attr_thumb) &&
        difftime(attr_img.st_ctime, attr_thumb.st_ctime) <= 0 &&
        pixmap_load(&thumb, thumb_path)) {
        goto thumb_done;
    }

    if (antialias) {
        scaler = (scale > 1.0) ? pixmap_bicubic : pixmap_average;
    } else {
        scaler = pixmap_nearest;
    }

    if (fill) {
        offset_x = size / 2 - thumb_width / 2;
        offset_y = size / 2 - thumb_height / 2;
        thumb_width = size;
        thumb_height = size;
    } else {
        offset_x = 0;
        offset_y = 0;
    }

    // create thumbnail
    if (!pixmap_create(&thumb, thumb_width, thumb_height)) {
        return;
    }
    pixmap_scale(scaler, full, &thumb, offset_x, offset_y, scale, image->alpha);

    // save thumbnail to disk
    if (make_directories(thumb_path)) {
        pixmap_save(&thumb, thumb_path);
    }

thumb_done:
    image_free_frames(image);
    frame = image_create_frames(image, 1);
    if (frame) {
        frame->pm = thumb;
    }
}

void image_set_format(struct image* ctx, const char* fmt, ...)
{
    va_list args;
    int len;
    char* buffer;

    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }
    ++len; // last null
    buffer = realloc(ctx->format, len);
    if (!buffer) {
        return;
    }
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);
    ctx->format = buffer;
}

void image_add_meta(struct image* ctx, const char* key, const char* fmt, ...)
{
    va_list args;
    int len;
    void* buffer;

    buffer =
        realloc(ctx->info, (ctx->num_info + 1) * sizeof(struct image_info));
    if (!buffer) {
        return;
    }
    ctx->info = buffer;

    // construct value string
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }
    ++len; // last null
    buffer = malloc(len);
    if (!buffer) {
        return;
    }
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    ctx->info[ctx->num_info].key = key;
    ctx->info[ctx->num_info].value = buffer;
    ++ctx->num_info;
}

struct pixmap* image_allocate_frame(struct image* ctx, size_t width,
                                    size_t height)
{
    if (image_create_frames(ctx, 1) &&
        pixmap_create(&ctx->frames[0].pm, width, height)) {
        return &ctx->frames[0].pm;
    }
    image_free_frames(ctx);
    return NULL;
}

struct image_frame* image_create_frames(struct image* ctx, size_t num)
{
    struct image_frame* frames;

    frames = calloc(1, num * sizeof(struct image_frame));
    if (frames) {
        ctx->frames = frames;
        ctx->num_frames = num;
    }

    return frames;
}

void image_free_frames(struct image* ctx)
{
    for (size_t i = 0; i < ctx->num_frames; ++i) {
        pixmap_free(&ctx->frames[i].pm);
    }
    free(ctx->frames);
    ctx->frames = NULL;
    ctx->num_frames = 0;
}

// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include <linux/limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* TODO: probably should be moved to config, to make use of expand_path() */
bool image_thumb_path(struct image* image, char* path)
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
    char thumb_path[PATH_MAX] = { 0 }; /* PATH_MAX */
    char cmd[PATH_MAX + 10] = { 0 };   /* PATH_MAX + strlen("mkdir -p ") */
    char* last_slash;

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

    if (!image_thumb_path(image, thumb_path) ||
        !pixmap_load(&thumb, thumb_path) ||
        !(thumb.width == thumb_width && thumb.height == thumb_height)) {
        goto thumb_create;
    }

    goto thumb_done;
thumb_create:
    // create thumbnail
    if (!pixmap_create(&thumb, thumb_width, thumb_height)) {
        return;
    }
    pixmap_scale(scaler, full, &thumb, offset_x, offset_y, scale, image->alpha);

    // save thumbnail to disk

    // TODO: probably should be moved outside of this function
    // TODO: do not use system() here
    if (thumb_path[0]) {
        last_slash = strrchr(thumb_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            sprintf(cmd, "mkdir -p %s", thumb_path);
            *last_slash = '/';
            system(cmd);
        }
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

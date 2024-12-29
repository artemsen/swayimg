// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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

void image_thumbnail(struct image* image, struct pixmap* thumbnail)
{
    struct image_frame* frame;

    image_free_frames(image);
    frame = image_create_frames(image, 1);
    if (frame) {
        frame->pm = *thumbnail;
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

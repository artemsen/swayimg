// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct image* image_alloc(void)
{
    return calloc(1, sizeof(struct image));
}

void image_free(struct image* ctx)
{
    if (ctx) {
        image_free_frames(ctx);

        free(ctx->source);
        free(ctx->parent_dir);
        free(ctx->format);

        list_for_each(ctx->info, struct image_info, it) {
            free(it);
        }

        free(ctx);
    }
}

void image_set_source(struct image* ctx, const char* source)
{
    ctx->source = str_dup(source, NULL);

    // set name and parent dir
    if (strcmp(source, LDRSRC_STDIN) == 0 ||
        strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        ctx->name = ctx->source;
        str_dup("", &ctx->parent_dir);
    } else {
        size_t pos = strlen(ctx->source) - 1;
        // get name
        while (pos && ctx->source[--pos] != '/') { }
        ctx->name = ctx->source + pos + (ctx->source[pos] == '/' ? 1 : 0);
        // get parent dir
        if (pos == 0) {
            str_dup("", &ctx->parent_dir);
        } else {
            const size_t end = pos;
            while (pos && ctx->source[--pos] != '/') { }
            if (ctx->source[pos] == '/') {
                ++pos;
            }
            str_append(ctx->source + pos, end - pos, &ctx->parent_dir);
        }
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
    struct image_info* entry;
    const size_t key_len = strlen(key) + 1 /* last null */;

    // get value string size
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }

    // allocate new entry
    len += sizeof(struct image_info) + key_len + 1 /* last null of value */;
    entry = calloc(1, len);
    if (!entry) {
        return;
    }

    // fill entry
    entry->key = (char*)entry + sizeof(struct image_info);
    memcpy(entry->key, key, key_len);
    entry->value = entry->key + key_len;
    va_start(args, fmt);
    vsprintf(entry->value, fmt, args);
    va_end(args);

    ctx->info = list_append(ctx->info, entry);
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

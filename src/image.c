// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include "buildcfg.h"
#include "exif.h"
#include "formats/loader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Add image size as text to meta info.
 * @param ctx image context
 * @param bytes file size in bytes
 */
static void add_size_info(struct image* ctx, size_t bytes)
{
    const size_t kibibyte = 1024;
    const size_t mebibyte = kibibyte * 1024;
    float size = bytes;
    char unit;

    if (bytes >= mebibyte) {
        size /= mebibyte;
        unit = 'M';
    } else {
        size /= kibibyte;
        unit = 'K';
    }
    image_add_meta(ctx, "File size", "%.02f %ciB", size, unit);
}

/**
 * Create image instance from memory buffer.
 * @param path path to the image
 * @param data raw image data
 * @param size size of image data in bytes
 * @return image instance or NULL on errors
 */
static struct image* image_create(const char* path, const uint8_t* data,
                                  size_t size)
{
    struct image* ctx;
    enum loader_status status;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    ctx->path = path;

    image_add_meta(ctx, "File", "%s", image_file_name(ctx));
    add_size_info(ctx, size);

    status = image_decode(ctx, data, size);
    if (status != ldr_success) {
        if (status == ldr_unsupported) {
            image_error(ctx, "unsupported format");
        }
        image_free(ctx);
        return NULL;
    }

    image_add_meta(ctx, "Image size", "%lux%lu", ctx->width, ctx->height);

#ifdef HAVE_LIBEXIF
    process_exif(ctx, data, size);
#endif

    return ctx;
}

struct image* image_from_file(const char* file)
{
    struct image* ctx = NULL;
    void* data = MAP_FAILED;
    struct stat st;
    int fd;

    // open file
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s: %s\n", file, strerror(errno));
        goto done;
    }
    // get file size
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "%s: %s\n", file, strerror(errno));
        goto done;
    }
    // map file to memory
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "%s: %s\n", file, strerror(errno));
        goto done;
    }

    ctx = image_create(file, data, st.st_size);

done:
    if (data != MAP_FAILED) {
        munmap(data, st.st_size);
    }
    if (fd != -1) {
        close(fd);
    }
    return ctx;
}

struct image* image_from_stdin(void)
{
    struct image* ctx = NULL;
    uint8_t* data = NULL;
    size_t size = 0;
    size_t capacity = 0;

    while (true) {
        if (size == capacity) {
            const size_t new_capacity = capacity + 256 * 1024;
            uint8_t* new_buf = realloc(data, new_capacity);
            if (!new_buf) {
                fprintf(stderr, "Not enough memory\n");
                goto done;
            }
            data = new_buf;
            capacity = new_capacity;
        }

        const ssize_t rc = read(STDIN_FILENO, data + size, capacity - size);
        if (rc == 0) {
            break;
        }
        if (rc == -1 && errno != EAGAIN) {
            perror("Error reading stdin");
            goto done;
        }
        size += rc;
    }

    if (data) {
        ctx = image_create("{STDIN}", data, size);
    }

done:
    free(data);
    return ctx;
}

void image_free(struct image* ctx)
{
    if (ctx) {
        while (ctx->info) {
            struct meta* next = ctx->info->next;
            free((void*)ctx->info->key);
            free((void*)ctx->info->value);
            free((void*)ctx->info);
            ctx->info = next;
        }

        free((void*)ctx->data);
        free((void*)ctx->info);
        free(ctx);
    }
}

const char* image_file_name(const struct image* ctx)
{
    const char* name = strrchr(ctx->path, '/');
    if (name) {
        return ++name; // skip slash
    } else {
        return ctx->path;
    }
}

void image_flip_vertical(struct image* ctx)
{
    argb_t* data = (argb_t*)ctx->data;
    const size_t stride = ctx->width * sizeof(argb_t);
    void* buffer;

    buffer = malloc(stride);
    if (buffer) {
        for (size_t y = 0; y < ctx->height / 2; ++y) {
            void* src = &data[y * ctx->width];
            void* dst = &data[(ctx->height - y - 1) * ctx->width];
            memcpy(buffer, dst, stride);
            memcpy(dst, src, stride);
            memcpy(src, buffer, stride);
        }
        free(buffer);
    }
}

void image_flip_horizontal(struct image* ctx)
{
    argb_t* data = (argb_t*)ctx->data;

    for (size_t y = 0; y < ctx->height; ++y) {
        argb_t* line = &data[y * ctx->width];
        for (size_t x = 0; x < ctx->width / 2; ++x) {
            argb_t* left = &line[x];
            argb_t* right = &line[ctx->width - x];
            const argb_t swap = *left;
            *left = *right;
            *right = swap;
        }
    }
}

void image_rotate(struct image* ctx, size_t angle)
{
    argb_t* data = (argb_t*)ctx->data;
    const size_t pixels = ctx->width * ctx->height;

    if (angle == 180) {
        for (size_t i = 0; i < pixels / 2; ++i) {
            argb_t* color1 = &data[i];
            argb_t* color2 = &data[pixels - i - 1];
            const uint32_t swap = *color1;
            *color1 = *color2;
            *color2 = swap;
        }
    } else if (angle == 90 || angle == 270) {
        const size_t buf_len = pixels * sizeof(argb_t);
        argb_t* new_buffer = malloc(buf_len);
        if (new_buffer) {
            const size_t new_width = ctx->height;
            const size_t new_height = ctx->width;
            for (size_t y = 0; y < ctx->height; ++y) {
                for (size_t x = 0; x < ctx->width; ++x) {
                    size_t new_pos;
                    if (angle == 90) {
                        new_pos = x * new_width + (new_width - y - 1);
                    } else {
                        new_pos = (new_height - x - 1) * new_width + y;
                    }
                    new_buffer[new_pos] = ctx->data[y * ctx->width + x];
                }
            }
            free((void*)ctx->data);
            ctx->width = new_width;
            ctx->height = new_height;
            ctx->data = new_buffer;
        }
    }
}

void image_add_meta(struct image* ctx, const char* key, const char* fmt, ...)
{
    va_list args;
    struct meta* entry;
    const size_t key_len = strlen(key) + 1 /*last null*/;
    int val_len;

    entry = malloc(sizeof(struct meta));
    if (!entry) {
        return;
    }
    entry->next = NULL;

    // contruct value string
    va_start(args, fmt);
    val_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (val_len <= 0) {
        free(entry);
        return;
    }
    ++val_len; // last null
    entry->value = malloc(val_len);
    if (!entry->value) {
        free(entry);
        return;
    }
    va_start(args, fmt);
    vsprintf((char*)entry->value, fmt, args);
    va_end(args);

    // strdup for key
    entry->key = malloc(key_len);
    if (!entry->key) {
        free((void*)entry->value);
        free(entry);
        return;
    }
    memcpy((void*)entry->key, key, key_len);

    if (!ctx->info) {
        ctx->info = entry;
    } else {
        // get last entry
        struct meta* last = ctx->info;
        while (last->next) {
            last = last->next;
        }
        last->next = entry;
    }
}

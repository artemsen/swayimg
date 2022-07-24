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

    image_add_meta(ctx, "File", "%s", path);

    status = image_decode(ctx, data, size);
    if (status != ldr_success) {
        if (status == ldr_unsupported) {
            image_error(ctx, "unsupported format");
        }
        image_free(ctx);
        return NULL;
    }

    add_size_info(ctx, size);
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
        fprintf(stderr, "Unable to open file %s: %s\n", file, strerror(errno));
        goto done;
    }
    // get file size
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "Unable to get file stat for %s: %s\n", file,
                strerror(errno));
        goto done;
    }
    // map file to memory
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Unable to map shared file: [%i] %s\n", errno,
                strerror(errno));
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
        free(ctx->data);
        free((void*)ctx->info);
        free(ctx);
    }
}

void image_flip_vertical(struct image* ctx)
{
    const size_t stride = ctx->width * sizeof(argb_t);
    void* buffer = malloc(stride);
    if (buffer) {
        for (size_t y = 0; y < ctx->height / 2; ++y) {
            void* src = &ctx->data[y * ctx->width];
            void* dst = &ctx->data[(ctx->height - y - 1) * ctx->width];
            memcpy(buffer, dst, stride);
            memcpy(dst, src, stride);
            memcpy(src, buffer, stride);
        }
        free(buffer);
    }
}

void image_flip_horizontal(struct image* ctx)
{
    for (size_t y = 0; y < ctx->height; ++y) {
        uint32_t* line = &ctx->data[y * ctx->width];
        for (size_t x = 0; x < ctx->width / 2; ++x) {
            uint32_t* left = &line[x];
            uint32_t* right = &line[ctx->width - x];
            const uint32_t swap = *left;
            *left = *right;
            *right = swap;
        }
    }
}

void image_rotate(struct image* ctx, size_t angle)
{
    const size_t pixels = ctx->width * ctx->height;

    if (angle == 180) {
        for (size_t i = 0; i < pixels / 2; ++i) {
            uint32_t* color1 = &ctx->data[i];
            uint32_t* color2 = &ctx->data[pixels - i - 1];
            const uint32_t swap = *color1;
            *color1 = *color2;
            *color2 = swap;
        }
    } else if (angle == 90 || angle == 270) {
        const size_t buf_len = pixels * sizeof(argb_t);
        uint32_t* new_buffer = malloc(buf_len);
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
            free(ctx->data);
            ctx->width = new_width;
            ctx->height = new_height;
            ctx->data = new_buffer;
        }
    }
}

static void add_meta(struct image* ctx, const char* key, const char* value)
{
    char* buffer = (char*)ctx->info;
    const char* delim = ":\t";
    const size_t cur_len = ctx->info ? strlen(ctx->info) + 1 : 0;
    const size_t add_len = strlen(key) + strlen(value) + strlen(delim) + 1;

    buffer = realloc(buffer, cur_len + add_len);
    if (buffer) {
        if (cur_len == 0) {
            buffer[0] = 0;
        } else {
            strcat(buffer, "\n");
        }
        strcat(buffer, key);
        strcat(buffer, delim);
        strcat(buffer, value);
        ctx->info = buffer;
    }
}

void image_add_meta(struct image* ctx, const char* key, const char* fmt, ...)
{
    int len;
    va_list args;
    char* text;

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len > 0) {
        ++len; // last null
        text = malloc(len);
        if (text) {
            va_start(args, fmt);
            len = vsnprintf(text, len, fmt, args);
            va_end(args);
            if (len > 0) {
                add_meta(ctx, key, text);
            }
            free(text);
        }
    }
}

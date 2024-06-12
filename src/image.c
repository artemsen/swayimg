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

    // save common file info
    ctx->file_path = path;
    ctx->file_name = strrchr(path, '/');
    if (!ctx->file_name) {
        ctx->file_name = path;
    } else {
        ++ctx->file_name; // skip slash
    }
    ctx->file_size = size;

    // decode image
    status = load_image(ctx, data, size);
    if (status != ldr_success) {
        if (status == ldr_unsupported) {
            image_print_error(ctx, "unsupported format");
        }
        image_free(ctx);
        return NULL;
    }

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
        ctx = image_create(STDIN_FILE_NAME, data, size);
    }

done:
    free(data);
    return ctx;
}

void image_free(struct image* ctx)
{
    if (ctx) {
        image_free_frames(ctx);
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

void image_set_format(struct image* ctx, const char* fmt, ...)
{
    va_list args;
    int len;
    char* buffer;

    va_start(args, fmt);
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
    } else {
        image_print_error(ctx, "not enough memory");
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

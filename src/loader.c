// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/** File list context. */
struct context {
    struct image* image; ///< Currently displayed image
    const char** files;  ///< List of files to view
    size_t total;        ///< Total number of files in the list
    ssize_t current;     ///< Index of currently displayed image in the list
};

static struct context ctx;

/**
 * Image loader function.
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 * @return image instance or NULL if decode failed
 */
typedef struct image* (*loader)(const uint8_t* data, size_t size);

// declaration of loaders
struct image* load_bmp(const uint8_t* data, size_t size);
struct image* load_png(const uint8_t* data, size_t size);
#ifdef HAVE_LIBJPEG
struct image* load_jpeg(const uint8_t* data, size_t size);
#endif
#ifdef HAVE_LIBGIF
struct image* load_gif(const uint8_t* data, size_t size);
#endif
#ifdef HAVE_LIBRSVG
struct image* load_svg(const uint8_t* data, size_t size);
#endif
#ifdef HAVE_LIBWEBP
struct image* load_webp(const uint8_t* data, size_t size);
#endif
#ifdef HAVE_LIBAVIF
struct image* load_avif(const uint8_t* data, size_t size);
#endif

// list of available loaders (functions from formats/*)
static const loader loaders[] = {
    &load_png,
    &load_bmp,
#ifdef HAVE_LIBJPEG
    &load_jpeg,
#endif
#ifdef HAVE_LIBGIF
    &load_gif,
#endif
#ifdef HAVE_LIBRSVG
    &load_svg,
#endif
#ifdef HAVE_LIBWEBP
    &load_webp,
#endif
#ifdef HAVE_LIBAVIF
    &load_avif,
#endif
};

/**
 * Load image from memory buffer.
 * @param[in] file path to the file to load
 * @return image instance or NULL on errors
 */
static struct image* load_image_data(const void* data, size_t size)
{
    struct image* img = NULL;
    for (size_t i = 0; !img && i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        img = loaders[i]((const uint8_t*)data, size);
    }
    return img;
}

/**
 * Load image from file.
 * @param[in] file path to the file to load
 * @return image instance or NULL on errors
 */
static struct image* load_image_file(const char* file)
{
    struct image* img = NULL;
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
        fprintf(stderr, "Unable to get file stat for %s: %s\n", file, strerror(errno));
        goto done;
    }
    // map file to memory
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Unable to map shared file: [%i] %s\n", errno,
                strerror(errno));
        goto done;
    }

    img = load_image_data(data, st.st_size);
    if (img) {
        img->name = file;
    } else {
        fprintf(stderr, "Unsupported file format: %s\n", file);
    }

done:
    if (data != MAP_FAILED) {
        munmap(data, st.st_size);
    }
    if (fd != -1) {
        close(fd);
    }
    return img;
}

void loader_init(const char** files, size_t count)
{
    ctx.files = files;
    ctx.total = count;
    ctx.current = -1;
}

void loader_free(void)
{
    if (ctx.image) {
        free_image(ctx.image);
    }
}

struct image* load_next_file(bool forward)
{
    struct image* img = NULL;

    const ssize_t delta = forward ? 1 : -1;
    ssize_t idx = ctx.current;

    while (!img) {
        idx += delta;
        if (idx >= (ssize_t)ctx.total) {
            if (ctx.current < 0) {
                break; // no valid files found
            }
            idx = 0;
        } else if (idx < 0) {
            idx = ctx.total - 1;
        }
        img = load_image_file(ctx.files[idx]);
    }

    if (img) {
        if (ctx.image) {
            free_image(ctx.image);
        }
        ctx.image = img;
        ctx.current = idx;
    }

    return ctx.image;
}

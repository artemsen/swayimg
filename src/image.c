// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "image.h"

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

struct image* load_image(const void* data, size_t size)
{
    struct image* img = NULL;
    for (size_t i = 0; !img && i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        img = loaders[i]((const uint8_t*)data, size);
    }
    return img;
}

struct image* load_image_file(const char* file)
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

    img = load_image(data, st.st_size);
    if (img) {
        img->name = file;
    } else {
        fprintf(stderr, "Unsupported file format: %s\n", file);
    }

done:
    if (data != MAP_FAILED) {
        munmap(data, st.st_size);
    }
    if (fd == -1) {
        close(fd);
    }
    return img;
}

struct image* create_image(cairo_format_t color, size_t width, size_t height)
{
    struct image* img = NULL;

    img = calloc(1, sizeof(struct image));
    if (!img) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    img->surface = cairo_image_surface_create(color, width, height);
    const cairo_status_t status = cairo_surface_status(img->surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Unable to create surface: %s\n",
                cairo_status_to_string(status));
        free_image(img);
        return NULL;
    }

    return img;
}

void set_image_meta(struct image* img, const char* format, ...)
{
    int len;

    va_list args;
    va_start(args, format);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (len < 0) {
        fprintf(stderr, "Invalid format description\n");
        free_image(img);
        return;
    }

    len += 1; // including the terminating null
    img->format = malloc(len);
    if (!img->format) {
        fprintf(stderr, "Not enough memory\n");
        free_image(img);
        return;
    }

    va_start(args, format);
    vsnprintf((char*)img->format, len, format, args);
    va_end(args);
}

void free_image(struct image* img)
{
    if (img) {
        if (img->format) {
            free((void*)img->format);
        }
        if (img->surface) {
            cairo_surface_destroy(img->surface);
        }
        free(img);
    }
}

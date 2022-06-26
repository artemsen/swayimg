// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include "buildcfg.h"
#include "exif.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Image loader function.
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 * @param[in] format buffer for format description
 * @param[in] format_sz size of format buffer
 * @return image surface or NULL if load failed
 */
typedef cairo_surface_t* (*image_load)(const uint8_t* data, size_t size,
                                       char* format, size_t format_sz);
// Construct function name of loader
#define LOADER_FUNCTION(name) load_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                                 \
    cairo_surface_t* LOADER_FUNCTION(name)(const uint8_t* data, size_t size, \
                                           char* format, size_t format_sz)

// declaration of loaders
#ifdef HAVE_LIBJPEG
LOADER_DECLARE(jpeg);
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
LOADER_DECLARE(png);
#endif // HAVE_LIBPNG
#ifdef HAVE_LIBWEBP
LOADER_DECLARE(webp);
#endif // HAVE_LIBWEBP
#ifdef HAVE_LIBGIF
LOADER_DECLARE(gif);
#endif // HAVE_LIBGIF
LOADER_DECLARE(bmp);
#ifdef HAVE_LIBRSVG
LOADER_DECLARE(svg);
#endif // HAVE_LIBRSVG
#ifdef HAVE_LIBAVIF
LOADER_DECLARE(avif);
#endif // HAVE_LIBAVIF
#ifdef HAVE_LIBJXL
LOADER_DECLARE(jxl);
#endif // HAVE_LIBJXL

// list of available loaders (functions from formats/*)
static const image_load loaders[] = {
#ifdef HAVE_LIBJPEG
    &LOADER_FUNCTION(jpeg),
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBPNG
    &LOADER_FUNCTION(png),
#endif // HAVE_LIBPNG
#ifdef HAVE_LIBWEBP
    &LOADER_FUNCTION(webp),
#endif // HAVE_LIBWEBP
#ifdef HAVE_LIBGIF
    &LOADER_FUNCTION(gif),
#endif // HAVE_LIBGIF
    &LOADER_FUNCTION(bmp),
#ifdef HAVE_LIBRSVG
    &LOADER_FUNCTION(svg),
#endif // HAVE_LIBRSVG
#ifdef HAVE_LIBAVIF
    &LOADER_FUNCTION(avif),
#endif // HAVE_LIBAVIF
#ifdef HAVE_LIBJXL
    &LOADER_FUNCTION(jxl),
#endif // HAVE_LIBJXL
};

const char* supported_formats(void)
{
    return "bmp"
#ifdef HAVE_LIBJPEG
           ", jpeg"
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBJXL
           ", jxl"
#endif // HAVE_LIBJXL
#ifdef HAVE_LIBPNG
           ", png"
#endif // HAVE_LIBPNG
#ifdef HAVE_LIBGIF
           ", gif"
#endif // HAVE_LIBGIF
#ifdef HAVE_LIBRSVG
           ", svg"
#endif // HAVE_LIBRSVG
#ifdef HAVE_LIBWEBP
           ", webp"
#endif // HAVE_LIBWEBP
#ifdef HAVE_LIBAVIF
           ", avif"
#endif // HAVE_LIBAVIF
        ;
}

/**
 * Convert file size to human readable text.
 * @param[in] bytes file size in bytes
 * @param[out] text output text
 * @param[in] len size of output buffer
 */
static void human_size(uint64_t bytes, char* text, size_t len)
{
    const size_t kib = 1024;
    const size_t mib = kib * 1024;
    const size_t gib = mib * 1024;
    const size_t tib = gib * 1024;

    size_t multiplier;
    char prefix;
    if (bytes > tib) {
        multiplier = tib;
        prefix = 'T';
    } else if (bytes >= gib) {
        multiplier = gib;
        prefix = 'G';
    } else if (bytes >= mib) {
        multiplier = mib;
        prefix = 'M';
    } else {
        multiplier = kib;
        prefix = 'K';
    }

    snprintf(text, len, "%.02f %ciB", (double)bytes / multiplier, prefix);
}

/**
 * Create image instance from memory buffer.
 * @param[in] path path to the image
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 * @return image instance or NULL on errors
 */
static image_t* image_create(const char* path, const uint8_t* data, size_t size)
{
    image_t* img;
    char meta[32];
    cairo_surface_t* surface = NULL;

    // decode image
    for (size_t i = 0; i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        surface = loaders[i](data, size, meta, sizeof(meta));
        if (surface) {
            break;
        }
    }

    if (!surface) {
        // failed to load
        return NULL;
    }

    // create image instance
    img = calloc(1, sizeof(image_t));
    if (!img) {
        cairo_surface_destroy(surface);
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    img->surface = surface;
    img->path = path;

    path = strrchr(path, '/');
    if (path) {
        ++path; // skip slash
    } else {
        path = img->path; // use full path
    }

    // add general meta info
    add_image_info(img, "File", path);
    add_image_info(img, "Format", meta);
    human_size(size, meta, sizeof(meta));
    add_image_info(img, "File size", meta);
    snprintf(meta, sizeof(meta), "%ix%i",
             cairo_image_surface_get_width(img->surface),
             cairo_image_surface_get_height(img->surface));
    add_image_info(img, "Image size", meta);

#ifdef HAVE_LIBEXIF
    // handle EXIF data
    read_exif(img, data, size);
#endif // HAVE_LIBEXIF

    return img;
}

image_t* image_from_file(const char* file)
{
    image_t* img = NULL;
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

    img = image_create(file, data, st.st_size);
    if (!img) {
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

image_t* image_from_stdin(void)
{
    image_t* img = NULL;
    uint8_t* data = NULL;
    size_t size = 0;
    size_t capacity = 0;

    while (1) {
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
        img = image_create("{STDIN}", data, size);
        if (!img) {
            fprintf(stderr, "Unsupported file format\n");
        }
    }

done:
    if (data) {
        free(data);
    }
    return img;
}

void image_free(image_t* img)
{
    if (img) {
        if (img->surface) {
            cairo_surface_destroy(img->surface);
        }
        free((void*)img->info);
        free(img);
    }
}

void add_image_info(image_t* img, const char* key, const char* value)
{
    char* buffer = (char*)img->info;
    const char* delim = ":\t";
    const size_t cur_len = img->info ? strlen(img->info) + 1 : 0;
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
        img->info = buffer;
    }
}

// SPDX-License-Identifier: MIT
// Image loader: interface and common framework for decoding images.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "buildcfg.h"
#include "exif.h"
#include "str.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Construct function name of loader
#define LOADER_FUNCTION(name) decode_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                     \
    enum loader_status LOADER_FUNCTION(name)(struct image * ctx, \
                                             const uint8_t* data, size_t size)

const char* supported_formats = "bmp, pnm, tga"
#ifdef HAVE_LIBJPEG
                                ", jpeg"
#endif
#ifdef HAVE_LIBPNG
                                ", png"
#endif
#ifdef HAVE_LIBGIF
                                ", gif"
#endif
#ifdef HAVE_LIBWEBP
                                ", webp"
#endif
#ifdef HAVE_LIBRSVG
                                ", svg"
#endif
#ifdef HAVE_LIBHEIF
                                ", heif, avif"
#endif
#ifdef HAVE_LIBAVIF
#ifndef HAVE_LIBHEIF
                                ", avif"
#endif
                                ", avifs"
#endif
#ifdef HAVE_LIBJXL
                                ", jxl"
#endif
#ifdef HAVE_LIBEXR
                                ", exr"
#endif
#ifdef HAVE_LIBTIFF
                                ", tiff"
#endif
    ;

// declaration of loaders
LOADER_DECLARE(bmp);
LOADER_DECLARE(pnm);
LOADER_DECLARE(tga);
#ifdef HAVE_LIBEXR
LOADER_DECLARE(exr);
#endif
#ifdef HAVE_LIBGIF
LOADER_DECLARE(gif);
#endif
#ifdef HAVE_LIBHEIF
LOADER_DECLARE(heif);
#endif
#ifdef HAVE_LIBAVIF
LOADER_DECLARE(avif);
#endif
#ifdef HAVE_LIBJPEG
LOADER_DECLARE(jpeg);
#endif
#ifdef HAVE_LIBJXL
LOADER_DECLARE(jxl);
#endif
#ifdef HAVE_LIBPNG
LOADER_DECLARE(png);
#endif
#ifdef HAVE_LIBRSVG
LOADER_DECLARE(svg);
#endif
#ifdef HAVE_LIBTIFF
LOADER_DECLARE(tiff);
#endif
#ifdef HAVE_LIBWEBP
LOADER_DECLARE(webp);
#endif

// list of available decoders
static const image_decoder decoders[] = {
#ifdef HAVE_LIBJPEG
    &LOADER_FUNCTION(jpeg),
#endif
#ifdef HAVE_LIBPNG
    &LOADER_FUNCTION(png),
#endif
#ifdef HAVE_LIBGIF
    &LOADER_FUNCTION(gif),
#endif
    &LOADER_FUNCTION(bmp),  &LOADER_FUNCTION(pnm),
#ifdef HAVE_LIBWEBP
    &LOADER_FUNCTION(webp),
#endif
#ifdef HAVE_LIBHEIF
    &LOADER_FUNCTION(heif),
#endif
#ifdef HAVE_LIBAVIF
    &LOADER_FUNCTION(avif),
#endif
#ifdef HAVE_LIBRSVG
    &LOADER_FUNCTION(svg),
#endif
#ifdef HAVE_LIBJXL
    &LOADER_FUNCTION(jxl),
#endif
#ifdef HAVE_LIBEXR
    &LOADER_FUNCTION(exr),
#endif
#ifdef HAVE_LIBTIFF
    &LOADER_FUNCTION(tiff),
#endif
    &LOADER_FUNCTION(tga),
};

/**
 * Load image from memory buffer.
 * @param img destination image
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
static enum loader_status image_from_memory(struct image* img,
                                            const uint8_t* data, size_t size)
{
    enum loader_status status = ldr_unsupported;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(decoders) && status == ldr_unsupported; ++i) {
        status = decoders[i](img, data, size);
    }

    img->file_size = size;

#ifdef HAVE_LIBEXIF
    process_exif(img, data, size);
#endif

    return status;
}

/**
 * Load image from file.
 * @param img destination image
 * @param file path to the file to load
 * @return loader status
 */
static enum loader_status image_from_file(struct image* img, const char* file)
{
    enum loader_status status = ldr_ioerror;
    void* data = MAP_FAILED;
    struct stat st;
    int fd;

    // open file and get its size
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        return ldr_ioerror;
    }
    if (fstat(fd, &st) == -1) {
        close(fd);
        return ldr_ioerror;
    }

    // map file to memory
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return ldr_ioerror;
    }

    // load from mapped memory
    status = image_from_memory(img, data, st.st_size);

    munmap(data, st.st_size);
    close(fd);

    return status;
}

/**
 * Load image from stdin.
 * @param img destination image
 * @return loader status
 */
static enum loader_status image_from_stdin(struct image* img)
{
    enum loader_status status = ldr_ioerror;
    uint8_t* data = NULL;
    size_t size = 0;
    size_t capacity = 0;

    while (true) {
        ssize_t rc;

        if (size == capacity) {
            const size_t new_capacity = capacity + 256 * 1024;
            uint8_t* new_buf = realloc(data, new_capacity);
            if (!new_buf) {
                break;
            }
            data = new_buf;
            capacity = new_capacity;
        }

        rc = read(STDIN_FILENO, data + size, capacity - size);
        if (rc == 0) {
            status = image_from_memory(img, data, size);
            break;
        }
        if (rc == -1 && errno != EAGAIN) {
            break;
        }
        size += rc;
    }

    free(data);
    return status;
}

struct image* loader_get_image(const char* source)
{
    struct image* img = NULL;
    enum loader_status status = ldr_ioerror;

    img = image_create();
    if (!img) {
        return NULL;
    }

    // save image source info
    img->file_path = source;
    img->file_name = strrchr(img->file_path, '/');
    if (!img->file_name) {
        img->file_name = img->file_path;
    } else {
        ++img->file_name; // skip slash
    }

    // decode image
    if (strcmp(source, STDIN_FILE_NAME) == 0) {
        status = image_from_stdin(img);
    } else {
        status = image_from_file(img, source);
    }

    if (status != ldr_success) {
        image_free(img);
        img = NULL;
    }

    return img;
}

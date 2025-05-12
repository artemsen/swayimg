// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "../fs.h"
#include "../shellcmd.h"
#include "buildcfg.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Construct function name of loader
#define LOADER_FUNCTION(name) decode_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                      \
    enum image_status LOADER_FUNCTION(name)(struct imgdata * img, \
                                            const uint8_t* data, size_t size)

// declaration of loaders
LOADER_DECLARE(bmp);
LOADER_DECLARE(dicom);
LOADER_DECLARE(farbfeld);
LOADER_DECLARE(pnm);
LOADER_DECLARE(qoi);
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
#ifdef HAVE_LIBSIXEL
LOADER_DECLARE(sixel);
#endif
#ifdef HAVE_LIBRAW
LOADER_DECLARE(raw);
#endif
#ifdef HAVE_LIBWEBP
LOADER_DECLARE(webp);
#endif

// clang-format off
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
    &LOADER_FUNCTION(bmp),
    &LOADER_FUNCTION(pnm),
    &LOADER_FUNCTION(dicom),
    &LOADER_FUNCTION(qoi),
    &LOADER_FUNCTION(farbfeld),
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
#ifdef HAVE_LIBRAW
    &LOADER_FUNCTION(raw),
#endif
#ifdef HAVE_LIBTIFF
    &LOADER_FUNCTION(tiff),
#endif
#ifdef HAVE_LIBSIXEL
    &LOADER_FUNCTION(sixel),
#endif
    &LOADER_FUNCTION(tga) // should be the last one
};
// clang-format on

const char* image_formats(void)
{
    const char* formats = "bmp, pnm, farbfeld, tga, dicom"
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
#ifdef HAVE_LIBSIXEL
                          ", sixel"
#endif
#ifdef HAVE_LIBRAW
                          ", raw"
#endif
        ;
    return formats;
}

/**
 * Load image from memory buffer.
 * @param img destination image
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
static enum image_status load_from_memory(struct image* img,
                                          const uint8_t* data, size_t size)
{
    enum image_status status = imgload_unsupported;
    const size_t dec_num = ARRAY_SIZE(decoders);

    if (img->data) {
        image_clear(img, IMGDATA_ALL);
    } else {
        img->data = calloc(1, sizeof(*img->data));
        if (!img->data) {
            return imgload_unknown;
        }
    }

    for (size_t i = 0; i < dec_num && status == imgload_unsupported; ++i) {
        status = decoders[i](img->data, data, size);
        if (status != imgload_success) {
            image_clear(img, IMGDATA_ALL);
        }
    }

    if (status != imgload_success) {
        image_free(img, IMGDATA_ALL);
        img->data = NULL;
    } else {
        // set common image data parts
        img->file_size = size;

        // name and parent dir
        if (strcmp(img->source, LDRSRC_STDIN) == 0 ||
            strncmp(img->source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
            // special url
            img->name = img->source;
            str_dup("", &img->data->parent);
        } else {
            // filesystem path
            size_t parent_len;
            const char* parent;
            img->name = fs_name(img->source);
            parent = fs_parent(img->source, &parent_len);
            if (parent) {
                str_append(parent, parent_len, &img->data->parent);
            } else {
                str_dup("", &img->data->parent);
            }
        }
    }

    return status;
}

/**
 * Load image from file.
 * @param img destination image
 * @param file path to the file to load
 * @return loader status
 */
static enum image_status load_from_file(struct image* img, const char* file)
{
    enum image_status status = imgload_unknown;
    void* data = MAP_FAILED;
    struct stat st;
    int fd;

    // check file type
    if (stat(file, &st) == -1 || !S_ISREG(st.st_mode)) {
        return imgload_unknown;
    }

    // open file and map it to memory
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        return imgload_unknown;
    }
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return imgload_unknown;
    }

    // load from mapped memory
    status = load_from_memory(img, data, st.st_size);

    munmap(data, st.st_size);
    close(fd);

    return status;
}

/**
 * Load image from stream file (stdin).
 * @param img destination image
 * @param fd file descriptor for read
 * @return loader status
 */
static enum image_status load_from_stream(struct image* img, int fd)
{
    enum image_status status = imgload_unknown;
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

        rc = read(fd, data + size, capacity - size);
        if (rc == 0) {
            status = load_from_memory(img, data, size);
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

/**
 * Load image from stdout printed by external command.
 * @param img destination image
 * @param cmd execution command to get stdout data
 * @return loader status
 */
static enum image_status load_from_exec(struct image* img, const char* cmd)
{
    struct array* out = NULL;
    enum image_status status;
    int rc;

    rc = shellcmd_exec(cmd, &out);
    if (rc == 0 && out) {
        status = load_from_memory(img, out->data, out->size);
    } else {
        status = imgload_unknown;
    }

    arr_free(out);
    return status;
}

enum image_status image_load(struct image* img)
{
    enum image_status status;

    image_free(img, IMGDATA_ALL);

    if (strcmp(img->source, LDRSRC_STDIN) == 0) {
        status = load_from_stream(img, STDIN_FILENO);
    } else if (strncmp(img->source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        status = load_from_exec(img, img->source + LDRSRC_EXEC_LEN);
    } else {
        status = load_from_file(img, img->source);
    }

    return status;
}

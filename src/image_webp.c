// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// WebP image format support
//

#include "config.h"
#ifndef HAVE_LIBWEBP
#error Invalid build configuration
#endif

#include "image_loader.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <webp/decode.h>

// Format name
static const char* const format_name = "WebP";

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

static uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

// implementation of struct loader::load
static cairo_surface_t* load(const char* file, const uint8_t* header, size_t header_len)
{
    int fd;
    void* fdata = MAP_FAILED;
    size_t fsize = 0;
    cairo_surface_t* img = NULL;

    // check signature
    if (header_len < sizeof(signature) || memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    // map file
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        load_error(format_name, errno, "Unable to open file");
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        load_error(format_name, errno, "Unable to get file stat");
        goto done;
    }
    fsize = st.st_size;
    fdata = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fdata == MAP_FAILED) {
        load_error(format_name, errno, "Unable to map file");
        goto done;
    }

    // get image properties
    WebPBitstreamFeatures prop;
    VP8StatusCode rc = WebPGetFeatures(fdata, fsize, &prop);
    if (rc != VP8_STATUS_OK) {
        load_error(format_name, 0, "Unable to get image properties, error %i", rc);
        goto done;
    }

    // create surface
    img = cairo_image_surface_create(
            prop.has_alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
            prop.width, prop.height);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        load_error(format_name, 0, "Unable to create surface: %s",
                   cairo_status_to_string(cairo_surface_status(img)));
        cairo_surface_destroy(img);
        img = NULL;
        goto done;
    }

    uint8_t* data = cairo_image_surface_get_data(img);
    const size_t stride = cairo_image_surface_get_stride(img);
    const size_t len = stride * prop.height;
    if (!WebPDecodeBGRAInto(fdata, fsize, data, len, stride)) {
        load_error(format_name, 0, "Something went wrong");
        cairo_surface_destroy(img);
        img = NULL;
        goto done;
    }

    // handle transparency
    if (prop.has_alpha) {
        for (size_t i = 0; i < len; i += 4 /* argb */) {
            const uint8_t alpha = data[i + 3];
            if (alpha != 0xff) {
                data[i + 0] = multiply_alpha(alpha, data[i + 0]);
                data[i + 1] = multiply_alpha(alpha, data[i + 1]);
                data[i + 2] = multiply_alpha(alpha, data[i + 2]);
            }
        }
    }

    cairo_surface_mark_dirty(img);

done:
    if (fdata != MAP_FAILED) {
        munmap(fdata, fsize);
    }
    close(fd);

    return img;
}

// declare format
const struct loader webp_loader = {
    .format = format_name,
    .load = load
};

// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// WebP image format support
//

#include "config.h"
#ifndef HAVE_LIBWEBP
#error Invalid build configuration
#endif

#include "../image.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <webp/decode.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

static uint8_t multiply_alpha(uint8_t alpha, uint8_t color)
{
    const uint16_t temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

// WebP loader implementation
struct image* load_webp(const char* file, const uint8_t* header, size_t header_len)
{
    int fd;
    void* fdata = MAP_FAILED;
    size_t fsize = 0;
    struct image* img = NULL;

    // check signature
    if (header_len < sizeof(signature) || memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    // map file
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        perror("Unable to open WebP file");
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Unable to get WebP file stat");
        goto done;
    }
    fsize = st.st_size;
    fdata = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fdata == MAP_FAILED) {
        perror("Unable to map WebP file");
        goto done;
    }

    // get image properties
    WebPBitstreamFeatures prop;
    VP8StatusCode status = WebPGetFeatures(fdata, fsize, &prop);
    if (status != VP8_STATUS_OK) {
        fprintf(stderr, "Unable to get WebP image properties: status %i\n", status);
        goto done;
    }

    // format description
    char format[32];
    strcpy(format, "WebP");
    if (prop.format == 1) {
        strcat(format, " lossy");
    } else if (prop.format == 2) {
        strcat(format, " lossless");
    }
    if (prop.has_alpha) {
        strcat(format, " +alpha");
    }
    if (prop.has_animation) {
        strcat(format, " +animation");
    }

    // create image instance
    img = create_image(prop.has_alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
                       prop.width, prop.height,
                       "%s", format);
    if (!img) {
        goto done;
    }

    uint8_t* data = cairo_image_surface_get_data(img->surface);
    const size_t stride = cairo_image_surface_get_stride(img->surface);
    const size_t len = stride * prop.height;
    if (!WebPDecodeBGRAInto(fdata, fsize, data, len, stride)) {
        fprintf(stderr, "Error decoding WebP\n");
        free_image(img);
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

    cairo_surface_mark_dirty(img->surface);

done:
    if (fdata != MAP_FAILED) {
        munmap(fdata, fsize);
    }
    close(fd);

    return img;
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// JPEG image format support
//

#include "config.h"
#ifndef HAVE_LIBJPEG
#error Invalid build configuration
#endif

#include "loader.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>

#include <jpeglib.h>

// Format name
static const char* const format_name = "JPEG";

// JPEG signature
static const uint8_t signature[] = { 0xff, 0xd8 };

struct jpg_error_manager {
    struct jpeg_error_mgr mgr;
    jmp_buf setjmp;
};

static void jpg_error_exit(j_common_ptr jpg)
{
    struct jpg_error_manager* err = (struct jpg_error_manager*)jpg->err;

    char msg[JMSG_LENGTH_MAX] = { 0 };
    (*(jpg->err->format_message))(jpg, msg);
    load_error(format_name, 0, "Decode failed: %s", msg);

    longjmp(err->setjmp, 1);
}

// implementation of struct loader::load
static cairo_surface_t* load(const char* file, const uint8_t* header, size_t header_len)
{
    struct jpeg_decompress_struct jpg;
    struct jpg_error_manager err;

    // check signature
    if (header_len < sizeof(signature) || memcmp(header, signature, sizeof(signature))) {
        return NULL;
    }

    FILE* fd = fopen(file, "rb");
    if (!fd) {
        load_error(format_name, errno, "Unable to open file");
        return NULL;
    }

    jpg.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = jpg_error_exit;
    if (setjmp(err.setjmp)) {
        jpeg_destroy_decompress(&jpg);
        fclose(fd);
        return NULL;
    }

    jpeg_create_decompress(&jpg);
    jpeg_stdio_src(&jpg, fd);
    jpeg_read_header(&jpg, TRUE);
    jpeg_start_decompress(&jpg);
#ifdef LIBJPEG_TURBO_VERSION
    jpg.out_color_space = JCS_EXT_BGRA;
#endif // LIBJPEG_TURBO_VERSION

    // create canvas
    cairo_surface_t* img = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
                                     jpg.output_width, jpg.output_height);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        load_error(format_name, 0, "Unable to create surface: %s",
                   cairo_status_to_string(cairo_surface_status(img)));
        cairo_surface_destroy(img);
        jpeg_destroy_decompress(&jpg);
        fclose(fd);
        return NULL;
    }

    uint8_t* raw = cairo_image_surface_get_data(img);
    const size_t stride = cairo_image_surface_get_stride(img);
    while (jpg.output_scanline < jpg.output_height) {
        uint8_t* line = raw + jpg.output_scanline * stride;
        jpeg_read_scanlines(&jpg, &line, 1);

        // convert grayscale to argb (cairo internal format)
        if (jpg.out_color_components == 1) {
            uint32_t* pixel = (uint32_t*)line;
            for (int x = jpg.output_width - 1; x >= 0; --x) {
                const uint8_t src = *(line + x);
                pixel[x] = 0xff000000 | src << 16 | src << 8 | src;
            }
        }

#ifndef LIBJPEG_TURBO_VERSION
        // convert rgb to argb (cairo internal format)
        if (jpg.out_color_components == 3) {
            uint32_t* pixel = (uint32_t*)line;
            for (int x = jpg.output_width - 1; x >= 0; --x) {
                const uint8_t* src = line + x * 3;
                pixel[x] = 0xff000000 | src[0] << 16 | src[1] << 8 | src[2];
            }
        }
#endif // LIBJPEG_TURBO_VERSION

    }

    cairo_surface_mark_dirty(img);

    jpeg_finish_decompress(&jpg);
    jpeg_destroy_decompress(&jpg);
    fclose(fd);

    return img;
}

// declare format
const struct loader jpeg_loader = {
    .format = format_name,
    .load = load
};

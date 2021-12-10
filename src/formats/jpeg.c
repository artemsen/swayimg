// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// JPEG image format support
//

#include "config.h"
#ifndef HAVE_LIBJPEG
#error Invalid build configuration
#endif

#include "../image.h"

#include <errno.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// must be the last include file (error in jpeglib.h)
#include <jpeglib.h>

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
    fprintf(stderr, "JPEG decode failed: %s\n", msg);

    longjmp(err->setjmp, 1);
}

// JPEG loader implementation
struct image* load_jpeg(const uint8_t* data, size_t size)
{
    struct image* img = NULL;
    struct jpeg_decompress_struct jpg;
    struct jpg_error_manager err;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return NULL;
    }

    jpg.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = jpg_error_exit;
    if (setjmp(err.setjmp)) {
        free_image(img);
        jpeg_destroy_decompress(&jpg);
        return NULL;
    }

    jpeg_create_decompress(&jpg);
    jpeg_mem_src(&jpg, data, size);
    jpeg_read_header(&jpg, TRUE);
    jpeg_start_decompress(&jpg);
#ifdef LIBJPEG_TURBO_VERSION
    jpg.out_color_space = JCS_EXT_BGRA;
#endif // LIBJPEG_TURBO_VERSION

    // create image instance
    img = create_image(CAIRO_FORMAT_RGB24, jpg.output_width, jpg.output_height);
    if (!img) {
        jpeg_destroy_decompress(&jpg);
        return NULL;
    }
    set_image_meta(img, "JPEG %dbit", jpg.out_color_components * 8);

    uint8_t* raw = cairo_image_surface_get_data(img->surface);
    const size_t stride = cairo_image_surface_get_stride(img->surface);
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

    cairo_surface_mark_dirty(img->surface);

    jpeg_finish_decompress(&jpg);
    jpeg_destroy_decompress(&jpg);

    return img;
}

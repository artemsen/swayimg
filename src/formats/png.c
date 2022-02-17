// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

//
// PNG image format support
//

#include "common.h"

#include <cairo/cairo.h>
#include <png.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// PNG memory reader
struct mem_reader {
    const uint8_t* data;
    const size_t size;
    size_t position;
};

// PNG reader callback, see `png_rw_ptr` in png.h
static void png_reader(png_structp png, png_bytep buffer, size_t size)
{
    struct mem_reader* reader = (struct mem_reader*)png_get_io_ptr(png);
    if (reader && reader->position + size < reader->size) {
        memcpy(buffer, reader->data + reader->position, size);
        reader->position += size;
    } else {
        png_error(png, "No data in PNG reader");
    }
}

/**
 * Create array with pointers to image lines.
 * @param[in] surface image surface
 * @return allocated buffer, the caller must free it
 */
static png_bytep* get_lines(cairo_surface_t* surface)
{
    uint8_t* raw = cairo_image_surface_get_data(surface);
    const size_t stride = cairo_image_surface_get_stride(surface);
    const size_t height = cairo_image_surface_get_height(surface);

    png_bytep* lines = malloc(height * sizeof(png_bytep));
    if (!lines) {
        return NULL;
    }
    for (size_t i = 0; i < height; ++i) {
        lines[i] = raw + stride * i;
    }

    return lines;
}

// PNG loader implementation
cairo_surface_t* load_png(const uint8_t* data, size_t size, char* format,
                          size_t format_sz)
{
    cairo_surface_t* surface = NULL;
    png_struct* png = NULL;
    png_info* info = NULL;
    png_bytep* lines = NULL;
    size_t width, height;
    png_byte color_type, bit_depth;

    struct mem_reader reader = {
        .data = data,
        .size = size,
        .position = 0,
    };

    // check signature
    if (png_sig_cmp(data, 0, size) != 0) {
        return NULL;
    }

    // create decoder
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Unable to create PNG decoder\n");
        return NULL;
    }
    info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fprintf(stderr, "Unable to create PNG info\n");
        return NULL;
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        free(lines);
        if (surface) {
            cairo_surface_destroy(surface);
        }
        return NULL;
    }

    // get general image info
    png_set_read_fn(png, &reader, &png_reader);
    png_read_info(png, info);
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);

    // setup decoder
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
        if (bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png);
        }
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    png_set_packing(png);
    png_set_packswap(png);
    png_set_bgr(png);

    // prepare surface and metadata
    surface = create_surface(width, height, true);
    if (!surface) {
        return NULL;
    }
    snprintf(format, format_sz, "PNG %dbit", bit_depth);

    // allocate buffer for pointers to image lines
    lines = get_lines(surface);
    if (!lines) {
        png_destroy_read_struct(&png, &info, NULL);
        cairo_surface_destroy(surface);
        fprintf(stderr, "Not enough memory to decode PNG\n");
        return NULL;
    }

    // read image
    png_read_image(png, lines);

    // handle transparency
    apply_alpha(surface);

    // free resources
    png_destroy_read_struct(&png, &info, NULL);
    free(lines);

    return surface;
}

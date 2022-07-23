// SPDX-License-Identifier: MIT
// PNG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <png.h>
#include <setjmp.h>
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

// PNG loader implementation
enum loader_status decode_png(struct image* ctx, const uint8_t* data,
                              size_t size)
{
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
        return ldr_unsupported;
    }

    // create decoder
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        image_error(ctx, "unable to initialize png decoder");
        return ldr_fmterror;
    }
    info = png_create_info_struct(png);
    if (!info) {
        image_error(ctx, "unable to create png object");
        png_destroy_read_struct(&png, NULL, NULL);
        return ldr_fmterror;
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        free(lines);
        image_deallocate(ctx);
        image_error(ctx, "failed to decode png");
        return ldr_fmterror;
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

    if (!image_allocate(ctx, width, height)) {
        png_destroy_read_struct(&png, &info, NULL);
        return ldr_fmterror;
    }

    // prepare list of pointers to image lines
    lines = malloc(height * sizeof(png_bytep));
    if (!lines) {
        image_error(ctx, "not enough memory");
        png_destroy_read_struct(&png, &info, NULL);
        image_deallocate(ctx);
        return ldr_fmterror;
    }
    for (size_t i = 0; i < height; ++i) {
        lines[i] = (png_bytep)&ctx->data[ctx->width * i];
    }

    // read image
    png_read_image(png, lines);
    image_add_meta(ctx, "Format", "PNG %dbit", bit_depth * 4);
    ctx->alpha = true;

    // free resources
    png_destroy_read_struct(&png, &info, NULL);
    free(lines);

    return ldr_success;
}

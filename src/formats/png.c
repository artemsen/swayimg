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

/**
 * Bind pixmap with PNG decode buffer.
 * @param buffer png buffer to reallocate
 * @param pm pixmap to bind
 * @return false if decode failed
 */
static bool bind_pixmap(png_bytep** buffer, const struct pixmap* pm)
{
    png_bytep* ptr = realloc(*buffer, pm->height * sizeof(png_bytep));

    if (!ptr) {
        return false;
    }

    *buffer = ptr;
    for (uint32_t i = 0; i < pm->height; ++i) {
        ptr[i] = (png_bytep)&pm->data[i * pm->width];
    }

    return true;
}

/**
 * Decode single framed image.
 * @param ctx image context
 * @param png png decoder
 * @param info png image info
 * @return false if decode failed
 */
static bool decode_single(struct image* ctx, png_struct* png, png_info* info)
{
    const uint32_t width = png_get_image_width(png, info);
    const uint32_t height = png_get_image_height(png, info);
    struct pixmap* pm = image_allocate_frame(ctx, width, height);
    png_bytep* rdrows = NULL;

    if (!pm) {
        return false;
    }
    if (!bind_pixmap(&rdrows, pm)) {
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        free(rdrows);
        return false;
    }
    png_read_image(png, rdrows);

    free(rdrows);

    return true;
}

#ifdef PNG_APNG_SUPPORTED
/**
 * Decode single PNG frame.
 * @param ctx image context
 * @param png png decoder
 * @param info png image info
 * @param index number of the frame to load
 * @return true if completed successfully
 */
static bool decode_frame(struct image* ctx, png_struct* png, png_info* info,
                         size_t index)
{
    bool rc = false;
    struct image_frame* frame = &ctx->frames[index];
    png_byte dispose = 0, blend = 0;
    png_uint_16 delay_num = 0, delay_den = 0;
    png_uint_32 x = 0, y = 0, width = 0, height = 0;
    png_bytep* rdrows = NULL;
    struct pixmap frame_pm = { 0, 0, NULL };

    if (setjmp(png_jmpbuf(png))) {
        goto done;
    }

    // get frame params
    if (png_get_valid(png, info, PNG_INFO_acTL)) {
        png_read_frame_head(png, info);
    }
    if (png_get_valid(png, info, PNG_INFO_fcTL)) {
        png_get_next_frame_fcTL(png, info, &width, &height, &x, &y, &delay_num,
                                &delay_den, &dispose, &blend);
    }

    // fixup frame params
    if (width == 0) {
        width = png_get_image_width(png, info);
    }
    if (height == 0) {
        height = png_get_image_height(png, info);
    }
    if (delay_den == 0) {
        delay_den = 100;
    }
    if (delay_num == 0) {
        delay_num = 100;
    }
    frame->duration = (float)delay_num * 1000 / delay_den;

    // decode frame into pixmap
    if (!pixmap_create(&frame_pm, width, height) ||
        !bind_pixmap(&rdrows, &frame_pm)) {
        goto done;
    }
    png_read_image(png, rdrows);

    // handle dispose
    if (dispose == PNG_DISPOSE_OP_PREVIOUS) {
        if (index == 0) {
            dispose = PNG_DISPOSE_OP_BACKGROUND;
        } else if (index + 1 < ctx->num_frames) {
            struct pixmap* next = &ctx->frames[index + 1].pm;
            pixmap_copy(next, 0, 0, &frame->pm, frame->pm.width,
                        frame->pm.height);
        }
    }

    // put frame on final pixmap
    switch (blend) {
        case PNG_BLEND_OP_SOURCE:
            pixmap_copy(&frame->pm, x, y, &frame_pm, frame_pm.width,
                        frame_pm.height);
            break;
        case PNG_BLEND_OP_OVER:
            pixmap_over(&frame->pm, x, y, &frame_pm, frame_pm.width,
                        frame_pm.height);
            break;
    }

    // handle dispose
    if (dispose == PNG_DISPOSE_OP_NONE && index + 1 < ctx->num_frames) {
        struct pixmap* next = &ctx->frames[index + 1].pm;
        pixmap_copy(next, 0, 0, &frame->pm, frame->pm.width, frame->pm.height);
    }

    rc = true;

done:
    pixmap_free(&frame_pm);
    free(rdrows);
    return rc;
}

/**
 * Decode multi framed image.
 * @param ctx image context
 * @param png png decoder
 * @param info png image info
 * @return false if decode failed
 */
static bool decode_multiple(struct image* ctx, png_struct* png, png_info* info)
{
    const uint32_t width = png_get_image_width(png, info);
    const uint32_t height = png_get_image_height(png, info);
    const uint32_t frames = png_get_num_frames(png, info);
    uint32_t index;

    // allocate frames
    if (!image_create_frames(ctx, frames)) {
        return false;
    }
    for (index = 0; index < frames; ++index) {
        struct image_frame* frame = &ctx->frames[index];
        if (!pixmap_create(&frame->pm, width, height)) {
            return false;
        }
    }

    // decode frames
    for (index = 0; index < frames; ++index) {
        if (!decode_frame(ctx, png, info, index)) {
            break;
        }
    }
    if (index != frames) {
        // not all frames were decoded, leave only the first
        for (index = 1; index < frames; ++index) {
            pixmap_free(&ctx->frames[index].pm);
        }
        ctx->num_frames = 1;
    }

    if (png_get_first_frame_is_hidden(png, info) && ctx->num_frames > 1) {
        --ctx->num_frames;
        pixmap_free(&ctx->frames[0].pm);
        memmove(&ctx->frames[0], &ctx->frames[1],
                ctx->num_frames * sizeof(*ctx->frames));
    }

    return true;
}
#endif // PNG_APNG_SUPPORTED

// PNG loader implementation
enum loader_status decode_png(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    png_struct* png = NULL;
    png_info* info = NULL;
    png_byte color_type, bit_depth;
    bool rc;

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
        image_print_error(ctx, "unable to initialize png decoder");
        return ldr_fmterror;
    }
    info = png_create_info_struct(png);
    if (!info) {
        image_print_error(ctx, "unable to create png object");
        png_destroy_read_struct(&png, NULL, NULL);
        return ldr_fmterror;
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        image_print_error(ctx, "failed to decode png");
        return ldr_fmterror;
    }

    // get general image info
    png_set_read_fn(png, &reader, &png_reader);
    png_read_info(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);

    // setup decoder
    if (png_get_interlace_type(png, info) != PNG_INTERLACE_NONE) {
        png_set_interlace_handling(png);
    }
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
    png_set_expand(png);

    png_read_update_info(png, info);

#ifdef PNG_APNG_SUPPORTED
    if (png_get_valid(png, info, PNG_INFO_acTL) &&
        png_get_num_frames(png, info) > 1) {
        rc = decode_multiple(ctx, png, info);
    } else {
        rc = decode_single(ctx, png, info);
    }
#else
    rc = decode_single(ctx, png, info);
#endif // PNG_APNG_SUPPORTED

    if (!rc) {
        image_free_frames(ctx);
    } else {
        image_set_format(ctx, "PNG %dbit", bit_depth * 4);
        ctx->alpha = true;
    }

    // free resources
    png_destroy_read_struct(&png, &info, NULL);

    return rc ? ldr_success : ldr_fmterror;
}

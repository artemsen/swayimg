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
 * Decode current frame.
 * @param png png decoder
 * @param pm destination pixmap
 * @return false if decode failed
 */
static bool read_frame(png_structp png, struct pixmap* pm)
{
    png_bytepp rows = malloc(pm->height * sizeof(png_bytep));
    if (!rows) {
        return false;
    }

    for (uint32_t i = 0; i < pm->height; ++i) {
        rows[i] = (png_bytep)&pm->data[i * pm->width];
    }

    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        return false;
    }

    png_read_image(png, rows);

    free(rows);

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
    return pm && read_frame(png, pm);
}

#ifdef PNG_APNG_SUPPORTED
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
    struct pixmap canvas;
    bool rc = false;

    if (!pixmap_create(&canvas, width, height)) {
        image_print_error(ctx, "not enough memory");
        return false;
    }
    if (!image_create_frames(ctx, frames)) {
        goto done;
    }

    for (uint32_t index = 0; index < frames; ++index) {
        struct image_frame* frame = &ctx->frames[index];
        png_byte dispose_op = PNG_DISPOSE_OP_BACKGROUND;
        png_byte blend_op = PNG_BLEND_OP_SOURCE;
        png_uint_16 delay_num = 100;
        png_uint_16 delay_den = 100;
        png_uint_32 x_offset = 0;
        png_uint_32 y_offset = 0;
        png_uint_32 frame_width = width;
        png_uint_32 frame_height = height;
        struct pixmap framebuf;

        if (!pixmap_create(&frame->pm, width, height)) {
            goto done;
        }

        if (setjmp(png_jmpbuf(png))) {
            free(framebuf.data);
            goto done;
        }

        if (png_get_valid(png, info, PNG_INFO_acTL)) {
            png_read_frame_head(png, info);
        }
        if (png_get_valid(png, info, PNG_INFO_fcTL)) {
            png_get_next_frame_fcTL(png, info, &frame_width, &frame_height,
                                    &x_offset, &y_offset, &delay_num,
                                    &delay_den, &dispose_op, &blend_op);
        }

        if (frame_width + x_offset > width ||
            frame_height + y_offset > height) {
            image_print_error(ctx, "malformed png");
            goto done;
        }

        if (!pixmap_create(&framebuf, frame_width, frame_height)) {
            image_print_error(ctx, "not enough memory for frame buffer");
            goto done;
        }

        if (!read_frame(png, &framebuf)) {
            pixmap_free(&framebuf);
            goto done;
        }

        switch (blend_op) {
            case PNG_BLEND_OP_SOURCE:
                pixmap_copy(&canvas, x_offset, y_offset, &framebuf,
                            framebuf.width, framebuf.height);
                break;
            case PNG_BLEND_OP_OVER:
                pixmap_over(&canvas, x_offset, y_offset, &framebuf,
                            framebuf.width, framebuf.height);
                break;
        }
        memcpy(frame->pm.data, canvas.data,
               canvas.width * canvas.height * sizeof(argb_t));

        if (dispose_op == PNG_DISPOSE_OP_BACKGROUND) {
            pixmap_fill(&canvas, x_offset, y_offset, frame_width, frame_height,
                        0);
        }

        if (delay_den == 0) {
            delay_den = 100;
        }
        frame->duration = (float)delay_num * 1000 / delay_den;

        pixmap_free(&framebuf);
    }

    if (png_get_first_frame_is_hidden(png, info)) {
        --ctx->num_frames;
        pixmap_free(&ctx->frames[0].pm);
        memmove(&ctx->frames[0], &ctx->frames[1],
                ctx->num_frames * sizeof(*ctx->frames));
    }

    rc = true;

done:
    pixmap_free(&canvas);
    return rc;
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

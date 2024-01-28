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

// read image to allocated buffer
static bool do_read_png_image(png_structp png, uint32_t* out, uint32_t width,
                              uint32_t height)
{
    png_bytepp rows;

    rows = png_malloc(png, height * sizeof(png_bytep));
    if (!rows) {
        return false;
    }

    for (uint32_t i = 0; i < height; ++i) {
        rows[i] = (png_bytep)&out[i * width];
    }

    if (setjmp(png_jmpbuf(png))) {
        png_free(png, rows);
        return false;
    }

    png_read_image(png, rows);

    png_free(png, rows);

    return true;
}

// get a single png frame
static bool decode_png_frame(struct image* ctx, png_structp png, png_infop info)
{
    struct pixmap* pm;
    uint32_t width, height;

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);

    pm = image_allocate_frame(ctx, width, height);
    if (!pm) {
        png_destroy_read_struct(&png, &info, NULL);
        return false;
    }

    if (do_read_png_image(png, pm->data, width, height)) {
        return true;
    } else {
        image_free_frames(ctx);
        return false;
    }

    return true;
}

#ifdef PNG_APNG_SUPPORTED
/**
 * Copy frame area of output buffer
 * @param dst frame buffer
 * @param dw frame buffer width
 * @param dh frame buffer height
 * @param src png output buffer
 * @param sw output buffer width
 * @param xo output buffer offset by x
 * @param yo output buffer offset by y
 */
static inline void copy_frame(uint32_t* dst, uint32_t dw, uint32_t dh,
                              uint32_t* src, uint32_t sw, uint32_t xo,
                              uint32_t yo)
{
    for (uint32_t i = 0; i < dh; ++i) {
        memcpy(dst + (i * dw), src + (yo + i) * sw + xo, dw * sizeof(*dst));
    }
}

/**
 * Paste frame area to output buffer
 * @param dst png output buffer
 * @param dw output buffer width
 * @param src frame buffer
 * @param sw frame buffer wudth
 * @param xo output buffer offset by x
 * @param yo output buffer offset by y
 */
static inline void paste_frame(uint32_t* dst, uint32_t dw, uint32_t* src,
                               uint32_t sw, uint32_t sh, uint32_t xo,
                               uint32_t yo)
{
    for (uint32_t i = 0; i < sh; ++i) {
        memcpy(dst + (yo + i) * dw + xo, src + (i * sw), sw * sizeof(*src));
    }
}

/**
 * Clear frame area of output buffer
 * @param dst output buffer
 * @param w output buffer width
 * @param aw clearing area width
 * @param ah clearing area height
 * @param xo output buffer offset by x
 * @param yo output buffer offset by y
 */
static inline void clear_area(uint32_t* dst, uint32_t w, uint32_t aw,
                              uint32_t ah, uint32_t xo, uint32_t yo)
{
    for (uint32_t i = 0; i < ah; ++i) {
        memset(dst + (yo + i) * w + xo, 0, aw * sizeof(*dst));
    }
}

/**
 * Composite two buffers based on its alpha
 * @param dst output buffer
 * @param dw output buffer width
 * @param src frame buffer
 * @param sh frame buffer height
 * @param sw frame buffer width
 * @param xo output buffer offset by x
 * @param yo output buffer offset by y
 */
static inline void blend_over(uint32_t* dst, uint32_t dw, uint32_t* src,
                              uint32_t sw, uint32_t sh, uint32_t xo,
                              uint32_t yo)
{
    for (uint32_t y = 0; y < sh; ++y) {
        for (uint32_t x = 0; x < sw; ++x) {
            uint32_t fg_pixel = *(src + y * sw + x);

            uint8_t fg_alpha = fg_pixel >> 24;

            if (fg_alpha == 255) {
                *(dst + (yo + y) * dw + xo + x) = fg_pixel;
                continue;
            }

            if (fg_alpha != 0) {
                uint32_t bg_pixel = *(dst + (yo + y) * dw + xo + x);
                uint32_t bg_alpha = bg_pixel >> 24;

                if (bg_alpha != 0) {
                    *(dst + (yo + y) * dw + xo + x) = ARGB_ALPHA_BLEND(
                        bg_alpha, fg_alpha, bg_pixel, fg_pixel);
                } else {
                    *(dst + (yo + y) * dw + xo + x) = fg_pixel;
                }
            }
        }
    }
}

// Delete all allocated frames except first one and reduce allocated memory
static void delete_frames_keep_default(struct image* ctx, uint32_t last_frame)
{
    struct image_frame* tmp;

    ctx->num_frames = 1;
    for (uint32_t i = 1; i <= last_frame; ++i) {
        free(ctx->frames[i].data);
    }

    tmp = realloc(ctx->frames, sizeof(*ctx->frames));

    if (tmp) {
        ctx->frames = tmp;
    }
}

// Get png frames sequence or default png image
static bool decode_png_frames(struct image* ctx, png_structp png,
                              png_infop info)
{
    uint32_t* frame = NULL;
    uint32_t* output_buffer = NULL;
    uint32_t* tmp = NULL;
    uint8_t dispose_op, blend_op;
    uint16_t delay_num, delay_den;
    uint32_t num_frames;
    uint32_t width, height;
    uint32_t x_offset, y_offset, sub_width, sub_height;
    bool first_frame_hidden;
    size_t buffer_size;

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    first_frame_hidden = png_get_first_frame_is_hidden(png, info);
    num_frames = png_get_num_frames(png, info);
    buffer_size = width * height * sizeof(*frame);

    output_buffer = malloc(buffer_size);
    if (!output_buffer) {
        image_print_error(ctx, "not enough memory");
        return false;
    }

    if (!image_create_frames(ctx, num_frames)) {
        free(output_buffer);
        return false;
    }

    // buffer must be black and transparent at start
    memset(output_buffer, 0, buffer_size);

    for (uint32_t frame_num = 0; frame_num < num_frames; frame_num++) {
        if (!image_frame_allocate(&ctx->frames[frame_num], width, height)) {
            goto fail;
        }

        if (setjmp(png_jmpbuf(png))) {
            if (frame_num > 0) {
                delete_frames_keep_default(ctx, frame_num);
                goto done;
            } else {
                goto fail;
            }
        }

        if (png_get_valid(png, info, PNG_INFO_acTL)) {
            png_read_frame_head(png, info);
        }

        if (png_get_valid(png, info, PNG_INFO_fcTL)) {
            png_get_next_frame_fcTL(png, info, &sub_width, &sub_height,
                                    &x_offset, &y_offset, &delay_num,
                                    &delay_den, &dispose_op, &blend_op);
        } else {
            sub_width = width;
            sub_height = height;
            x_offset = y_offset = 0;
            delay_num = delay_den = 100;
            dispose_op = PNG_DISPOSE_OP_BACKGROUND;
            blend_op = PNG_BLEND_OP_SOURCE;
        }

        if (sub_width + x_offset > width || sub_height + y_offset > height) {
            image_print_error(ctx, "malformed png");
            goto fail;
        }

        frame = malloc(sub_height * sub_width * sizeof(*frame));
        if (!frame) {
            image_print_error(ctx, "not enough memory for frame buffer");
            goto fail;
        }

        if (!do_read_png_image(png, frame, sub_width, sub_height)) {
            free(frame);
            if (frame_num > 0) {
                delete_frames_keep_default(ctx, frame_num);
                goto done;
            } else {
                goto fail;
            }
        }

        if (dispose_op == PNG_DISPOSE_OP_PREVIOUS) {
            if (frame_num > 0) {
                tmp = malloc(sub_width * sub_height * sizeof(*tmp));
                if (tmp) {
                    copy_frame(tmp, sub_width, sub_height, output_buffer, width,
                               x_offset, y_offset);
                }
            } else {
                dispose_op = PNG_DISPOSE_OP_BACKGROUND;
            }
        }

        switch (blend_op) {
            case PNG_BLEND_OP_SOURCE:
                paste_frame(output_buffer, width, frame, sub_width, sub_height,
                            x_offset, y_offset);
                break;
            case PNG_BLEND_OP_OVER:
                blend_over(output_buffer, width, frame, sub_width, sub_height,
                           x_offset, y_offset);
                break;
            default:
                image_print_error(ctx, "unsupported blend type");
                break;
        }

        switch (dispose_op) {
            case PNG_DISPOSE_OP_PREVIOUS:
                memcpy(ctx->frames[frame_num].data, output_buffer, buffer_size);

                if (tmp) {
                    paste_frame(output_buffer, width, tmp, sub_width,
                                sub_height, x_offset, y_offset);
                    free(tmp);
                    tmp = NULL;
                }
                break;
            case PNG_DISPOSE_OP_BACKGROUND:
                memcpy(ctx->frames[frame_num].data, output_buffer, buffer_size);

                clear_area(output_buffer, width, sub_width, sub_height,
                           x_offset, y_offset);
                break;
            case PNG_DISPOSE_OP_NONE:
                memcpy(ctx->frames[frame_num].data, output_buffer, buffer_size);
                break;
            default:
                image_print_error(ctx, "unsupported disposition type");
                break;
        }

        if (delay_num == 0) {
            ctx->frames[frame_num].duration = 0;
        } else {
            if (delay_den == 0) {
                delay_den = 100;
            }

            ctx->frames[frame_num].duration =
                (size_t)((double)delay_num * 1000.0f / (double)delay_den);
        }

        free(frame);
    }

    if (first_frame_hidden) {
        struct image_frame* tmp;
        ctx->num_frames -= 1;
        free(ctx->frames[0].data);
        memmove(&ctx->frames[0], &ctx->frames[1],
                ctx->num_frames * sizeof(*ctx->frames));

        tmp = realloc(ctx->frames, ctx->num_frames * sizeof(*ctx->frames));

        if (tmp) {
            ctx->frames = tmp;
        }
    }

done:
    free(output_buffer);
    return true;

fail:
    free(output_buffer);
    image_free_frames(ctx);
    return false;
}
#endif

// PNG loader implementation
enum loader_status decode_png(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    png_struct* png = NULL;
    png_info* info = NULL;
    png_byte color_type, bit_depth;
    png_byte interlace;
    int ret;

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
    interlace = png_get_interlace_type(png, info);

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

    if (interlace != PNG_INTERLACE_NONE)
        png_set_interlace_handling(png);

    png_set_expand(png);

    png_read_update_info(png, info);

#ifdef PNG_APNG_SUPPORTED
    if (png_get_valid(png, info, PNG_INFO_acTL)) {
        if (png_get_num_frames(png, info) > 1) {
            ret = decode_png_frames(ctx, png, info);
        } else {
            ret = decode_png_frame(ctx, png, info);
        }
    } else {
        ret = decode_png_frame(ctx, png, info);
    }
#else
    ret = decode_png_frame(ctx, png, info);
#endif

    if (!ret) {
        png_destroy_read_struct(&png, &info, NULL);
        return ldr_fmterror;
    }

    image_set_format(ctx, "PNG %dbit", bit_depth * 4);
    ctx->alpha = true;

    // free resources
    png_destroy_read_struct(&png, &info, NULL);

    return ldr_success;
}

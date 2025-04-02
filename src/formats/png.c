// SPDX-License-Identifier: MIT
// PNG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "png.h"

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
 * Bind pixmap with PNG line-reading decoder.
 * @param pm pixmap to bind
 * @return array of pointers to pixmap data
 */
static png_bytep* bind_pixmap(const struct pixmap* pm)
{
    png_bytep* ptr = malloc(pm->height * sizeof(*ptr));

    if (ptr) {
        for (uint32_t i = 0; i < pm->height; ++i) {
            ptr[i] = (png_bytep)&pm->data[i * pm->width];
        }
    }

    return ptr;
}

/**
 * Decode single framed image.
 * @param img image context
 * @param png png decoder
 * @param info png image info
 * @return false if decode failed
 */
static bool decode_single(struct image* img, png_struct* png, png_info* info)
{
    const uint32_t width = png_get_image_width(png, info);
    const uint32_t height = png_get_image_height(png, info);
    struct pixmap* pm = image_alloc_frame(img, width, height);
    png_bytep* bind;

    if (!pm) {
        return false;
    }

    bind = bind_pixmap(pm);
    if (!bind) {
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        free(bind);
        return false;
    }

    png_read_image(png, bind);

    free(bind);

    return true;
}

#ifdef PNG_APNG_SUPPORTED
/**
 * Decode single PNG frame.
 * @param img image context
 * @param png png decoder
 * @param info png image info
 * @param index number of the frame to load
 * @return true if completed successfully
 */
static bool decode_frame(struct image* img, png_struct* png, png_info* info,
                         size_t index)
{
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    png_uint_32 offset_x = 0;
    png_uint_32 offset_y = 0;
    png_uint_16 delay_num = 0;
    png_uint_16 delay_den = 0;
    png_byte dispose = 0;
    png_byte blend = 0;
    png_bytep* bind;
    struct pixmap frame_png;
    struct image_frame* frame_img = &img->frames[index];

    // get frame params
    if (png_get_valid(png, info, PNG_INFO_acTL)) {
        png_read_frame_head(png, info);
    }
    if (png_get_valid(png, info, PNG_INFO_fcTL)) {
        png_get_next_frame_fcTL(png, info, &width, &height, &offset_x,
                                &offset_y, &delay_num, &delay_den, &dispose,
                                &blend);
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

    // allocate frame buffer and bind it to png reader
    if (!pixmap_create(&frame_png, width, height)) {
        return false;
    }
    bind = bind_pixmap(&frame_png);
    if (!bind) {
        pixmap_free(&frame_png);
        return false;
    }

    // decode frame into pixmap
    if (setjmp(png_jmpbuf(png))) {
        pixmap_free(&frame_png);
        free(bind);
        return false;
    }
    png_read_image(png, bind);

    // handle dispose
    if (dispose == PNG_DISPOSE_OP_PREVIOUS) {
        if (index == 0) {
            dispose = PNG_DISPOSE_OP_BACKGROUND;
        } else if (index + 1 < img->num_frames) {
            struct pixmap* next = &img->frames[index + 1].pm;
            pixmap_copy(&frame_img->pm, next, 0, 0, false);
        }
    }

    // put frame on final pixmap
    pixmap_copy(&frame_png, &frame_img->pm, offset_x, offset_y,
                blend == PNG_BLEND_OP_OVER);

    // handle dispose
    if (dispose == PNG_DISPOSE_OP_NONE && index + 1 < img->num_frames) {
        struct pixmap* next = &img->frames[index + 1].pm;
        pixmap_copy(&frame_img->pm, next, 0, 0, false);
    }

    // calc frame duration in milliseconds
    frame_img->duration = (float)delay_num * 1000 / delay_den;

    pixmap_free(&frame_png);
    free(bind);

    return true;
}

/**
 * Decode multi framed image.
 * @param img image context
 * @param png png decoder
 * @param info png image info
 * @return false if decode failed
 */
static bool decode_multiple(struct image* img, png_struct* png, png_info* info)
{
    const uint32_t width = png_get_image_width(png, info);
    const uint32_t height = png_get_image_height(png, info);
    const uint32_t frames = png_get_num_frames(png, info);
    uint32_t index;

    // allocate frames
    if (!image_alloc_frames(img, frames)) {
        return false;
    }
    for (index = 0; index < frames; ++index) {
        struct image_frame* frame = &img->frames[index];
        if (!pixmap_create(&frame->pm, width, height)) {
            return false;
        }
    }

    // decode frames
    for (index = 0; index < frames; ++index) {
        if (!decode_frame(img, png, info, index)) {
            break;
        }
    }
    if (index != frames) {
        // not all frames were decoded, leave only the first
        for (index = 1; index < frames; ++index) {
            pixmap_free(&img->frames[index].pm);
        }
        img->num_frames = 1;
    }

    if (png_get_first_frame_is_hidden(png, info) && img->num_frames > 1) {
        --img->num_frames;
        pixmap_free(&img->frames[0].pm);
        memmove(&img->frames[0], &img->frames[1],
                img->num_frames * sizeof(*img->frames));
    }

    return true;
}
#endif // PNG_APNG_SUPPORTED

// PNG loader implementation
enum image_status decode_png(struct image* img, const uint8_t* data,
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
        return imgload_unsupported;
    }

    // create decoder
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        return imgload_fmterror;
    }
    info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        return imgload_fmterror;
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return imgload_fmterror;
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
        rc = decode_multiple(img, png, info);
    } else {
        rc = decode_single(img, png, info);
    }
#else
    rc = decode_single(img, png, info);
#endif // PNG_APNG_SUPPORTED

    // read text info
    if (rc) {
        png_text* txt;
        int total;
        if (png_get_text(png, info, &txt, &total)) {
            for (int i = 0; i < total; ++i) {
                image_add_meta(img, txt[i].key, "%s", txt[i].text);
            }
        }
    }

    if (!rc) {
        image_free(img, IMGFREE_FRAMES);
    } else {
        image_set_format(img, "PNG %dbit", bit_depth * 4);
        img->alpha = true;
    }

    // free resources
    png_destroy_read_struct(&png, &info, NULL);

    return rc ? imgload_success : imgload_fmterror;
}

bool export_png(const struct pixmap* pm, const struct image_info* info,
                const char* path)
{
    png_struct* png = NULL;
    png_info* png_inf = NULL;
    png_bytep* bind = NULL;
    FILE* file;

    file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    // create encoder
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(file);
        return false;
    }
    png_inf = png_create_info_struct(png);
    if (!png_inf) {
        png_destroy_write_struct(&png, NULL);
        fclose(file);
        return false;
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &png_inf);
        fclose(file);
        return false;
    }

    png_init_io(png, file);

    // setup output: 8bit RGBA
    png_set_IHDR(png, png_inf, pm->width, pm->height, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_bgr(png);

    // save meta info as text
    if (info) {
        const size_t info_sz = list_size(&info->list);
        png_text* txt = calloc(1, info_sz * sizeof(png_text));
        if (txt) {
            size_t i = 0;
            list_for_each(info, const struct image_info, it) {
                txt[i].key = it->key;
                txt[i].text = it->value;
                ++i;
            }
            png_set_text(png, png_inf, txt, info_sz);
            free(txt);
        }
    }

    png_write_info(png, png_inf);

    // encode image
    bind = bind_pixmap(pm);
    if (!bind) {
        png_destroy_write_struct(&png, &png_inf);
        fclose(file);
        return false;
    }
    png_write_image(png, bind);
    png_write_end(png, NULL);

    // free resources
    png_destroy_write_struct(&png, &png_inf);
    free(bind);
    fclose(file);

    return true;
}

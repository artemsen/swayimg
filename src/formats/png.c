// SPDX-License-Identifier: MIT
// PNG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "png.h"

#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

// PNG memory reader
struct mem_reader {
    const uint8_t* data;
    const size_t size;
    size_t position;
};

// PNG reader callback, see `png_rw_ptr` in png.h
static void mem_reader(png_structp png, png_bytep buffer, size_t size)
{
    struct mem_reader* reader = (struct mem_reader*)png_get_io_ptr(png);
    if (reader && reader->position + size < reader->size) {
        memcpy(buffer, reader->data + reader->position, size);
        reader->position += size;
    } else {
        png_error(png, "No data in PNG reader");
    }
}

// PNG writer callback, see `png_rw_ptr` in png.h
static void mem_writer(png_structp png, png_bytep buffer, size_t size)
{
    int fd = *(int*)png_get_io_ptr(png);
    while (size) {
        const ssize_t rcv = write(fd, buffer, size);
        if (rcv == -1) {
            if (errno != EINTR) {
                return;
            }
            continue;
        }
        size -= rcv;
        buffer = ((uint8_t*)buffer) + rcv;
    }
}

// PNG flusher callback, see `png_flush_ptr` in png.h
static void flush_stub(__attribute__((unused)) png_structp png) { }

/**
 * Create and lock file.
 * @param path path to the file
 * @return file descriptor or error code < 0
 */
static int create_locked(const char* path)
{
    const struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_pid = getpid(),
    };
    int fd;

    // open and lock file
    fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        return -errno;
    }

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        const int rc = -errno;
        close(fd);
        fd = rc;
    }

    return fd;
}

/**
 * Unlock and close file.
 * @param fd file descriptor
 */
static void close_locked(int fd)
{
    const struct flock unlock = {
        .l_type = F_UNLCK,
        .l_whence = SEEK_SET,
        .l_pid = getpid(),
    };

    fcntl(fd, F_SETLK, &unlock);
    close(fd);
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
static bool decode_single(struct imgdata* img, png_struct* png, png_info* info)
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
static bool decode_frame(struct imgdata* img, png_struct* png, png_info* info,
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
    struct imgframe* frame_img = arr_nth(img->frames, index);

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
        } else if (index + 1 < img->frames->size) {
            struct imgframe* next = arr_nth(img->frames, index + 1);
            pixmap_copy(&frame_img->pm, &next->pm, 0, 0, false);
        }
    }

    // put frame on final pixmap
    pixmap_copy(&frame_png, &frame_img->pm, offset_x, offset_y,
                blend == PNG_BLEND_OP_OVER);

    // handle dispose
    if (dispose == PNG_DISPOSE_OP_NONE && index + 1 < img->frames->size) {
        struct imgframe* next = arr_nth(img->frames, index + 1);
        pixmap_copy(&frame_img->pm, &next->pm, 0, 0, false);
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
static bool decode_multiple(struct imgdata* img, png_struct* png,
                            png_info* info)
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
        struct imgframe* frame = arr_nth(img->frames, index);
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
            struct imgframe* frame = arr_nth(img->frames, index);
            pixmap_free(&frame->pm);
        }
        arr_resize(img->frames, 1);
    }

    if (png_get_first_frame_is_hidden(png, info) && img->frames->size > 1) {
        struct imgframe* first = arr_nth(img->frames, 0);
        pixmap_free(&first->pm);
        arr_remove(img->frames, 0);
    }

    return true;
}
#endif // PNG_APNG_SUPPORTED

// PNG loader implementation
enum image_status decode_png(struct imgdata* img, const uint8_t* data,
                             size_t size)
{
    png_struct* png;
    png_info* info;
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
    png_set_read_fn(png, &reader, &mem_reader);
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

    if (rc) {
        // read text info
        png_text* txt;
        int total;
        if (png_get_text(png, info, &txt, &total)) {
            for (int i = 0; i < total; ++i) {
                image_add_info(img, txt[i].key, "%s", txt[i].text);
            }
        }

        image_set_format(img, "PNG %dbit", bit_depth * 4);
        img->alpha = true;
    }

    png_destroy_read_struct(&png, &info, NULL);

    return rc ? imgload_success : imgload_fmterror;
}

int export_png(const struct pixmap* pm, const struct array* info,
               const char* path)
{
    png_struct* png;
    png_info* png_inf;
    png_bytep* bind;
    int fd;

    // open and lock file
    fd = create_locked(path);
    if (fd < 0) {
        return -fd;
    }

    // create encoder
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        close_locked(fd);
        return EFAULT;
    }
    png_inf = png_create_info_struct(png);
    if (!png_inf) {
        close_locked(fd);
        png_destroy_write_struct(&png, NULL);
        return EFAULT;
    }

    // bind buffer with image
    bind = bind_pixmap(pm);
    if (!bind) {
        close_locked(fd);
        png_destroy_write_struct(&png, &png_inf);
        return ENOMEM;
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        close_locked(fd);
        png_destroy_write_struct(&png, &png_inf);
        free(bind);
        return EILSEQ;
    }

    // setup output: 8bit RGBA
    png_set_IHDR(png, png_inf, pm->width, pm->height, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_bgr(png);

    // save meta info as text
    if (info) {
        png_text* txt = calloc(1, info->size * sizeof(png_text));
        if (txt) {
            for (size_t i = 0; i < info->size; ++i) {
                const struct imginfo* inf = arr_nth((struct array*)info, i);
                txt[i].key = inf->key;
                txt[i].text = inf->value;
            }
            png_set_text(png, png_inf, txt, info->size);
            free(txt);
        }
    }

    // encode png
    png_set_write_fn(png, &fd, &mem_writer, &flush_stub);
    png_write_info(png, png_inf);
    png_write_image(png, bind);
    png_write_end(png, NULL);

    // free resources
    close_locked(fd);
    png_destroy_write_struct(&png, &png_inf);
    free(bind);

    return 0;
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <setjmp.h>
#include <cairo/cairo.h>

#define HEADER_SIZE 8

////////////////////////////////////////////////////////////////////////////////
// PNG image support
////////////////////////////////////////////////////////////////////////////////
cairo_surface_t* load_png(const char* file, const uint8_t* header)
{
    // check signature
    static const uint8_t png_sig[] = { 0x89, 0x50, 0x4E, 0x47,
                                       0x0D, 0x0A, 0x1A, 0x0A };
    if (memcmp(header, png_sig, sizeof(png_sig))) {
        return NULL; // not a PNG file
    }

    // load png
    cairo_surface_t* img = cairo_image_surface_create_from_png(file);
    cairo_status_t sst = cairo_surface_status(img);
    if (sst != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(img);
        img = NULL;
        fprintf(stderr, "Unable to decode PNG file %s: %s\n",
                file, cairo_status_to_string(sst));
    }

    return img;
}

////////////////////////////////////////////////////////////////////////////////
// JPEG image support
////////////////////////////////////////////////////////////////////////////////
#ifdef HAVE_LIBJPEG
#include <jpeglib.h>
struct jpg_error_manager {
    struct jpeg_error_mgr mgr;
    jmp_buf setjmp;
};

void jpg_error_exit(j_common_ptr jpg)
{
    struct jpg_error_manager* err = (struct jpg_error_manager*)jpg->err;
    longjmp(err->setjmp, 1);
}

cairo_surface_t* load_jpg(const char* file, const uint8_t* header)
{
    struct jpeg_decompress_struct jpg;
    struct jpg_error_manager err;

    // check signature
    static const uint8_t jpg_sig[] = { 0xff, 0xd8 };
    if (memcmp(header, jpg_sig, sizeof(jpg_sig))) {
        return NULL; // not a JPEG file
    }

    FILE* fd = fopen(file, "rb");
    if (!fd) {
        fprintf(stderr, "Unable to open file %s: [%i] %s\n", file, errno,
                strerror(errno));
        return NULL;
    }

    jpg.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = jpg_error_exit;
    if (setjmp(err.setjmp)) {
        fprintf(stderr, "Unable to decode JPEG\n");
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
        fprintf(stderr, "Unable to create cairo surface\n");
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

        // convert grayscale to argb
        if (jpg.out_color_components == 1) {
            uint32_t* pixel = (uint32_t*)line;
            for (int x = jpg.output_width - 1; x >= 0; --x) {
                const uint8_t src = *(line + x);
                pixel[x] = 0xff000000 | src << 16 | src << 8 | src;
            }
        }

#ifndef LIBJPEG_TURBO_VERSION
        // convert rgb to argb
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
#endif // HAVE_LIBJPEG

////////////////////////////////////////////////////////////////////////////////
// GIF image support
////////////////////////////////////////////////////////////////////////////////
#ifdef HAVE_LIBGIF
#include <gif_lib.h>
cairo_surface_t* load_gif(const char* file, const uint8_t* header)
{
    cairo_surface_t* img = NULL;

    // check signature
    static const uint8_t gif_sig[] = { 'G', 'I', 'F' };
    if (memcmp(header, gif_sig, sizeof(gif_sig))) {
        return NULL; // not a GIF file
    }

    int err;
    GifFileType* gif = DGifOpenFileName(file, &err);
    if (!gif) {
        fprintf(stderr, "GIF decoder error: %s\n", GifErrorString(err));
        return NULL;
    }

    // decode with high-level API
    if (DGifSlurp(gif) != GIF_OK) {
        fprintf(stderr, "GIF decoder error: %s\n", GifErrorString(gif->Error));
        goto done;
    }
    if (!gif->SavedImages) {
        fprintf(stderr, "GIF decoder error: no saved images\n");
        goto done;
    }

    // create canvas
    img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, gif->SWidth, gif->SHeight);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Unable to create cairo surface\n");
        cairo_surface_destroy(img);
        img = NULL;
        goto done;
    }

    // we don't support animation, show the first frame only
    const GifImageDesc* frame = &gif->SavedImages->ImageDesc;
    const GifColorType* colors = gif->SColorMap ? gif->SColorMap->Colors :
                                                  frame->ColorMap->Colors;
    uint32_t* base = (uint32_t*)(cairo_image_surface_get_data(img) +
                     frame->Top * cairo_image_surface_get_stride(img));
    for (int y = 0; y < frame->Height; ++y) {
        uint32_t* pixel = base + y * gif->SWidth + frame->Left;
        const uint8_t* raster = &gif->SavedImages->RasterBits[y * gif->SWidth];
        for (int x = 0; x < frame->Width; ++x) {
            const uint8_t color = raster[x];
            if (color != gif->SBackGroundColor) {
                const GifColorType* rgb = &colors[color];
                *pixel = 0xff000000 |
                    rgb->Red << 16 | rgb->Green << 8 | rgb->Blue;
            }
            ++pixel;
        }
    }

    cairo_surface_mark_dirty(img);

done:
    DGifCloseFile(gif, NULL);
    return img;
}
#endif // HAVE_LIBGIF

/**
 * Image loader function.
 * @param[in] file path to the image file
 * @param[in] header header data (HEADER_SIZE bytes)
 * @return image surface or NULL if decode failed
 */
typedef cairo_surface_t* (*load)(const char* file, const uint8_t* header);

// Loaders
static const load loaders[] = {
#ifdef HAVE_LIBJPEG
    load_jpg,
#endif // HAVE_LIBJPEG
    load_png,
#ifdef HAVE_LIBGIF
    load_gif,
#endif // HAVE_LIBGIF
};

/**
 * Load image from file.
 * @param[in] file path to the file to load
 * @return image surface or NULL if file format is not supported
 */
cairo_surface_t* load_image(const char* file)
{
    cairo_surface_t* img = NULL;

    // read header
    uint8_t header[HEADER_SIZE];
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open file %s: [%i] %s\n", file, errno,
                strerror(errno));
        return NULL;
    }
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        fprintf(stderr, "Unable to read file %s: [%i] %s\n", file, errno,
                strerror(errno));
        close(fd);
        return NULL;
    }
    close(fd);

    // decode
    for (size_t i = 0; !img && i < sizeof(loaders) / sizeof(loaders[0]); ++i) {
        img = loaders[i](file, header);
    }

    if (!img) {
        fprintf(stderr, "Unsupported format: %s\n", file);
    }

    return img;
}

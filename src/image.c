// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
        fprintf(stderr, "Unable to decode PNG file: %s\n", cairo_status_to_string(sst));
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
        const int ec = errno;
        fprintf(stderr, "Unable to open file: [%i] %s\n", ec, strerror(ec));
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

////////////////////////////////////////////////////////////////////////////////
// BMP image support
////////////////////////////////////////////////////////////////////////////////
cairo_surface_t* load_bmp(const char* file, const uint8_t* header)
{
    cairo_surface_t* img = NULL;
    uint8_t* buffer = NULL;
    uint32_t* color_map = NULL;
    size_t color_map_sz = 0;

    struct __attribute__((__packed__)) bmp_file_header {
        uint16_t type;
        uint32_t file_size;
        uint32_t reserved;
        uint32_t offset;
    };

    struct __attribute__((__packed__)) bmp_info_header {
        uint32_t hdr_size;
        uint32_t width;
        uint32_t height;
        uint16_t planes;
        uint16_t bpp;
        uint32_t compression;
        uint32_t img_size;
        uint32_t hres;
        uint32_t vres;
        uint32_t clr_palette;
        uint32_t clr_important;
    };

    // check signature
    static const uint8_t bmp_sig[] = { 0x42, 0x4d };
    if (memcmp(header, bmp_sig, sizeof(bmp_sig))) {
        return NULL; // not a Windows BMP file
    }

    const int fd = open(file, O_RDONLY);
    if (fd == -1) {
        const int ec = errno;
        fprintf(stderr, "Unable to open file: [%i] %s\n", ec, strerror(ec));
        return NULL;
    }

    struct bmp_file_header bmp_fh;
    struct bmp_info_header bmp_ih;
    if (read(fd, &bmp_fh, sizeof(bmp_fh)) != sizeof(bmp_fh) ||
        read(fd, &bmp_ih, sizeof(bmp_ih)) != sizeof(bmp_ih)) {
        const int ec = errno;
        fprintf(stderr, "Unable to read file: [%i] %s\n", ec, strerror(ec));
        goto done;
    }

    if (bmp_ih.planes != 1) {
        fprintf(stderr, "Too many planes: %i\n", bmp_ih.planes);
        goto done;
    }
    if (bmp_ih.compression != 0) {
        fprintf(stderr, "BMP compression is not supported: %i\n", bmp_ih.compression);
        goto done;
    }

    if (bmp_ih.clr_palette) {
        color_map_sz = bmp_ih.clr_palette * sizeof(uint32_t);
        color_map = malloc(color_map_sz);
        if (!color_map) {
            fprintf(stderr, "Not enough memory\n");
            goto done;
        }
        if (read(fd, color_map, color_map_sz) != (ssize_t)color_map_sz) {
            const int ec = errno;
            fprintf(stderr, "Unable to read file: [%i] %s\n", ec, strerror(ec));
            goto done;
        }
    }

    // read pixel data
    if (lseek(fd, bmp_fh.offset, SEEK_SET) == -1) {
        const int ec = errno;
        fprintf(stderr, "Unable to set position: [%i] %s\n", ec, strerror(ec));
        goto done;
    }
    const size_t stride = 4 * ((bmp_ih.width * bmp_ih.bpp + 31) / 32);
    const size_t size = bmp_ih.height * stride;
    buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Not enough memory\n");
        goto done;
    }
    if (read(fd, buffer, size) != (ssize_t)size) {
        const int ec = errno;
        fprintf(stderr, "Unable to read file: [%i] %s\n", ec, strerror(ec));
        goto done;
    }

    // create canvas
    cairo_format_t fmt;
    switch (bmp_ih.bpp) {
        case 8:
        case 24:
            fmt = CAIRO_FORMAT_RGB24;
            break;
        case 32:
            fmt = CAIRO_FORMAT_ARGB32;
            break;
        default:
            fprintf(stderr, "Unsupported pixel size: %i bits\n", bmp_ih.bpp);
            goto done;
    }
    img = cairo_image_surface_create(fmt, bmp_ih.width, bmp_ih.height);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Unable to create cairo surface\n");
        cairo_surface_destroy(img);
        img = NULL;
        goto done;
    }

    // flip and convert to argb (cairo internal format)
    uint32_t* pixel = (uint32_t*)cairo_image_surface_get_data(img);
    for (ssize_t y = bmp_ih.height - 1; y >= 0; --y) {
        uint8_t* src_y = buffer + y * stride;
        for (size_t x = 0; x < bmp_ih.width; ++x) {
            uint8_t* src_x = src_y + x * (bmp_ih.bpp / 8);
            uint8_t a = 0xff, r = 0, g = 0, b = 0;
            if (bmp_ih.bpp == 32) {
                a = src_x[3];
                r = src_x[2];
                g = src_x[1];
                b = src_x[0];
            }
            else if (bmp_ih.bpp == 24) {
                r = src_x[2];
                g = src_x[1];
                b = src_x[0];
            }
            else if (*src_x < color_map_sz) {
                const uint8_t* clr = (uint8_t*)&color_map[*src_x];
                r = clr[2];
                g = clr[1];
                b = clr[0];
            }
            *pixel = a << 24 | r << 16 | g << 8 | b;
            ++pixel;
        }
    }

    cairo_surface_mark_dirty(img);

done:
    if (color_map) {
        free(color_map);
    }
    if (buffer) {
        free(buffer);
    }
    close(fd);

    return img;
}

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
    load_bmp,
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
    const int fd = open(file, O_RDONLY);
    if (fd == -1) {
        const int ec = errno;
        fprintf(stderr, "Unable to open file: [%i] %s\n", ec, strerror(ec));
        return NULL;
    }
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
        const int ec = errno;
        fprintf(stderr, "Unable to read file: [%i] %s\n", ec, strerror(ec));
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

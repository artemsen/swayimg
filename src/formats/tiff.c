// SPDX-License-Identifier: MIT
// TIFF format decoder.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <string.h>
#include <tiffio.h>

// TIFF signatures
static const uint8_t signature1[] = { 0x49, 0x49, 0x2a, 0x00 };
static const uint8_t signature2[] = { 0x4d, 0x4d, 0x00, 0x2a };

// Size of buffer for error messages, see libtiff for details
#define LIBTIFF_ERRMSG_SZ 1024

// TIFF memory reader
struct mem_reader {
    const uint8_t* data;
    size_t size;
    size_t position;
};

// TIFF memory reader: TIFFReadWriteProc
static tmsize_t tiff_read(thandle_t data, void* buffer, tmsize_t size)
{
    struct mem_reader* mr = data;
    if (mr->position >= mr->size) {
        return 0;
    }
    if (mr->position + size > mr->size) {
        size = mr->size - mr->position;
    }
    memcpy(buffer, mr->data + mr->position, size);
    mr->position += size;
    return size;
}

// TIFF memory reader: TIFFReadWriteProc
static tmsize_t tiff_write(thandle_t data, void* buffer, tmsize_t size)
{
    return 0;
}

// TIFF memory reader: TIFFSeekProc
static toff_t tiff_seek(thandle_t data, toff_t off, int xxx)
{
    struct mem_reader* mr = data;
    if (off < mr->size) {
        mr->position = off;
    }
    return mr->position;
}

// TIFF memory reader: TIFFCloseProc
static int tiff_close(thandle_t data)
{
    return 0;
}

// TIFF memory reader: TIFFSizeProc
static toff_t tiff_size(thandle_t data)
{
    struct mem_reader* mr = data;
    return mr->size;
}

// TIFF memory reader: TIFFMapFileProc
static int tiff_map(thandle_t data, void** base, toff_t* size)
{
    struct mem_reader* mr = data;
    *base = (uint8_t*)mr->data;
    *size = mr->size;
    return 0;
}

// TIFF memory reader: TIFFUnmapFileProc
static void tiff_unmap(thandle_t data, void* base, toff_t size) { }

// TIFF loader implementation
enum loader_status decode_tiff(struct image* ctx, const uint8_t* data,
                               size_t size)
{
    TIFF* tiff;
    TIFFRGBAImage timg;
    argb_t* buffer = NULL;
    char err[LIBTIFF_ERRMSG_SZ];
    struct mem_reader reader;

    // check signature
    if (size < sizeof(signature1) || size < sizeof(signature2) ||
        (memcmp(data, signature1, sizeof(signature1)) &&
         memcmp(data, signature2, sizeof(signature2)))) {
        return ldr_unsupported;
    }

    reader.data = data;
    reader.size = size;
    reader.position = 0;

    TIFFSetErrorHandler(NULL);
    TIFFSetWarningHandler(NULL);

    tiff = TIFFClientOpen("", "r", &reader, tiff_read, tiff_write, tiff_seek,
                          tiff_close, tiff_size, tiff_map, tiff_unmap);
    if (!tiff) {
        image_error(ctx, "unable to open tiff decoder\n");
        return ldr_fmterror;
    }

    *err = 0;
    if (!TIFFRGBAImageBegin(&timg, tiff, 0, err)) {
        image_error(ctx, "unable to initialize tiff decoder: %s\n", err);
        goto fail;
    }

    buffer = image_allocate(ctx, timg.width, timg.height);
    if (!buffer) {
        goto fail;
    }
    if (!TIFFRGBAImageGet(&timg, buffer, timg.width, timg.height)) {
        image_error(ctx, "unable to decode tiff\n");
        goto fail;
    }

    // convert ABGR -> ARGB
    for (size_t i = 0; i < ctx->width * ctx->height; ++i) {
        buffer[i] = ARGB_FROM_ABGR(buffer[i]);
    }

    if (timg.orientation == ORIENTATION_TOPLEFT) {
        image_flip_vertical(ctx);
    }
    ctx->alpha = true;
    image_add_meta(ctx, "Format", "TIFF %dbpp",
                   timg.bitspersample * timg.samplesperpixel);

    TIFFRGBAImageEnd(&timg);
    TIFFClose(tiff);
    return ldr_success;

fail:
    image_deallocate(ctx);
    TIFFRGBAImageEnd(&timg);
    TIFFClose(tiff);
    return ldr_fmterror;
}

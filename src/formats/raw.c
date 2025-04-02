// SPDX-License-Identifier: MIT
// Raw format decoder.

#include "loader.h"

#include <stdio.h>
#include <stdlib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <libraw.h>
#pragma GCC diagnostic pop

// Raw loader implementation
enum image_status decode_raw(struct image* img, const uint8_t* data,
                             size_t size)
{
    libraw_data_t* decoder = NULL;
    libraw_processed_image_t* raw_img = NULL;
    size_t pos, end;
    int rc;

    decoder = libraw_init(0);
    if (!decoder) {
        return imgload_unsupported;
    }

    rc = libraw_open_buffer(decoder, data, size);
    if (rc != LIBRAW_SUCCESS) {
        goto fail;
    }

    rc = libraw_unpack(decoder);
    if (rc != LIBRAW_SUCCESS) {
        goto fail;
    }

    decoder->params.output_bps = 8;

    rc = libraw_dcraw_process(decoder);
    if (rc != LIBRAW_SUCCESS) {
        goto fail;
    }

    raw_img = libraw_dcraw_make_mem_image(decoder, NULL);
    if (!raw_img) {
        goto fail;
    }

    if (raw_img->type != LIBRAW_IMAGE_BITMAP || raw_img->colors != 3 ||
        raw_img->bits != 8) {
        goto fail;
    }

    if (!image_alloc_frame(img, raw_img->width, raw_img->height)) {
        goto fail;
    }

    pos = 0;
    end = raw_img->width * raw_img->height;
    while (pos < end) {
        const uint8_t* src = &raw_img->data[pos * 3];
        argb_t* dst = &img->frames[0].pm.data[pos];
        *dst = ARGB(0xff, src[0], src[1], src[2]);
        ++pos;
    }

    image_set_format(img, "RAW");

    libraw_dcraw_clear_mem(raw_img);
    libraw_close(decoder);
    return imgload_success;

fail:
    image_free(img, IMGFREE_FRAMES);
    if (raw_img) {
        libraw_dcraw_clear_mem(raw_img);
    }
    libraw_close(decoder);
    return imgload_unsupported;
}

// SPDX-License-Identifier: MIT
// JPEG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "buildcfg.h"
#include "exif.h"
#include "loader.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

// depends on stdio.h, uses FILE but doesn't include the header
#include <jpeglib.h>

// JPEG signature
static const uint8_t signature[] = { 0xff, 0xd8 };

struct jpg_error_manager {
    struct jpeg_error_mgr mgr;
    jmp_buf setjmp;
};

static void jpg_error_exit(j_common_ptr jpg)
{
    struct jpg_error_manager* err = (struct jpg_error_manager*)jpg->err;
    char msg[JMSG_LENGTH_MAX] = { 0 };
    (*(jpg->err->format_message))(jpg, msg);
    longjmp(err->setjmp, 1);
}

// JPEG loader implementation
enum image_status decode_jpeg(struct imgdata* img, const uint8_t* data,
                              size_t size)
{
    struct pixmap* pm;
    struct jpeg_decompress_struct jpg;
    struct jpg_error_manager err;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return imgload_unsupported;
    }

    jpg.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = jpg_error_exit;
    if (setjmp(err.setjmp)) {
        jpeg_destroy_decompress(&jpg);
        return imgload_fmterror;
    }

    jpeg_create_decompress(&jpg);
    jpeg_mem_src(&jpg, data, size);
    jpeg_read_header(&jpg, TRUE);

#ifdef LIBJPEG_TURBO_VERSION
    jpg.out_color_space = JCS_EXT_BGRA;
#endif // LIBJPEG_TURBO_VERSION
    jpeg_start_decompress(&jpg);

    pm = image_alloc_frame(img, pixmap_xrgb, jpg.output_width,
                           jpg.output_height);
    if (!pm) {
        jpeg_destroy_decompress(&jpg);
        return imgload_fmterror;
    }

    while (jpg.output_scanline < jpg.output_height) {
        uint8_t* line = (uint8_t*)&pm->data[jpg.output_scanline * pm->width];
        jpeg_read_scanlines(&jpg, &line, 1);

        // convert grayscale/rgb to argb
        if (jpg.out_color_components == 1 || jpg.out_color_components == 3) {
            uint32_t* pixel = (uint32_t*)line;
            for (int x = jpg.output_width - 1; x >= 0; --x) {
                const uint8_t* src = line + x * jpg.out_color_components;
                switch (jpg.out_color_components) {
                    case 1:
                        pixel[x] = ARGB(ARGB_MAX_COLOR, src[0], src[0], src[0]);
                        break;
                    case 3:
                        pixel[x] = ARGB(ARGB_MAX_COLOR, src[0], src[1], src[2]);
                        break;
                }
            }
        }
    }

    image_set_format(img, "JPEG %dbit", jpg.num_components * 8);

    jpeg_finish_decompress(&jpg);
    jpeg_destroy_decompress(&jpg);

#ifdef HAVE_LIBEXIF
    process_exif(img, data, size);
#endif

    return imgload_success;
}

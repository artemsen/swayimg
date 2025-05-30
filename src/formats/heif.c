// SPDX-License-Identifier: MIT
// HEIF and AVIF formats decoder.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "buildcfg.h"
#include "exif.h"
#include "loader.h"

#include <libheif/heif.h>
#include <stdlib.h>

#ifdef HAVE_LIBEXIF
/**
 * Read Exif info.
 * @param img image data container
 * @param pih handle of HEIF/AVIF image
 */
static void read_exif(struct imgdata* img, struct heif_image_handle* pih)
{
    heif_item_id id;
    const int count =
        heif_image_handle_get_list_of_metadata_block_IDs(pih, "Exif", &id, 1);

    for (int i = 0; i < count; i++) {
        size_t sz = heif_image_handle_get_metadata_size(pih, id);
        uint8_t* data = malloc(sz);
        if (data) {
            const struct heif_error err =
                heif_image_handle_get_metadata(pih, id, data);
            if (err.code == heif_error_Ok) {
                process_exif(img, data + 4 /* skip offset */, sz);
            }
            free(data);
        }
    }
}
#endif // HAVE_LIBEXIF

// HEIF/AVIF loader implementation
enum image_status decode_heif(struct imgdata* img, const uint8_t* data,
                              size_t size)
{
    struct heif_context* heif = NULL;
    struct heif_image_handle* pih = NULL;
    struct heif_image* him = NULL;
    struct heif_error err;
    const uint8_t* decoded;
    struct pixmap* pm;
    int stride = 0;
    enum image_status status = imgload_fmterror;

    if (heif_check_filetype(data, size) != heif_filetype_yes_supported) {
        return imgload_unsupported;
    }

    heif = heif_context_alloc();
    if (!heif) {
        goto done;
    }
    err = heif_context_read_from_memory(heif, data, size, NULL);
    if (err.code != heif_error_Ok) {
        goto done;
    }
    err = heif_context_get_primary_image_handle(heif, &pih);
    if (err.code != heif_error_Ok) {
        goto done;
    }
    err = heif_decode_image(pih, &him, heif_colorspace_RGB,
                            heif_chroma_interleaved_RGBA, NULL);
    if (err.code != heif_error_Ok) {
        goto done;
    }
    decoded =
        heif_image_get_plane_readonly(him, heif_channel_interleaved, &stride);
    if (!decoded) {
        goto done;
    }

    pm = image_alloc_frame(img, heif_image_get_primary_width(him),
                           heif_image_get_primary_height(him));
    if (!pm) {
        goto done;
    }

    // convert to plain image frame
    for (size_t y = 0; y < pm->height; ++y) {
        const argb_t* src = (const argb_t*)(decoded + y * stride);
        argb_t* dst = &pm->data[y * pm->width];
        for (size_t x = 0; x < pm->width; ++x) {
            dst[x] = ABGR_TO_ARGB(src[x]);
        }
    }

    img->alpha = heif_image_handle_has_alpha_channel(pih);
    image_set_format(img, "HEIF/AVIF %dbpp",
                     heif_image_handle_get_luma_bits_per_pixel(pih));
#ifdef HAVE_LIBEXIF
    read_exif(img, pih);
#endif

    status = imgload_success;

done:
    if (him) {
        heif_image_release(him);
    }
    if (pih) {
        heif_image_handle_release(pih);
    }
    if (heif) {
        heif_context_free(heif);
    }
    return status;
}

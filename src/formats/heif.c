// SPDX-License-Identifier: MIT
// HEIF and AVIF formats decoder.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "../exif.h"
#include "buildcfg.h"
#include "loader.h"

#include <libheif/heif.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBEXIF
/**
 * Read Exif info.
 * @param ctx image context
 * @param pih handle of HEIF/AVIF image
 */
static void read_exif(struct image* ctx, struct heif_image_handle* pih)
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
                process_exif(ctx, data + 4 /* skip offset */, sz);
            }
            free(data);
        }
    }
}
#endif // HAVE_LIBEXIF

// HEIF/AVIF loader implementation
enum loader_status decode_heif(struct image* ctx, const uint8_t* data,
                               size_t size)
{
    struct heif_context* heif = NULL;
    struct heif_image_handle* pih = NULL;
    struct heif_image* img = NULL;
    struct heif_error err;
    const uint32_t* decoded;

    if (heif_check_filetype(data, size) != heif_filetype_yes_supported) {
        return ldr_unsupported;
    }

    heif = heif_context_alloc();
    if (!heif) {
        image_error(ctx, "unable to create heif context");
        return ldr_fmterror;
    }
    err = heif_context_read_from_memory(heif, data, size, NULL);
    if (err.code != heif_error_Ok) {
        goto fail;
    }
    err = heif_context_get_primary_image_handle(heif, &pih);
    if (err.code != heif_error_Ok) {
        goto fail;
    }
    err = heif_decode_image(pih, &img, heif_colorspace_RGB,
                            heif_chroma_interleaved_RGBA, NULL);
    if (err.code != heif_error_Ok) {
        goto fail;
    }
    decoded = (const uint32_t*)heif_image_get_plane_readonly(
        img, heif_channel_interleaved, NULL);
    if (!decoded) {
        err.message = "no decoded data";
        goto fail;
    }

    if (!image_allocate(ctx, heif_image_get_primary_width(img),
                        heif_image_get_primary_height(img))) {
        err.message = "not enough memory";
        goto fail;
    }

    // convert ABGR -> ARGB
    for (size_t i = 0; i < ctx->width * ctx->height; ++i) {
        ((argb_t*)ctx->data)[i] = ARGB_FROM_ABGR(decoded[i]);
    }

    ctx->alpha = heif_image_handle_has_alpha_channel(pih);
    image_add_meta(ctx, "Format", "HEIF/AVIF %dbpp",
                   heif_image_handle_get_luma_bits_per_pixel(pih));
#ifdef HAVE_LIBEXIF
    read_exif(ctx, pih);
#endif

    heif_image_release(img);
    heif_image_handle_release(pih);
    heif_context_free(heif);
    return ldr_success;

fail:
    image_error(ctx, "heif decode failed: %s", err.message);
    if (img) {
        heif_image_release(img);
    }
    if (pih) {
        heif_image_handle_release(pih);
    }
    heif_context_free(heif);
    return ldr_fmterror;
}

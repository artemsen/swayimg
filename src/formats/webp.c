// SPDX-License-Identifier: MIT
// WebP format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "buildcfg.h"
#include "exif.h"
#include "loader.h"

#include <string.h>
#include <webp/demux.h>

// WebP signature
static const uint8_t signature[] = { 'R', 'I', 'F', 'F' };

// WebP loader implementation
enum image_status decode_webp(struct imgdata* img, const uint8_t* data,
                              size_t size)
{
    const WebPData raw = { .bytes = data, .size = size };
    WebPAnimDecoderOptions webp_opts;
    WebPAnimDecoder* webp_dec = NULL;
    WebPAnimInfo webp_info;
    WebPBitstreamFeatures prop;
    int prev_timestamp = 0;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return imgload_unsupported;
    }

    // get image properties
    if (WebPGetFeatures(data, size, &prop) != VP8_STATUS_OK) {
        return imgload_fmterror;
    }

    // open decoder
    WebPAnimDecoderOptionsInit(&webp_opts);
    webp_opts.color_mode = MODE_BGRA;
    webp_dec = WebPAnimDecoderNew(&raw, &webp_opts);
    if (!webp_dec) {
        goto fail;
    }
    if (!WebPAnimDecoderGetInfo(webp_dec, &webp_info)) {
        goto fail;
    }

    // allocate frame sequence
    if (!image_alloc_frames(img, webp_info.frame_count)) {
        goto fail;
    }

    // decode every frame
    for (size_t i = 0; i < webp_info.frame_count; ++i) {
        uint8_t* buffer;
        int timestamp;
        struct imgframe* frame = arr_nth(img->frames, i);
        struct pixmap* pm = &frame->pm;

        if (!pixmap_create(pm, prop.has_alpha ? pixmap_argb : pixmap_xrgb,
                           webp_info.canvas_width, webp_info.canvas_height)) {
            goto fail;
        }
        if (!WebPAnimDecoderGetNext(webp_dec, &buffer, &timestamp)) {
            goto fail;
        }
        memcpy(pm->data, buffer, pm->width * pm->height * sizeof(argb_t));

        if (webp_info.frame_count > 1) {
            frame->duration = timestamp - prev_timestamp;
            prev_timestamp = timestamp;
            if (frame->duration <= 0) {
                frame->duration = 100;
            }
        }
    }

#ifdef HAVE_LIBEXIF
    const WebPDemuxer* webp_dmx = WebPAnimDecoderGetDemuxer(webp_dec);
    if (WebPDemuxGetI(webp_dmx, WEBP_FF_FORMAT_FLAGS) & EXIF_FLAG) {
        WebPChunkIterator it;
        if (WebPDemuxGetChunk(webp_dmx, "EXIF", 1, &it)) {
            process_exif(img, it.chunk.bytes, it.chunk.size);
            WebPDemuxReleaseChunkIterator(&it);
        }
    }
#endif // HAVE_LIBEXIF

    WebPAnimDecoderDelete(webp_dec);

    image_set_format(
        img, "WebP %s %s%s", prop.format == 1 ? "lossy" : "lossless",
        prop.has_alpha ? "+alpha" : "", prop.has_animation ? "+animation" : "");

    return imgload_success;

fail:
    if (webp_dec) {
        WebPAnimDecoderDelete(webp_dec);
    }
    return imgload_fmterror;
}

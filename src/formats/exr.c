// SPDX-License-Identifier: MIT
// EXR format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "../loader.h"

#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <openexr.h>
#pragma GCC diagnostic pop
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

// EXR signature
static const uint8_t signature[] = { 0x76, 0x2f, 0x31, 0x01 };

// EXR data buffer
struct data_buffer {
    const uint8_t* data;
    const size_t size;
};

// EXR data reader
static int64_t
exr_reader(__attribute__((unused)) exr_const_context_t exr, void* userdata,
           void* buffer, uint64_t sz, uint64_t offset,
           __attribute__((unused)) exr_stream_error_func_ptr_t error_cb)
{
    const struct data_buffer* buf = userdata;
    if (offset + sz > buf->size) {
        return -1;
    }
    memcpy(buffer, buf->data + offset, sz);
    return sz;
}

/**
 * Decode single chunk.
 * @param ectx EXR context
 * @param chunk source chunk info
 * @param decoder EXR decoder instance
 * @param buffer data pointer for unpacked data of the current chunk
 * @return result code
 */
static exr_result_t decode_chunk(const exr_context_t ectx,
                                 const exr_chunk_info_t* chunk,
                                 exr_decode_pipeline_t* decoder,
                                 uint8_t* buffer)
{
    exr_result_t rc;
    int8_t bpp = 0;

    // initialize decoder
    if (decoder->channels) {
        rc = exr_decoding_update(ectx, 0, chunk, decoder);
    } else {
        rc = exr_decoding_initialize(ectx, 0, chunk, decoder);
        if (rc == EXR_ERR_SUCCESS) {
            rc = exr_decoding_choose_default_routines(ectx, 0, decoder);
        }
    }
    if (rc != EXR_ERR_SUCCESS) {
        return rc;
    }

    // configure output buffer
    for (int16_t i = 0; i < decoder->channel_count; ++i) {
        bpp += decoder->channels[i].bytes_per_element;
    }
    for (int16_t i = 0; i < decoder->channel_count; ++i) {
        exr_coding_channel_info_t* cci = &decoder->channels[i];
        cci->decode_to_ptr = buffer;
        cci->user_pixel_stride = bpp;
        cci->user_line_stride = cci->width * bpp;
        cci->user_bytes_per_element = decoder->channels[i].bytes_per_element;
        buffer += decoder->channels[i].bytes_per_element;
    }

    // decode current chunk
    return exr_decoding_run(ectx, 0, decoder);
}

/**
 * Decode pixel.
 * @param decoder EXR decoder instance
 * @param unpacked pointer to unpacked data
 * @param size max number of bytes in unpacked buffer
 * @return ARGB value of the pixel
 */
static argb_t decode_pixel(const exr_decode_pipeline_t* decoder,
                           const uint8_t* unpacked, size_t size)
{
    uint8_t a = 0xff, r = 0, g = 0, b = 0;

    for (int16_t i = 0; i < decoder->channel_count; ++i) {
        const exr_coding_channel_info_t* channel = &decoder->channels[i];
        float intensity = 0;
        size_t color;

        // it's all a dirty hack =)
        switch (channel->data_type) {
            case EXR_PIXEL_UINT:
                break; // not supported
            case EXR_PIXEL_HALF:
                if (size >= sizeof(uint16_t)) {
                    // convert half to float
                    // todo
                    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign)
                    const uint16_t half = *(const uint16_t*)unpacked;
                    union {
                        uint32_t i;
                        float f;
                    } hf;
                    hf.i = (half & 0x8000) << 16;
                    hf.i |= ((half & 0x7c00) + 0x1c000) << 13;
                    hf.i |= (half & 0x03ff) << 13;
                    intensity = hf.f;
                }
                break;
            case EXR_PIXEL_FLOAT:
                if (size >= sizeof(float)) {
                    // todo
                    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign)
                    intensity = *(const float*)unpacked;
                }
                break;
            default:
                break; // not supported
        }
        color = intensity * 0xff;
        if (color > 0xff) {
            color = 0xff;
        }

        switch (*channel->channel_name) {
            case 'A':
                a = color;
                break;
            case 'R':
                r = color;
                break;
            case 'G':
                g = color;
                break;
            case 'B':
                b = color;
                break;
            default:
                break; // not supported
        }

        unpacked += channel->bytes_per_element;
        if (size <= (size_t)channel->bytes_per_element) {
            break;
        }
        size -= channel->bytes_per_element;
    }

    return ARGB_SET_A(a) | ARGB_SET_R(r) | ARGB_SET_G(g) | ARGB_SET_B(b);
}

/**
 * Load scanlined EXR image.
 * @param ectx EXR context
 * @param pm destination pixmap
 * @return result code
 */
static exr_result_t load_scanlined(const exr_context_t ectx, struct pixmap* pm)
{
    exr_result_t rc;
    int32_t scanlines;
    uint64_t chunk_size;
    uint8_t* buffer = NULL;
    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;
    argb_t* dst = pm->data;
    exr_attr_box2i_t dwnd;

    // temporary buffer for decoded chunk's scanlines
    rc = exr_get_chunk_unpacked_size(ectx, 0, &chunk_size);
    if (rc != EXR_ERR_SUCCESS) {
        return rc;
    }
    buffer = malloc(chunk_size);
    if (!buffer) {
        return EXR_ERR_OUT_OF_MEMORY;
    }

    // get image properties
    rc = exr_get_data_window(ectx, 0, &dwnd);
    if (rc != EXR_ERR_SUCCESS) {
        goto done;
    }
    rc = exr_get_scanlines_per_chunk(ectx, 0, &scanlines);
    if (rc != EXR_ERR_SUCCESS) {
        goto done;
    }

    // decode chunks
    for (int32_t y = dwnd.min.y; y <= dwnd.max.y; y += scanlines) {
        exr_chunk_info_t chunk;
        int8_t bpp = 0;

        rc = exr_read_scanline_chunk_info(ectx, 0, y, &chunk);
        if (rc != EXR_ERR_SUCCESS) {
            break;
        }
        rc = decode_chunk(ectx, &chunk, &decoder, buffer);
        if (rc != EXR_ERR_SUCCESS) {
            break;
        }

        // put pixels to final image
        for (int16_t i = 0; i < decoder.channel_count; ++i) {
            bpp += decoder.channels[i].bytes_per_element;
        }
        for (size_t i = 0; i < chunk_size; i += bpp) {
            if (dst >= pm->data + (pm->width * pm->height)) {
                goto done;
            }
            *dst = decode_pixel(&decoder, buffer + i, chunk_size - i);
            ++dst;
        }
    }

done:
    exr_decoding_destroy(ectx, &decoder);
    free(buffer);
    return rc;
}

/**
 * Load tailed EXR image.
 * @param ectx EXR context
 * @param pm destination pixmap
 * @return result code
 */
static exr_result_t load_tailed(const exr_context_t ectx, struct pixmap* pm)
{
    exr_result_t rc;
    uint64_t chunk_size;
    uint8_t* buffer = NULL;
    int32_t levels_x, levels_y;
    exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

    // temporary buffer for decoded chunk's scanlines
    rc = exr_get_chunk_unpacked_size(ectx, 0, &chunk_size);
    if (rc != EXR_ERR_SUCCESS) {
        return rc;
    }
    buffer = malloc(chunk_size);
    if (!buffer) {
        return EXR_ERR_OUT_OF_MEMORY;
    }

    rc = exr_get_tile_levels(ectx, 0, &levels_x, &levels_y);
    if (rc != EXR_ERR_SUCCESS) {
        goto done;
    }

    for (int32_t lvl_y = 0; lvl_y < levels_y; ++lvl_y) {
        for (int32_t lvl_x = 0; lvl_x < levels_x; ++lvl_x) {
            int32_t lvl_w, lvl_h;
            int32_t tile_w, tile_h;
            int tile_x, tile_y;
            exr_chunk_info_t chunk;
            exr_decode_pipeline_t decoder = EXR_DECODE_PIPELINE_INITIALIZER;

            rc = exr_get_level_sizes(ectx, 0, lvl_x, lvl_y, &lvl_w, &lvl_h);
            if (rc != EXR_ERR_SUCCESS) {
                goto done;
            }
            rc = exr_get_tile_sizes(ectx, 0, lvl_x, lvl_y, &tile_w, &tile_h);
            if (rc != EXR_ERR_SUCCESS) {
                goto done;
            }

            tile_y = 0;
            for (int64_t img_y = 0; img_y < lvl_h; img_y += tile_h) {
                tile_x = 0;
                for (int64_t img_x = 0; img_x < lvl_w; img_x += tile_w) {
                    int8_t bpp = 0;

                    rc = exr_read_tile_chunk_info(ectx, 0, tile_x, tile_y,
                                                  lvl_x, lvl_y, &chunk);
                    if (rc != EXR_ERR_SUCCESS) {
                        goto done;
                    }
                    rc = decode_chunk(ectx, &chunk, &decoder, buffer);
                    if (rc != EXR_ERR_SUCCESS) {
                        goto done;
                    }

                    // put pixels to final image
                    for (int16_t i = 0; i < decoder.channel_count; ++i) {
                        bpp += decoder.channels[i].bytes_per_element;
                    }
                    for (int32_t y = 0; y < chunk.height; ++y) {
                        const uint8_t* src_line =
                            &buffer[y * chunk.width * bpp];
                        argb_t* dst_line = &pm->data[(img_y + y) * pm->width];
                        for (int32_t x = 0; x < chunk.width; ++x) {
                            dst_line[img_x + x] =
                                decode_pixel(&decoder, src_line + x * bpp, 42);
                        }
                    }
                    ++tile_x;
                }
                ++tile_y;
            }
        }
    }

done:
    exr_decoding_destroy(ectx, &decoder);
    free(buffer);
    return rc;
}

// EXR loader implementation
enum loader_status decode_exr(struct image* ctx, const uint8_t* data,
                              size_t size)
{
    exr_result_t rc;
    exr_context_t exr;
    exr_context_initializer_t einit = EXR_DEFAULT_CONTEXT_INITIALIZER;
    struct pixmap* pm;
    exr_attr_box2i_t dwnd;
    exr_storage_t storage;
    struct data_buffer buf = {
        .data = data,
        .size = size,
    };

    einit.user_data = &buf;
    einit.read_fn = exr_reader;

    // check signature
    if (size < sizeof(signature) ||
        memcmp(data, signature, sizeof(signature))) {
        return ldr_unsupported;
    }

    // decode
    rc = exr_start_read(&exr, "exr", &einit);
    if (rc != EXR_ERR_SUCCESS) {
        return ldr_fmterror;
    }

    rc = exr_get_data_window(exr, 0, &dwnd);
    if (rc != EXR_ERR_SUCCESS) {
        goto done;
    }

    pm = image_allocate_frame(ctx, dwnd.max.x - dwnd.min.x + 1,
                              dwnd.max.y - dwnd.min.y + 1);
    if (!pm) {
        rc = EXR_ERR_OUT_OF_MEMORY;
        goto done;
    }

    rc = exr_get_storage(exr, 0, &storage);
    if (rc != EXR_ERR_SUCCESS) {
        goto done;
    }

    image_set_format(ctx, "EXR");
    ctx->alpha = true;

    if (storage == EXR_STORAGE_SCANLINE) {
        rc = load_scanlined(exr, pm);
    } else if (storage == EXR_STORAGE_TILED) {
        rc = load_tailed(exr, pm);
    } else {
        rc = EXR_ERR_FEATURE_NOT_IMPLEMENTED;
    }

done:
    exr_finish(&exr);
    if (rc != EXR_ERR_SUCCESS) {
        image_free_frames(ctx);
        return ldr_fmterror;
    }
    return ldr_success;
}

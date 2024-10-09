#define QOI_NO_STDIO
#define QOI_IMPLEMENTATION

#include "../loader.h"
#include "./qoi.h"

#include <stdlib.h>
#include <string.h>


/**
 * Decode a QOI image.
 * @param ctx image context
 * @param data input QOI data
 * @param size size of the QOI data
 * @return true if decode was successful
 */
static bool decode_qoi_image(struct image* ctx, const uint8_t* data, size_t size)
{
    qoi_desc desc;
    void* pixels = qoi_decode(data, size, &desc, 4); // QOI uses RGBA (4 channels)

    if (!pixels) {
        return false; // Decoding failed
    }

    struct pixmap* pm = image_allocate_frame(ctx, desc.width, desc.height);
    if (!pm) {
        free(pixels);
        return false;
    }

    // Copy the decoded pixel data into the pixmap
    memcpy(pm->data, pixels, desc.width * desc.height * 4); // 4 bytes per pixel (RGBA)

    // reorder the color channels, RGBA -> BGRA
    uint8_t* src = (uint8_t*)pixels;
    uint8_t* dst = (uint8_t*)pm->data;
    for (size_t i = 0; i < desc.width * desc.height; ++i) {
        dst[0] = src[2];  // Blue
        dst[1] = src[1];  // Green
        dst[2] = src[0];  // Red
        dst[3] = src[3];  // Alpha
        src += 4;
        dst += 4;
    }

    free(pixels); // Free the QOI decoder output
    return true;
}

// QOI loader implementation
enum loader_status decode_qoi(struct image* ctx, const uint8_t* data, size_t size)
{
    size_t qoi_header_size = 14;

    if (size < qoi_header_size
    // || qoi_check_signature(data) != 0 //TODO: implement this
    ) {
        return ldr_unsupported;
    }

    // Decode the QOI image
    if (!decode_qoi_image(ctx, data, size)) {
        image_free_frames(ctx);
        return ldr_fmterror;
    }

    // Set image properties
    image_set_format(ctx, "QOI 32bit");
    ctx->alpha = true; // QOI supports alpha channel

    return ldr_success;
}

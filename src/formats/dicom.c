// SPDX-License-Identifier: MIT
// DICOM format decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include <limits.h>
#include <string.h>

// DICOM signature
static const uint8_t signature[] = { 'D', 'I', 'C', 'M' };
#define DICOM_SIGNATURE_OFFSET 128

// DICOM tags
#define TAG_SAMPLES_PER_PIXEL 0x00280002
#define TAG_ROWS              0x00280010
#define TAG_COLUMNS           0x00280011
#define TAG_BIT_ALLOCATED     0x00280100
#define TAG_SMALL_PIXEL_VAL   0x00280106
#define TAG_BIG_PIXEL_VAL     0x00280107
#define TAG_PIXEL_DATA        0x7fe00010

// DICOM element value types
enum value_representation {
    VR_AE = 'A' | ('E' << 8),
    VR_AS = 'A' | ('S' << 8),
    VR_AT = 'A' | ('T' << 8),
    VR_CS = 'C' | ('S' << 8),
    VR_DA = 'D' | ('A' << 8),
    VR_DS = 'D' | ('S' << 8),
    VR_DT = 'D' | ('T' << 8),
    VR_FD = 'F' | ('D' << 8),
    VR_FL = 'F' | ('L' << 8),
    VR_IS = 'I' | ('S' << 8),
    VR_LO = 'L' | ('O' << 8),
    VR_LT = 'L' | ('T' << 8),
    VR_PN = 'P' | ('N' << 8),
    VR_SH = 'S' | ('H' << 8),
    VR_SL = 'S' | ('L' << 8),
    VR_SS = 'S' | ('S' << 8),
    VR_ST = 'S' | ('T' << 8),
    VR_TM = 'T' | ('M' << 8),
    VR_UI = 'U' | ('I' << 8),
    VR_UL = 'U' | ('L' << 8),
    VR_US = 'U' | ('S' << 8),
    VR_UT = 'U' | ('T' << 8),
    VR_OB = 'O' | ('B' << 8),
    VR_OW = 'O' | ('W' << 8),
    VR_SQ = 'S' | ('Q' << 8),
    VR_UN = 'U' | ('N' << 8),
    VR_QQ = 'Q' | ('Q' << 8),
    VR_RT = 'R' | ('T' << 8),
};

// DICOM image description
struct dicom_image {
    uint16_t spp;        ///< Samples per Pixel
    uint16_t bpp;        ///< Number of bits allocated for each pixel sample
    uint16_t width;      ///< Image width
    uint16_t height;     ///< Image height
    int16_t px_min;      ///< Min pixel value encountered in the image
    int16_t px_max;      ///< Max pixel value encountered in the image
    const uint8_t* data; ///< Image data
    size_t data_sz;      ///< Size of data in bytes
};

// DICOM element description
struct element {
    uint32_t tag;
    uint16_t vr;
    const void* data;
    size_t size;
};

// Binary stream
struct stream {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

/**
 * Consume data from the stream.
 * @param stream binary stream
 * @param bytes number of bytes to consume
 * @return pointer to the data or NULL on End of stream
 */
static const uint8_t* consume(struct stream* stream, size_t bytes)
{
    const uint8_t* ptr = NULL;
    const size_t end = stream->pos + bytes;

    if (end <= stream->size) {
        ptr = stream->data + stream->pos;
        stream->pos = end;
    }

    return ptr;
}

/**
 * Read next data element from the stream.
 * @param stream binary stream
 * @param element output element description
 * @return false if no more elements int the stream
 */
static bool next_element(struct stream* stream, struct element* element)
{
    const uint8_t* data;

    // read tag
    if (!(data = consume(stream, sizeof(element->tag)))) {
        return false;
    }
    element->tag = (*(const uint16_t*)data) << 16;
    element->tag |= *(const uint16_t*)(data + sizeof(uint16_t));

    // read value representation (type)
    if (!(data = consume(stream, sizeof(element->vr)))) {
        return false;
    }
    element->vr = *(const uint16_t*)data;

    // get payload size
    if (!(data = consume(stream, sizeof(uint16_t)))) {
        return false;
    }
    element->size = *(const uint16_t*)data;
    if (element->size == 0 &&
        (element->vr == VR_OB || element->vr == VR_OW || element->vr == VR_SQ ||
         element->vr == VR_UN || element->vr == VR_UT)) {
        if (!(data = consume(stream, sizeof(uint32_t)))) {
            return false;
        }
        element->size = *(const uint32_t*)data;
    }

    // get payload data
    if (element->size == 0) {
        element->data = NULL;
    } else if (!(element->data = consume(stream, element->size))) {
        return false;
    }

    return true;
}

/**
 * Get image description from the stream.
 * @param stream binary stream
 * @param image output image description
 * @return true if image description is valid
 */
static bool get_image(struct stream* stream, struct dicom_image* image)
{
    struct element el;

    memset(image, 0, sizeof(*image));

    // collect info
    while (next_element(stream, &el)) {
        if (!el.data) {
            continue;
        }
        if (el.tag == TAG_SAMPLES_PER_PIXEL && el.vr == VR_US) {
            image->spp = *(const uint16_t*)el.data;
        } else if (el.tag == TAG_ROWS && el.vr == VR_US) {
            image->height = *(const uint16_t*)el.data;
        } else if (el.tag == TAG_COLUMNS && el.vr == VR_US) {
            image->width = *(const uint16_t*)el.data;
        } else if (el.tag == TAG_BIT_ALLOCATED && el.vr == VR_US) {
            image->bpp = *(const uint16_t*)el.data;
        } else if (el.tag == TAG_SMALL_PIXEL_VAL && el.vr == VR_SS) {
            image->px_min = *(const int16_t*)el.data;
        } else if (el.tag == TAG_BIG_PIXEL_VAL && el.vr == VR_SS) {
            image->px_max = *(const int16_t*)el.data;
        } else if (el.tag == TAG_PIXEL_DATA && el.vr == VR_OW) {
            image->data = el.data;
            image->data_sz = el.size;
        }
    }

    // check
    if (!image->data || image->height == 0 || image->width == 0 ||
        image->data_sz != image->width * image->height * (image->bpp / 8)) {
        return false;
    }

    return true;
}

// DICOM loader implementation
enum image_status decode_dicom(struct imgdata* img, const uint8_t* data,
                               size_t size)
{
    struct dicom_image dicom;
    struct stream stream;
    struct pixmap* pm;
    size_t total_pixels;
    double pixel_coeff;

    // check signature
    if (size < DICOM_SIGNATURE_OFFSET + sizeof(signature) ||
        memcmp(data + DICOM_SIGNATURE_OFFSET, signature, sizeof(signature))) {
        return imgload_unsupported;
    }

    // get image description
    stream.data = data;
    stream.size = size;
    stream.pos = DICOM_SIGNATURE_OFFSET + sizeof(signature);
    if (!get_image(&stream, &dicom) || dicom.spp != 1 /* monochrome */ ||
        dicom.bpp != 16 /* 2 bytes per pixel */) {
        return imgload_fmterror;
    }

    // calculate min/max color value if not set yet
    if (dicom.px_max == 0 || dicom.px_max <= dicom.px_min) {
        dicom.px_min = INT16_MAX;
        for (size_t i = 0; i < dicom.data_sz; i += sizeof(int16_t)) {
            const int16_t color = *(const int16_t*)(dicom.data + i);
            if (dicom.px_max < color) {
                dicom.px_max = color;
            }
            if (dicom.px_min > color) {
                dicom.px_min = color;
            }
        }
    }

    // Calculate coefficient for converting 16-bit color to 8-bit
    if (dicom.px_max <= dicom.px_min) {
        pixel_coeff = 1.0;
    } else {
        pixel_coeff = 256.0 / (dicom.px_max - dicom.px_min);
    }

    // allocate image buffer
    pm = image_alloc_frame(img, dicom.width, dicom.height);
    if (!pm) {
        return imgload_fmterror;
    }

    // decode image
    total_pixels = dicom.width * dicom.height;
    for (size_t i = 0; i < total_pixels; ++i) {
        int16_t color = *((const int16_t*)dicom.data + i);
        color -= dicom.px_min;
        color *= pixel_coeff;
        pm->data[i] = ARGB(0xff, color, color, color);
    }

    image_set_format(img, "DICOM");

    return imgload_success;
}

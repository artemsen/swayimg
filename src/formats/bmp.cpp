// SPDX-License-Identifier: MIT
// BMP format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <cstdlib>
#include <format>

// register format in factory
class ImageBmp;
static const ImageLoader::Registrator<ImageBmp>
    image_format_registartion("BMP", ImageLoader::Priority::Low);

/* BMP image. */
class ImageBmp : public Image {
private:
    // BMP signature
    static constexpr const uint8_t signature[] = { 'B', 'M' };

    // Compression types
    static constexpr const uint8_t BI_RGB = 0;
    static constexpr const uint8_t BI_RLE8 = 1;
    static constexpr const uint8_t BI_RLE4 = 2;
    static constexpr const uint8_t BI_BITFIELDS = 3;

    // RLE escape codes
    static constexpr const uint8_t RLE_ESC_EOL = 0;
    static constexpr const uint8_t RLE_ESC_EOF = 1;
    static constexpr const uint8_t RLE_ESC_DELTA = 2;

    // Default mask for 16-bit images
    static constexpr const uint16_t MASK555_RED = 0x7c00;
    static constexpr const uint16_t MASK555_GREEN = 0x03e0;
    static constexpr const uint16_t MASK555_BLUE = 0x001f;
    static constexpr const uint16_t MASK555_ALPHA = 0x0000;

    // Sizes of DIB Headers
    static constexpr const uint8_t BITMAPINFOHEADER_SIZE = 0x28;
    static constexpr const uint8_t BITMAPINFOV2HEADER_SIZE = 0x34;
    static constexpr const uint8_t BITMAPINFOV3HEADER_SIZE = 0x38;
    static constexpr const uint8_t BITMAPINFOV4HEADER_SIZE = 0x6C;
    static constexpr const uint8_t BITMAPINFOV5HEADER_SIZE = 0x7C;

    static constexpr const uint8_t BITS_PER_BYTE = 8;

    // Bitmap file header: BITMAPFILEHEADER
    struct __attribute__((__packed__)) Header {
        uint16_t type;
        uint32_t file_size;
        uint32_t reserved;
        uint32_t offset;
    };

    // Bitmap info: BITMAPINFOHEADER
    struct __attribute__((__packed__)) Info {
        uint32_t dib_size;
        int32_t width;
        int32_t height;
        uint16_t planes;
        uint16_t bpp;
        uint32_t compression;
        uint32_t img_size;
        uint32_t hres;
        uint32_t vres;
        uint32_t clr_palette;
        uint32_t clr_important;
    };

    // Masks used for for 16bit images
    struct __attribute__((__packed__)) Mask {
        uint32_t red;
        uint32_t green;
        uint32_t blue;
        uint32_t alpha;
    };

    // Color palette
    struct Palette {
        const uint32_t* table;
        size_t size;
    };

    /**
     * Get number of the consecutive zero bits (trailing) on the right.
     * @param val source value
     * @return number of zero bits
     */
    constexpr size_t right_zeros(const uint32_t val) const
    {
        size_t count = sizeof(uint32_t) * BITS_PER_BYTE;
        const size_t value = val & -(int32_t)val;
        if (value) {
            --count;
        }
        if (value & 0x0000ffff) {
            count -= 16;
        }
        if (value & 0x00ff00ff) {
            count -= 8;
        }
        if (value & 0x0f0f0f0f) {
            count -= 4;
        }
        if (value & 0x33333333) {
            count -= 2;
        }
        if (value & 0x55555555) {
            count -= 1;
        }
        return count;
    }

    /**
     * Get number of bits set.
     * @param val source value
     * @return number of bits set
     */
    constexpr size_t bits_set(const uint32_t val) const
    {
        size_t bits = val - ((val >> 1) & 0x55555555);
        bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
        return (((bits + (bits >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
    }

    /**
     * Get shift size for color channel.
     * @param val color channel mask
     * @return shift size: positive=right, negative=left
     */
    constexpr ssize_t mask_shift(const uint32_t mask) const
    {
        const ssize_t start = right_zeros(mask) + bits_set(mask);
        return start - BITS_PER_BYTE;
    }

    /**
     * Decode bitmap with masked colors.
     * @param pm target pixmap
     * @param bmp bitmap info
     * @param mask channels mask
     * @param buffer input bitmap buffer
     * @param buffer_sz size of buffer
     * @return false if input buffer has errors
     */
    bool decode_masked(Pixmap& pm, const Info& bmp, const Mask& mask,
                       const uint8_t* buffer, const size_t buffer_sz) const
    {
        const bool default_mask = mask.red == 0 && mask.green == 0 &&
            mask.blue == 0 && mask.alpha == 0;

        const uint32_t mask_r = default_mask ? MASK555_RED : mask.red;
        const uint32_t mask_g = default_mask ? MASK555_GREEN : mask.green;
        const uint32_t mask_b = default_mask ? MASK555_BLUE : mask.blue;
        const uint32_t mask_a = default_mask ? MASK555_ALPHA : mask.alpha;
        const ssize_t shift_r = mask_shift(mask_r);
        const ssize_t shift_g = mask_shift(mask_g);
        const ssize_t shift_b = mask_shift(mask_b);
        const ssize_t shift_a = mask_shift(mask_a);

        const size_t stride = 4 * ((bmp.width * bmp.bpp + 31) / 32);

        // check size of source buffer
        if (buffer_sz < pm.height() * stride) {
            return false;
        }

        for (size_t y = 0; y < pm.height(); ++y) {
            const uint8_t* src_y = buffer + y * stride;
            for (size_t x = 0; x < pm.width(); ++x) {
                const uint8_t* src = src_y + x * (bmp.bpp / BITS_PER_BYTE);
                uint32_t m, r, g, b, a;
                if (bmp.bpp == 32) {
                    m = *(uint32_t*)src;
                } else if (bmp.bpp == 16) {
                    m = *(uint16_t*)src;
                } else {
                    return false;
                }
                r = m & mask_r;
                g = m & mask_g;
                b = m & mask_b;
                r = 0xff & (shift_r > 0 ? r >> shift_r : r << -shift_r);
                g = 0xff & (shift_g > 0 ? g >> shift_g : g << -shift_g);
                b = 0xff & (shift_b > 0 ? b >> shift_b : b << -shift_b);

                if (mask_a) {
                    a = m & mask_a;
                    a = 0xff & (shift_a > 0 ? a >> shift_a : a << -shift_a);
                } else {
                    a = 0xff;
                }

                argb_t& dst = pm.at(x, y);
                dst.a = a;
                dst.r = r;
                dst.g = g;
                dst.b = b;
            }
        }

        return true;
    }

    /**
     * Decode RLE compressed bitmap.
     * @param pm target pixmap
     * @param bmp bitmap info
     * @param palette color palette
     * @param buffer input bitmap buffer
     * @param buffer_sz size of buffer
     * @return false if input buffer has errors
     */
    bool decode_rle(Pixmap& pm, const Info& bmp, const Palette& palette,
                    const uint8_t* buffer, const size_t buffer_sz) const
    {
        size_t x = 0, y = 0;
        size_t buffer_pos = 0;

        while (buffer_pos + 2 <= buffer_sz) {
            uint8_t rle1 = buffer[buffer_pos++];
            const uint8_t rle2 = buffer[buffer_pos++];
            if (rle1 == 0) {
                // escape code
                if (rle2 == RLE_ESC_EOL) {
                    x = 0;
                    ++y;
                } else if (rle2 == RLE_ESC_EOF) {
                    // remove alpha channel
                    pm.foreach([](argb_t& color) {
                        color.a = argb_t::max;
                    });
                    return true;
                } else if (rle2 == RLE_ESC_DELTA) {
                    if (buffer_pos + 2 >= buffer_sz) {
                        return false;
                    }
                    x += buffer[buffer_pos++];
                    y += buffer[buffer_pos++];
                } else {
                    // absolute mode
                    if (buffer_pos +
                            (bmp.compression == BI_RLE4 ? rle2 / 2 : rle2) >
                        buffer_sz) {
                        return false;
                    }
                    if (x + rle2 > pm.width() || y >= pm.height()) {
                        return false;
                    }
                    uint8_t val = 0;
                    for (size_t i = 0; i < rle2; ++i) {
                        uint8_t index;
                        if (bmp.compression == BI_RLE8) {
                            index = buffer[buffer_pos++];
                        } else {
                            if (i & 1) {
                                index = val & 0x0f;
                            } else {
                                val = buffer[buffer_pos++];
                                index = val >> 4;
                            }
                        }
                        if (index >= palette.size) {
                            return false;
                        }
                        pm.at(x, y) = palette.table[index];
                        ++x;
                    }
                    if ((bmp.compression == BI_RLE8 && rle2 & 1) ||
                        (bmp.compression == BI_RLE4 &&
                         ((rle2 & 3) == 1 || (rle2 & 3) == 2))) {
                        ++buffer_pos; // zero-padded 16-bit
                    }
                }
            } else {
                // encoded mode
                if (x + rle1 > pm.width()) {
                    rle1 = pm.width() - x;
                }
                if (y >= pm.height()) {
                    return false;
                }
                if (bmp.compression == BI_RLE8) {
                    // 8 bpp
                    if (rle2 >= palette.size) {
                        return false;
                    }
                    for (size_t i = 0; i < rle1; ++i) {
                        pm.at(x, y) = palette.table[rle2];
                        ++x;
                    }
                } else {
                    // 4 bpp
                    const uint8_t index[] = { static_cast<uint8_t>(rle2 >> 4),
                                              static_cast<uint8_t>(rle2 &
                                                                   0x0f) };
                    if (index[0] >= palette.size || index[1] >= palette.size) {
                        return false;
                    }
                    for (size_t i = 0; i < rle1; ++i) {
                        pm.at(x, y) = palette.table[index[i & 1]];
                        ++x;
                    }
                }
            }
        }

        return false;
    }

    // /**
    //  * Decode uncompressed bitmap.
    //  * @param pm target pixmap
    //  * @param palette color palette
    //  * @param buffer input bitmap buffer
    //  * @param buffer_sz size of buffer
    //  * @param decoded output data buffer
    //  * @return false if input buffer has errors
    //  */
    bool decode_rgb(Pixmap& pm, const Info& bmp, const Palette& palette,
                    const uint8_t* buffer, const size_t buffer_sz) const
    {
        const size_t stride = 4 * ((bmp.width * bmp.bpp + 31) / 32);

        // check size of source buffer
        if (buffer_sz < pm.height() * stride) {
            return false;
        }

        for (size_t y = 0; y < pm.height(); ++y) {
            const uint8_t* src_y = buffer + y * stride;
            for (size_t x = 0; x < pm.width(); ++x) {
                argb_t& dst = pm.at(x, y);
                const uint8_t* src = src_y + x * (bmp.bpp / BITS_PER_BYTE);
                if (bmp.bpp == 32) {
                    dst = *reinterpret_cast<const uint32_t*>(src);
                } else if (bmp.bpp == 24) {
                    dst = *reinterpret_cast<const uint32_t*>(src);
                    dst.a = argb_t::max;
                } else if (bmp.bpp == 8 || bmp.bpp == 4 || bmp.bpp == 1) {
                    // indexed colors
                    const size_t bits_offset = x * bmp.bpp;
                    const size_t byte_offset = bits_offset / BITS_PER_BYTE;
                    const size_t start_bit =
                        bits_offset - byte_offset * BITS_PER_BYTE;
                    const uint8_t index =
                        (*(src_y + byte_offset) >>
                         (BITS_PER_BYTE - bmp.bpp - start_bit)) &
                        (0xff >> (BITS_PER_BYTE - bmp.bpp));

                    if (index >= palette.size) {
                        return false;
                    }
                    dst = palette.table[index];
                    dst.a = argb_t::max;
                } else {
                    return false;
                }
            }
        }

        return true;
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(Header) + sizeof(Info) ||
            std::memcmp(data.data(), signature, sizeof(signature))) {
            return false;
        }

        const Header* hdr = reinterpret_cast<const Header*>(data.data());
        const Info* bmp =
            reinterpret_cast<const Info*>(data.data() + sizeof(Header));

        // check format
        if (hdr->offset >= data.size() ||
            hdr->offset < sizeof(Header) + sizeof(Info)) {
            return false;
        }
        if (bmp->dib_size > hdr->offset) {
            return false;
        }

        // allocate pixmap
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(bmp->bpp == 32 ? Pixmap::ARGB : Pixmap::RGB,
                  std::abs(bmp->width), std::abs(bmp->height));

        const uint8_t* image_data = data.data() + hdr->offset;
        const size_t image_data_sz = data.size() - hdr->offset;

        // palette
        const uint32_t* color_data = reinterpret_cast<const uint32_t*>(
            reinterpret_cast<const uint8_t*>(bmp) + bmp->dib_size);
        const size_t color_data_sz =
            hdr->offset - sizeof(Header) - bmp->dib_size;
        const Palette palette {
            .table = color_data,
            .size = color_data_sz / sizeof(uint32_t),
        };

        format = std::format("BMP {}bit ", bmp->bpp);

        // decode bitmap
        if (bmp->compression == BI_BITFIELDS || bmp->bpp == 16) {
            format += "masked";

            // create mask
            Mask mask {};
            const uint32_t* mask_location;
            if (bmp->dib_size > BITMAPINFOHEADER_SIZE) {
                mask_location = (const uint32_t*)(bmp + 1);
            } else {
                mask_location =
                    (color_data_sz >= 3 * sizeof(uint32_t) ? color_data
                                                           : nullptr);
            }
            if (mask_location) {
                mask.red = mask_location[0];
                mask.green = mask_location[1];
                mask.blue = mask_location[2];
                mask.alpha = bmp->dib_size > BITMAPINFOV2HEADER_SIZE
                    ? mask_location[3]
                    : 0;
            }

            if (!decode_masked(pm, *bmp, mask, image_data, image_data_sz)) {
                return false;
            }
        } else if (bmp->compression == BI_RLE8 || bmp->compression == BI_RLE4) {
            format += "RLE";
            if (!decode_rle(pm, *bmp, palette, image_data, image_data_sz)) {
                return false;
            }
        } else if (bmp->compression == BI_RGB) {
            format += "uncompressed";
            if (!decode_rgb(pm, *bmp, palette, image_data, image_data_sz)) {
                return false;
            }
        } else {
            return false;
        }

        if (bmp->height > 0) {
            pm.flip_vertical();
        }

        return true;
    }
};

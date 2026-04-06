// SPDX-License-Identifier: MIT
// TGA (Truevision TARGA) image format.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <cstring>
#include <format>

class ImageFormatTga : public ImageFormat {
public:
    ImageFormatTga()
        : ImageFormat(Priority::Lowest, "tga")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        if (data.size < sizeof(Header)) {
            return nullptr;
        }
        const Header* tga = reinterpret_cast<const Header*>(data.data);
        if (!validate_header(tga)) {
            return nullptr;
        }

        // get color map
        const uint8_t* colormap = nullptr;
        size_t colormap_sz = 0;
        const bool has_colormap =
            (tga->clrmap_type & TGA_COLORMAP) && tga->cm_size && tga->cm_bpc;
        switch (tga->image_type) {
            case TGA_UNC_CM:
            case TGA_RLE_CM:
                if (!has_colormap) {
                    return nullptr;
                }
                colormap_sz = tga->cm_size *
                    (tga->cm_bpc / 8 + (tga->cm_bpc % 8 ? 1 : 0));
                colormap = data.data + sizeof(Header) + tga->id_len;
                break;
            default:
                if (has_colormap) {
                    return nullptr;
                }
                break;
        }

        // get pixel array offset
        const size_t data_offset = sizeof(Header) + tga->id_len + colormap_sz;
        if (data_offset >= data.size) {
            return nullptr;
        }
        const uint8_t* pix_ptr = data.data + data_offset;
        const size_t pix_sz = data.size - data_offset;

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(tga->bpp == 32 ? Pixmap::ARGB : Pixmap::RGB, tga->width,
                  tga->height);
        bool rc = false;
        switch (tga->image_type) {
            case TGA_UNC_CM:
            case TGA_UNC_TC:
            case TGA_UNC_GS:
                rc = decode_unc(pm, tga, colormap, pix_ptr, pix_sz);
                break;
            case TGA_RLE_CM:
            case TGA_RLE_TC:
            case TGA_RLE_GS:
                rc = decode_rle(pm, tga, colormap, pix_ptr, pix_sz);
                break;
        }
        if (!rc) {
            return nullptr;
        }

        // fix orientation
        if (!(tga->desc & TGA_ORDER_T2B)) {
            pm.flip_vertical();
        }
        if (tga->desc & TGA_ORDER_R2L) {
            pm.flip_horizontal();
        }

        // set image format info
        image->format = std::format("TARGA {}bpp, ", tga->bpp);
        switch (tga->image_type) {
            case TGA_UNC_CM:
                image->format += "uncompressed color-mapped";
                break;
            case TGA_UNC_TC:
                image->format += "uncompressed true-color";
                break;
            case TGA_UNC_GS:
                image->format += "uncompressed grayscale";
                break;
            case TGA_RLE_CM:
                image->format += "RLE color-mapped";
                break;
            case TGA_RLE_TC:
                image->format += "RLE true-color";
                break;
            case TGA_RLE_GS:
                image->format += "RLE grayscale";
                break;
        }

        return image;
    }

private:
    // TGA image type constants
    static constexpr uint8_t TGA_UNC_CM = 1;  // uncompressed color-mapped
    static constexpr uint8_t TGA_UNC_TC = 2;  // uncompressed true-color
    static constexpr uint8_t TGA_UNC_GS = 3;  // uncompressed grayscale
    static constexpr uint8_t TGA_RLE_CM = 9;  // run-length encoded color-mapped
    static constexpr uint8_t TGA_RLE_TC = 10; // run-length encoded true-color
    static constexpr uint8_t TGA_RLE_GS = 11; // run-length encoded grayscale

    // TGA descriptor flags
    static constexpr uint8_t TGA_COLORMAP = 1;         // color map present flag
    static constexpr uint8_t TGA_ORDER_R2L = (1 << 4); // right-to-left
    static constexpr uint8_t TGA_ORDER_T2B = (1 << 5); // top-to-bottom
    static constexpr uint8_t TGA_PACKET_RLE = (1 << 7); // rle/raw field
    static constexpr uint8_t TGA_PACKET_LEN = 0x7f;     // length mask

    /** TGA file header. */
    struct __attribute__((__packed__)) Header {
        uint8_t id_len;
        uint8_t clrmap_type;
        uint8_t image_type;

        uint16_t cm_index;
        uint16_t cm_size;
        uint8_t cm_bpc;

        uint16_t origin_x;
        uint16_t origin_y;
        uint16_t width;
        uint16_t height;
        uint8_t bpp;
        uint8_t desc;
    };

    /**
     * Get pixel color from data stream.
     * @param data pointer to stream data
     * @param bpp bits per pixel
     * @return color
     */
    inline argb_t get_pixel(const uint8_t* data, const size_t bpp) const
    {
        argb_t pixel;

        switch (bpp) {
            case 8:
                pixel.a = argb_t::max;
                pixel.r = data[0];
                pixel.g = data[0];
                pixel.b = data[0];
                break;
            case 15:
            case 16: {
                const uint16_t src = *reinterpret_cast<const uint16_t*>(data);
                const uint8_t bit_repeat = argb_t::max / 31;
                pixel.b = (src & 0x001f) * bit_repeat;
                pixel.g = ((src & 0x03e0) >> 5) * bit_repeat;
                pixel.r = ((src & 0x7c00) >> 10) * bit_repeat;
                pixel.a = argb_t::max;
            } break;
            case 24:
                pixel.a = argb_t::max;
                pixel.r = data[2];
                pixel.g = data[1];
                pixel.b = data[0];
                break;
            default:
                pixel = *reinterpret_cast<const uint32_t*>(data);
                break;
        }

        return pixel;
    }

    /**
     * Decode uncompressed image.
     * @param pm destination pixmap
     * @param tga source image descriptor
     * @param colormap color map
     * @param data pointer to image data
     * @param size size of image data in bytes
     * @return true if image decoded successfully
     */
    bool decode_unc(Pixmap& pm, const Header* tga, const uint8_t* colormap,
                    const uint8_t* data, const size_t size) const
    {
        const uint8_t bytes_per_pixel = tga->bpp / 8 + (tga->bpp % 8 ? 1 : 0);
        const size_t num_pixels = pm.width() * pm.height();
        const size_t data_size = num_pixels * bytes_per_pixel;

        if (data_size > size) {
            return false;
        }

        if (tga->bpp == 32) {
            std::memcpy(pm.ptr(0, 0), data, data_size);
        } else {
            const uint8_t cm_bpp = tga->cm_bpc / 8 + (tga->cm_bpc % 8 ? 1 : 0);
            const uint8_t* src = data;
            for (size_t y = 0; y < pm.height(); ++y) {
                for (size_t x = 0; x < pm.width(); ++x) {
                    argb_t color;
                    if (!colormap) {
                        color = get_pixel(src, tga->bpp);
                    } else {
                        const uint8_t* entry = colormap + cm_bpp * (*src);
                        if (entry + cm_bpp > data) {
                            return false;
                        }
                        color = get_pixel(entry, tga->cm_bpc);
                    }
                    pm.at(x, y) = color;
                    src += bytes_per_pixel;
                }
            }
        }

        return true;
    }

    /**
     * Decode RLE compressed image.
     * @param pm destination pixmap
     * @param tga source image descriptor
     * @param colormap color map
     * @param data pointer to image data
     * @param size size of image data in bytes
     * @return true if image decoded successfully
     */
    bool decode_rle(Pixmap& pm, const Header* tga, const uint8_t* colormap,
                    const uint8_t* data, const size_t size) const
    {
        const uint8_t cm_bpp = tga->cm_bpc / 8 + (tga->cm_bpc % 8 ? 1 : 0);
        const uint8_t bytes_per_pixel = tga->bpp / 8 + (tga->bpp % 8 ? 1 : 0);
        const argb_t* pm_end = &pm.at(0, 0) + pm.width() * pm.height();
        argb_t* pixel = &pm.at(0, 0);
        size_t pos = 0;

        while (pixel < pm_end) {
            const uint8_t pack = data[pos++];
            const bool is_rle = (pack & TGA_PACKET_RLE);
            size_t len = (pack & TGA_PACKET_LEN) + 1;

            while (len--) {
                if (pos + bytes_per_pixel > size) {
                    return false;
                }

                if (!colormap) {
                    *pixel = get_pixel(data + pos, tga->bpp);
                } else {
                    const uint8_t* entry = colormap + cm_bpp * data[pos];
                    if (entry + cm_bpp > data) {
                        return false;
                    }
                    *pixel = get_pixel(entry, tga->cm_bpc);
                }

                if (pixel++ >= pm_end) {
                    break;
                }
                if (!is_rle) {
                    pos += bytes_per_pixel;
                }
            }
            if (is_rle) {
                pos += bytes_per_pixel;
            }
        }

        return true;
    }

    /**
     * Validate TGA header parameters.
     * @param tga pointer to TGA header
     * @return true if valid
     */
    bool validate_header(const Header* tga) const
    {
        // check basic image parameters
        if (tga->width == 0 || tga->height == 0) {
            return false;
        }

        // check bits per pixel
        if (tga->bpp != 8 && tga->bpp != 15 && tga->bpp != 16 &&
            tga->bpp != 24 && tga->bpp != 32) {
            return false;
        }

        // check image type
        switch (tga->image_type) {
            case TGA_UNC_CM:
            case TGA_UNC_TC:
            case TGA_UNC_GS:
            case TGA_RLE_CM:
            case TGA_RLE_TC:
            case TGA_RLE_GS:
                return true;
        }

        return false;
    }
};

// register format in factory
static ImageFormatTga format_tga;

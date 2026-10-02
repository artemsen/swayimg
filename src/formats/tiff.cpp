// SPDX-License-Identifier: MIT
// TIFF image format.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <tiffio.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <memory>
#include <vector>

namespace {

class ImageFormatTiff : public ImageFormat {
public:
    ImageFormatTiff() noexcept
        : ImageFormat(Priority::Low, "tiff")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        const bool le = check_signature(data, SIG_LE);
        if (!le && !check_signature(data, SIG_BE)) {
            return nullptr;
        }

        // suppress error messages
        TIFFSetErrorHandler(nullptr);
        TIFFSetWarningHandler(nullptr);

        BufferIO bio(data);
        const TiffImage tiff(TIFFClientOpen("", "r", &bio, &BufferIO::read,
                                            &BufferIO::write, &BufferIO::seek,
                                            &BufferIO::close, &BufferIO::size,
                                            &BufferIO::map, &BufferIO::unmap),
                             &TIFFClose);
        if (!tiff) {
            return nullptr;
        }

        // get image size
        uint32_t width;
        uint32_t height;
        if (!TIFFGetField(tiff.get(), TIFFTAG_IMAGEWIDTH, &width) ||
            !TIFFGetField(tiff.get(), TIFFTAG_IMAGELENGTH, &height)) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::ARGB, width, height);

        // firstly try to decode via the standard RGBA path, but it does not
        // support some formats (e.g. 12-bit grayscale or IEEE float samples),
        // so try to decode them manually as fallback
        if (!decode_rgba(tiff.get(), pm) &&
            !decode_manual(tiff.get(), le, pm)) {
            return nullptr;
        }

        uint16_t bits = 0;
        TIFFGetField(tiff.get(), TIFFTAG_BITSPERSAMPLE, &bits);
        image->format = std::format("TIFF {}bits", bits);

        return image;
    }

private:
    // Tiff image wrapper
    using TiffImage = std::unique_ptr<TIFF, decltype(&TIFFClose)>;

    // Tiff signatures (LE/BE)
    static constexpr const uint8_t SIG_LE[] = { 0x49, 0x49, 0x2a, 0x00 };
    static constexpr const uint8_t SIG_BE[] = { 0x4d, 0x4d, 0x00, 0x2a };

    /**
     * Decode image using native RGBA decoder.
     * @param tiff decoder handle
     * @param pm destination pixmap
     * @return true if image was decoded successfully
     */
    static bool decode_rgba(TIFF* tiff, Pixmap& pm)
    {
        uint16_t orientation = ORIENTATION_TOPLEFT;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_ORIENTATION, &orientation);

        if (!TIFFReadRGBAImageOriented(
                tiff, pm.width(), pm.height(),
                reinterpret_cast<uint32_t*>(pm.ptr(0, 0)), orientation, 1)) {
            return false;
        }

        pm.abgr_to_argb();
        return true;
    }

    /**
     * Manually decode image.
     * @param tiff decoder handle
     * @param little_endian byte order of image data
     * @param pm destination pixmap
     * @return true if image was decoded successfully
     */
    static bool decode_manual(TIFF* tiff, const bool little_endian, Pixmap& pm)
    {
        uint16_t bits = 0;
        uint16_t spp = 0;
        uint16_t photometric = 0;
        uint16_t fill_order = 0;
        uint16_t sample_format = 0;
        if (!TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bits) ||
            !TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &spp) ||
            !TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photometric)) {
            return false;
        }
        TIFFGetFieldDefaulted(tiff, TIFFTAG_FILLORDER, &fill_order);

        const bool is_subbyte_gray = spp != 0 && bits != 0 && bits < 16 &&
            bits % 8 != 0 && photometric <= PHOTOMETRIC_MINISBLACK;
        const bool is_float =
            TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sample_format) &&
            sample_format == SAMPLEFORMAT_IEEEFP;
        if (!is_subbyte_gray && !is_float) {
            return false;
        }

        // calculate strip layout
        uint32_t rows_per_strip = 0;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
        if (rows_per_strip == 0) {
            rows_per_strip = pm.height();
        }
        const tmsize_t strip_size = TIFFStripSize(tiff);
        if (strip_size <= 0) {
            return false;
        }
        const size_t row_stride = (pm.width() * bits * spp + 7) / 8;

        std::vector<uint8_t> strip_buf(strip_size);
        const bool lsb_first = fill_order == FILLORDER_LSB2MSB;

        // decode strips
        for (uint32_t strip = 0; strip < TIFFNumberOfStrips(tiff); ++strip) {
            if (TIFFReadEncodedStrip(tiff, strip, strip_buf.data(),
                                     strip_size) <= 0) {
                return false;
            }
            const uint32_t row = strip * rows_per_strip;
            const uint32_t rows = std::min(
                rows_per_strip, static_cast<uint32_t>(pm.height()) - row);
            for (uint32_t ry = 0; ry < rows; ++ry) {
                const uint8_t* src = strip_buf.data() + ry * row_stride;
                argb_t* dst = &pm.at(0, row + ry);
                decode_strip(src, dst, pm.width(), spp, bits, photometric,
                             lsb_first, little_endian, is_float);
            }
        }

        return true;
    }

    /**
     * Decode single strip.
     * @param src strip line buffer
     * @param dst destination buffer
     * @param width line width
     * @param spp samples per pixel
     * @param bits bits per channel (sample)
     * @param photometric photometric interpretation
     * @param lsb_first bit order, true if least significant bit first
     * @param little_endian byte order of image data
     * @param is_float source data type
     */
    static void decode_strip(const uint8_t* src, argb_t* dst,
                             const size_t width, const uint16_t spp,
                             const uint16_t bits, const uint16_t photometric,
                             const bool lsb_first, const bool little_endian,
                             const bool is_float)
    {
        for (size_t x = 0; x < width; ++x) {
            const size_t pixel_bit = x * bits * spp;
            if (spp == 3 && (photometric == PHOTOMETRIC_RGB)) {
                const uint8_t r = sample_to_byte(
                    src, pixel_bit, bits, lsb_first, little_endian, is_float);
                const uint8_t g =
                    sample_to_byte(src, pixel_bit + bits, bits, lsb_first,
                                   little_endian, is_float);
                const uint8_t b = sample_to_byte(
                    src, pixel_bit + static_cast<size_t>(bits * 2), bits,
                    lsb_first, little_endian, is_float);
                dst[x] = argb_t(argb_t::max, r, g, b);
            } else {
                uint8_t gray = sample_to_byte(src, pixel_bit, bits, lsb_first,
                                              little_endian, is_float);
                if (!photometric) { // white is zero
                    gray = 255 - gray;
                }
                dst[x] = argb_t(argb_t::max, gray, gray, gray);
            }
        }
    }

    /**
     * Unpack bit field from a byte buffer.
     * @param data source buffer
     * @param bit start bit position
     * @param count number of bits to read (up to 15)
     * @param lsb_first bit order, true if least significant bit first
     * @return unpacked value
     */
    static uint32_t unpack_bits(const uint8_t* data, const size_t bit,
                                const size_t count, const bool lsb_first)
    {
        uint32_t value = 0;
        for (size_t i = 0; i < count; ++i) {
            const size_t src_bit = bit + i;
            const uint8_t byte = data[src_bit / 8];
            uint32_t bitv;
            if (lsb_first) {
                bitv = (byte >> (src_bit % 8)) & 1;
                value |= bitv << i;
            } else {
                bitv = (byte >> (7 - src_bit % 8)) & 1;
                value = (value << 1) | bitv;
            }
        }
        return value;
    }

    /**
     * Read a byte-aligned value from a buffer into host order.
     * @param data source buffer
     * @param offset byte offset of the value
     * @param size value size in bytes
     * @param little_endian byte order of the source data
     * @return value in host byte order
     */
    static uint32_t read_value(const uint8_t* data, const size_t offset,
                               const size_t size, const bool little_endian)
    {
        if (size == 1) {
            return data[offset];
        }
        const uint16_t probe = 1;
        const bool host_little = *reinterpret_cast<const uint8_t*>(&probe) == 1;
        uint32_t value = 0;
        if (little_endian == host_little) {
            for (size_t i = 0; i < size; ++i) {
                value |= static_cast<uint32_t>(data[offset + i]) << (8 * i);
            }
        } else {
            for (size_t i = 0; i < size; ++i) {
                value = (value << 8) | data[offset + i];
            }
        }
        return value;
    }

    /**
     * Convert IEEE 754 half precision (16-bit) value to {@c float}.
     * @param half raw half precision bit pattern
     * @return float value
     */
    static float half_to_float(const uint16_t half)
    {
        const uint32_t sign = static_cast<uint32_t>(half & 0x8000) << 16;
        uint32_t exp = (half >> 10) & 0x1f;
        uint32_t mant = half & 0x3ff;
        uint32_t value;
        if (exp == 0) {
            if (mant == 0) {
                value = sign; // +/- zero
            } else {
                // subnormal
                exp = 0;
                while (!(mant & 0x400)) {
                    mant <<= 1;
                    --exp;
                }
                mant &= 0x3ff;
                value = sign | ((exp + 127) << 23) | (mant << 13);
            }
        } else if (exp == 0x1f) {
            value = sign | 0x7f800000 | (mant << 13); // inf / nan
        } else {
            value = sign | ((exp + 112) << 23) | (mant << 13);
        }
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    /**
     * Convert one TIFF sample to a normalized 8-bit channel value.
     * @param src row buffer
     * @param sample global bit position of the sample within the row
     * @param bits bits per sample
     * @param lsb_first bit order (sub-byte samples only)
     * @param little_endian byte order of the source data
     * @param is_float true if sample is IEEE floating point
     * @return normalized 8-bit channel value
     */
    static uint8_t sample_to_byte(const uint8_t* src, const size_t sample_bit,
                                  const uint16_t bits, const bool lsb_first,
                                  const bool little_endian, const bool is_float)
    {
        if (is_float) {
            float v;
            if (bits == 16) {
                const uint16_t half = static_cast<uint16_t>(
                    read_value(src, sample_bit / 8, 2, little_endian));
                v = half_to_float(half);
            } else {
                const uint32_t raw =
                    read_value(src, sample_bit / 8, bits / 8, little_endian);
                std::memcpy(&v, &raw, sizeof(v));
            }
            if (v < 0.0) {
                v = 0.0;
            } else if (v > 1.0) {
                v = 1.0;
            }
            return static_cast<uint8_t>(std::lround(v * 255.0));
        }

        const size_t sample_mask = (static_cast<size_t>(1) << bits) - 1;
        uint32_t sample =
            unpack_bits( // NOLINT(readability-suspicious-call-argument)
                src, sample_bit, bits, lsb_first);
        sample &= sample_mask;
        return static_cast<uint8_t>(
            (static_cast<uint64_t>(sample) * 255 + sample_mask / 2) /
            sample_mask);
    }

    /** Memory buffer I/O. */
    struct BufferIO {
        BufferIO(const Data& raw_data)
            : data(raw_data)
        {
        }

        /** Buffer reader: see TIFFReadWriteProc for details. */
        static tmsize_t read(thandle_t data, void* buffer, tmsize_t size)
        {
            BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            size = std::min(size,
                            static_cast<tmsize_t>(bufio->data.size) -
                                static_cast<tmsize_t>(bufio->position));
            std::memcpy(buffer, bufio->data.data + bufio->position, size);
            bufio->position += size;
            return size;
        }

        /** Buffer writer: see TIFFReadWriteProc for details. */
        static tmsize_t write(thandle_t, void*, tmsize_t) { return 0; }

        /** Buffer seek: see TIFFSeekProc for details. */
        static toff_t seek(thandle_t data, toff_t off, int)
        {
            BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            if (off < bufio->data.size) {
                bufio->position = off;
            }
            return bufio->position;
        }

        /** Buffer close: see TIFFCloseProc for details. */
        static int close(thandle_t) { return 0; }

        /** Buffer size getter: see TIFFSizeProc for details. */
        static toff_t size(thandle_t data)
        {
            const BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            return bufio->data.size;
        }

        /** Buffer map: see TIFFMapFileProc for details. */
        static int map(thandle_t data, void** base, toff_t* size)
        {
            const BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            *base = const_cast<uint8_t*>(bufio->data.data);
            *size = bufio->data.size;
            return 0;
        }

        /** Buffer unmap: see TIFFUnmapFileProc for details. */
        static void unmap(thandle_t, void*, toff_t) {}

        const Data& data;
        size_t position = 0;
    };
};

// register format in factory
ImageFormatTiff format_tiff;

} // anonymous namespace

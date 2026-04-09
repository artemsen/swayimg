// SPDX-License-Identifier: MIT
// TIFF image format.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <tiffio.h>

#include <cstring>
#include <memory>

class ImageFormatTiff : public ImageFormat {
public:
    ImageFormatTiff()
        : ImageFormat(Priority::Low, "tiff")
    {
    }

    // Size of buffer for error messages, see libtiff for details
    static constexpr size_t LIBTIFF_ERRMSG_SZ = 1024;

    ImagePtr decode(const Data& data) override
    {
        if (!check_signature(data, { 0x49, 0x49, 0x2a, 0x00 }) &&
            !check_signature(data, { 0x4d, 0x4d, 0x00, 0x2a })) {
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
        uint32_t width, height;
        if (!TIFFGetField(tiff.get(), TIFFTAG_IMAGEWIDTH, &width) ||
            !TIFFGetField(tiff.get(), TIFFTAG_IMAGELENGTH, &height)) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::ARGB, width, height);

        // decode image
        TIFFReadRGBAImageOriented(tiff.get(), width, height,
                                  reinterpret_cast<uint32_t*>(pm.ptr(0, 0)),
                                  ORIENTATION_TOPLEFT, 1);
        pm.abgr_to_argb();

        // something strange, but i don't know how to deal with it
        uint32_t orientation;
        if (TIFFGetField(tiff.get(), TIFFTAG_ORIENTATION, &orientation)) {
            if (orientation == ORIENTATION_RIGHTTOP) {
                image->flip_horizontal();
            }
        }

        image->format = "TIFF";

        return image;
    }

private:
    // Tiff image wrapper
    using TiffImage = std::unique_ptr<TIFF, decltype(&TIFFClose)>;

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
            const tmsize_t rest =
                static_cast<tmsize_t>(bufio->data.size) - bufio->position;
            if (size > rest) {
                size = rest;
            }
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
static ImageFormatTiff format_tiff;

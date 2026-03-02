// SPDX-License-Identifier: MIT
// TIFF format decoder.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <tiffio.h>

#include <cstring>
#include <format>

// register format in factory
class ImageTiff;
static const ImageLoader::Registrator<ImageTiff>
    image_format_registartion("TIFF", ImageLoader::Priority::Low);

/** Memory buffer I/O. */
struct BufferIO {
    BufferIO(const std::vector<uint8_t>& raw_data)
        : data(raw_data)
    {
    }

    /** Buffer reader: see TIFFReadWriteProc for details. */
    static tmsize_t read(thandle_t data, void* buffer, tmsize_t size)
    {
        BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
        const tmsize_t rest =
            static_cast<tmsize_t>(bufio->data.size()) - bufio->position;
        if (size > rest) {
            size = rest;
        }
        std::memcpy(buffer, bufio->data.data() + bufio->position, size);
        bufio->position += size;
        return size;
    }

    /** Buffer writer: see TIFFReadWriteProc for details. */
    static tmsize_t write(thandle_t, void*, tmsize_t) { return 0; }

    /** Buffer seek: see TIFFSeekProc for details. */
    static toff_t seek(thandle_t data, toff_t off, int)
    {
        BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
        if (off < bufio->data.size()) {
            bufio->position = off;
        }
        return bufio->position;
    }

    /** Buffer close: see TIFFCloseProc for details. */
    static int close(thandle_t) { return 0; }

    /** Buffer size getter: see TIFFSizeProc for details. */
    static toff_t size(thandle_t data)
    {
        BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
        return bufio->data.size();
    }

    /** Buffer map: see TIFFMapFileProc for details. */
    static int map(thandle_t data, void** base, toff_t* size)
    {
        BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
        *base = const_cast<uint8_t*>(bufio->data.data());
        *size = bufio->data.size();
        return 0;
    }

    /** Buffer unmap: see TIFFUnmapFileProc for details. */
    static void unmap(thandle_t, void*, toff_t) { }

    const std::vector<uint8_t>& data;
    size_t position = 0;
};

/** TIFF decoder wrapper. */
class Tiff {
public:
    ~Tiff()
    {
        if (tiff) {
            TIFFRGBAImageEnd(&img);
            TIFFClose(tiff);
        }
    }

    operator TIFF*() { return tiff; }
    operator TIFFRGBAImage*() { return &img; }

    TIFF* tiff = nullptr;
    TIFFRGBAImage img = {};
};

/* TIFF image. */
class ImageTiff : public Image {
private:
    // TIFF signatures
    static constexpr const uint8_t signature1[] = { 0x49, 0x49, 0x2a, 0x00 };
    static constexpr const uint8_t signature2[] = { 0x4d, 0x4d, 0x00, 0x2a };

    // Size of buffer for error messages, see libtiff for details
    static constexpr size_t LIBTIFF_ERRMSG_SZ = 1024;

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(signature1) ||
            data.size() < sizeof(signature2) ||
            (std::memcmp(data.data(), signature1, sizeof(signature1)) &&
             std::memcmp(data.data(), signature2, sizeof(signature2)))) {
            return false;
        }

        // suppress error messages
        TIFFSetErrorHandler(nullptr);
        TIFFSetWarningHandler(nullptr);

        Tiff tiff;
        BufferIO bio(data);

        tiff.tiff =
            TIFFClientOpen("", "r", &bio, &BufferIO::read, &BufferIO::write,
                           &BufferIO::seek, &BufferIO::close, &BufferIO::size,
                           &BufferIO::map, &BufferIO::unmap);
        if (!tiff.tiff) {
            return false;
        }

        char err[LIBTIFF_ERRMSG_SZ] = { 0 };
        if (!TIFFRGBAImageBegin(tiff, tiff, 0, err)) {
            return false;
        }

        // decode image
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::ARGB, tiff.img.width, tiff.img.height);
        if (!TIFFRGBAImageGet(tiff, reinterpret_cast<uint32_t*>(pm.ptr(0, 0)),
                              tiff.img.width, tiff.img.height)) {
            return false;
        }
        pm.abgr_to_argb();

        if (tiff.img.orientation == ORIENTATION_TOPLEFT) {
            flip_vertical();
        }

        format = std::format("TIFF {}bpp",
                             tiff.img.bitspersample * tiff.img.samplesperpixel);

        return true;
    }
};

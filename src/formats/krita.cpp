// SPDX-License-Identifier: MIT
// Krita image format.
// Copyright (C) 2026 Philip Kranz <pk@pmlk.net>

#include "../formatfactory.hpp"
#include "../imageformat.hpp"

#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_zip.h>

#include <cassert>
#include <vector>

namespace {

class ImageFormatKrita : public ImageFormat {
public:
    ImageFormatKrita() noexcept
        : ImageFormat(Priority::Low, "kra")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        if (!check_signature(data, signature)) {
            return nullptr;
        }

        // unpack png from zip stream
        Zip zip;
        std::vector<uint8_t> img_data = zip.unpack(data, "mergedimage.png");
        if (img_data.empty()) {
            return nullptr;
        }

        // decode unpacked png
        const Data png_data = {
            .data = img_data.data(),
            .size = img_data.size(),
        };
        ImagePtr image = FormatFactory::self().get("png")->decode(png_data);

        if (image) {
            image->format = "Krita";
        }
        return image;
    }

    [[nodiscard]] Pixmap preview(const Data& data, const size_t sz,
                                 const bool fill) const override
    {
        if (!check_signature(data, signature) ||
            !FormatFactory::self().embedded_thumb) {
            return ImageFormat::preview(data, sz, fill);
        }

        // unpack png from zip stream
        Zip zip;
        std::vector<uint8_t> img_data = zip.unpack(data, "preview.png");
        if (img_data.empty()) {
            return ImageFormat::preview(data, sz, fill);
        }

        // decode unpacked png
        const Data png_data = {
            .data = img_data.data(),
            .size = img_data.size(),
        };
        return FormatFactory::self().get("png")->preview(png_data, sz, fill);
    }

private:
    // zip signature
    static constexpr uint8_t signature[] = { 'P', 'K', 0x03, 0x04 };

    /** Wrapper to work with Zip stream. */
    class Zip {
    public:
        ~Zip()
        {
            if (zip) {
                if (mz_zip_entry_is_open(zip)) {
                    mz_zip_entry_close(zip);
                }
                mz_zip_close(zip);
                mz_zip_delete(&zip);
            }
            if (stream) {
                mz_stream_mem_delete(&stream);
            }
        }

        /**
         * Read content of one file from a zip stream.
         * @param data zip stream data
         * @param filename path of the file to deflate
         * @return content of deflated file
         */
        std::vector<uint8_t> unpack(const Data& data, const char* filename)
        {
            assert(!stream && !zip);

            // create stream
            stream = mz_stream_mem_create();
            if (!stream) {
                return {};
            }
            mz_stream_mem_set_buffer(stream, const_cast<uint8_t*>(data.data),
                                     data.size);
            if (mz_stream_open(stream, nullptr, MZ_OPEN_MODE_READ) != MZ_OK) {
                return {};
            }

            // open stream as zip
            zip = mz_zip_create();
            if (!zip || mz_zip_open(zip, stream, MZ_OPEN_MODE_READ) != MZ_OK) {
                return {};
            }

            // load file entry
            if (mz_zip_locate_entry(zip, filename, 0) != MZ_OK ||
                mz_zip_entry_read_open(zip, 0, nullptr) != MZ_OK) {
                return {};
            }

            // deflate entry
            std::vector<uint8_t> unpacked;
            while (true) {
                uint8_t buf[4096];
                const int32_t len = mz_zip_entry_read(zip, buf, sizeof(buf));
                if (len < 0) {
                    return {};
                }
                if (len == 0) {
                    break; // eof
                }
                unpacked.insert(unpacked.end(), buf, buf + len);
            }
            return unpacked;
        }

    private:
        void* stream = nullptr;
        void* zip = nullptr;
    };
};

// register format in factory
ImageFormatKrita format_krita;

} // anonymous namespace

#include "../formatfactory.hpp"
#include "../imageformat.hpp"
#include "../log.hpp"

#include <optional>
#include <vector>

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_mem.h"
#include "mz_zip.h"

namespace {

class ImageFormatKrita : public ImageFormat {
public:
    ImageFormatKrita() noexcept
        : ImageFormat(Priority::Lowest, "kra")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        std::optional<std::vector<uint8_t>> png =
            read_from_zip(data, "mergedimage.png");

        if (!png.has_value()) {
            return nullptr;
        }

        const ImageFormat* png_loader = FormatFactory::self().get("png");

        if (png_loader == nullptr) {
            return nullptr;
        }

        Data png_data;
        png_data.data = png.value().data();
        png_data.size = png.value().size();

        return png_loader->decode(png_data);
    }

    [[nodiscard]] Pixmap preview(const Data& data, const size_t sz,
                                 const bool fill) const override
    {
        if (!FormatFactory::self().embedded_thumb) {
            return ImageFormat::preview(data, sz, fill);
        }

        std::optional<std::vector<uint8_t>> png =
            read_from_zip(data, "preview.png");

        if (!png.has_value()) {
            return ImageFormat::preview(data, sz, fill);
        }

        const ImageFormat* png_loader = FormatFactory::self().get("png");

        if (png_loader == nullptr) {
            return ImageFormat::preview(data, sz, fill);
        }

        Data png_data;
        png_data.data = png.value().data();
        png_data.size = png.value().size();

        return png_loader->preview(png_data, sz, fill);
    }

private:
    static void delete_memstream(void* s) { mz_stream_mem_delete(&s); }
    static void delete_zip(void* s) { mz_zip_delete(&s); }

    using MemStream = std::unique_ptr<void, decltype(&delete_memstream)>;
    using ZipHandle = std::unique_ptr<void, decltype(&delete_zip)>;

    /**
     * Read one file from a zip file into a vector
     * @param data zip file buffer
     * @param filename path of the file to deflate
     * @return contents of deflated file
     */
    static std::optional<std::vector<uint8_t>>
    read_from_zip(const Data& data, const char* filename)
    {
        int32_t err = MZ_OK;

        const MemStream mem_stream(mz_stream_mem_create(), &delete_memstream);

        mz_stream_mem_set_buffer(mem_stream.get(), data.data, data.size);

        if (mz_stream_open(mem_stream.get(), nullptr, MZ_OPEN_MODE_READ) !=
            MZ_OK) {
            return std::nullopt;
        }

        const ZipHandle zip_handle(mz_zip_create(), &delete_zip);

        if (mz_zip_open(zip_handle.get(), mem_stream.get(),
                        MZ_OPEN_MODE_READ) != MZ_OK) {
            return std::nullopt;
        }

        if (mz_zip_locate_entry(zip_handle.get(), filename, 0) != MZ_OK) {
            mz_zip_close(zip_handle.get());
            return std::nullopt;
        }

        if (mz_zip_entry_read_open(zip_handle.get(), 0, nullptr) != MZ_OK) {
            mz_zip_close(zip_handle.get());
            return std::nullopt;
        }

        std::vector<uint8_t> result;
        std::vector<uint8_t> buffer(4096);

        int32_t bytes_read;
        do {
            bytes_read = mz_zip_entry_read(
                zip_handle.get(), reinterpret_cast<char*>(buffer.data()),
                buffer.capacity());

            if (bytes_read < 0) {
                err = bytes_read;
            } else {
                result.insert(result.end(), buffer.begin(), buffer.end());
            }
        } while (err == MZ_OK && bytes_read > 0);

        mz_zip_entry_close(zip_handle.get());
        mz_zip_close(zip_handle.get());

        return result;
    }
};

ImageFormatKrita format_krita;

}

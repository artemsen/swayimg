// SPDX-License-Identifier: MIT
// GIF image format.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <gif_lib.h>

#include <cstring>
#include <utility>

namespace {

class ImageFormatGif : public ImageFormat {
public:
    ImageFormatGif() noexcept
        : ImageFormat(Priority::Normal, "gif")
    {
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        if (!check_signature(data, { 'G', 'I', 'F' })) {
            return nullptr;
        }

        // open decoder
        int err;
        BufferReader buf_reader(data);
        Gif gif(DGifOpen(&buf_reader, &BufferReader::read, &err));
        if (!gif) {
            return nullptr;
        }
        if (DGifSlurp(gif) != GIF_OK) {
            return nullptr;
        }
        if (gif->ImageCount <= 0 || gif->SWidth <= 0 || gif->SHeight <= 0) {
            return nullptr; // no frames or empty canvas
        }

        // allocate image and frames
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(gif->ImageCount);
        for (auto& it : image->frames) {
            it.pm.create(Pixmap::ARGB, gif->SWidth, gif->SHeight);
        }

        // decode frames
        for (size_t i = 0; i < image->frames.size(); ++i) {
            decode_frame(gif, image->frames, i);
        }

        image->format = "GIF";
        if (gif->ImageCount > 1) {
            image->format += " animation";
        }

        return image;
    }

private:
    /** Memory buffer reader. */
    struct BufferReader {
        BufferReader(const Data& raw_data)
            : data(raw_data)
        {
        }

        // GIF reader callback, see `InputFunc` in gif_lib.h
        static int read(GifFileType* gif, GifByteType* dst, int sz)
        {
            BufferReader* reader =
                reinterpret_cast<BufferReader*>(gif->UserData);
            if (reader && sz >= 0 &&
                reader->position + sz <= reader->data.size) {
                std::memcpy(dst, reader->data.data + reader->position, sz);
                reader->position += sz;
                return sz;
            }
            return -1;
        }

        const Data& data;
        size_t position = 0;
    };

    /** GIF decoder wrapper. */
    class Gif {
    public:
        Gif(GifFileType* ptr)
            : gif(ptr)
        {
        }

        ~Gif()
        {
            if (gif) {
                DGifCloseFile(gif, nullptr);
            }
        }

        operator GifFileType*() const { return gif; }
        GifFileType* operator->() const { return gif; }

        GifFileType* gif;
    };

    /**
     * Decode single GIF frame.
     * @param gif GIF decoder
     * @param frames all image frames
     * @param index number of the frame to load
     */
    static void decode_frame(Gif& gif, std::vector<Image::Frame>& frames,
                             const size_t index)
    {
        Image::Frame& frame = frames[index];

        GraphicsControlBlock ctl {};
        ctl.TransparentColor = NO_TRANSPARENT_COLOR;
        DGifSavedExtensionToGCB(gif, index, &ctl);

        // handle disposition
        if (ctl.DisposalMode == DISPOSE_PREVIOUS && index + 1 < frames.size()) {
            const Pixmap& curr = frame.pm;
            Pixmap& next = frames[index + 1].pm;
            next.copy(curr, { .x = 0, .y = 0 });
        }

        const SavedImage* gif_img = &gif->SavedImages[index];
        const GifImageDesc* desc = &gif_img->ImageDesc;
        const ColorMapObject* color_map =
            desc->ColorMap ? desc->ColorMap : gif->SColorMap;
        if (!color_map) {
            return;
        }

        // clip the frame rectangle to the canvas: position and size of the
        // frame come from the file and may lie partly or fully outside it,
        // the raster (Width x Height bytes) is never read past its end
        const bool valid = gif_img->RasterBits && desc->Left >= 0 &&
            desc->Top >= 0 && desc->Width > 0 && desc->Height > 0;
        const size_t left = valid ? desc->Left : 0;
        const size_t top = valid ? desc->Top : 0;
        const size_t raster_width = valid ? desc->Width : 0;
        const size_t raster_height = valid ? desc->Height : 0;
        const size_t width = left < frame.pm.width()
            ? std::min(raster_width, frame.pm.width() - left)
            : 0;
        const size_t height = top < frame.pm.height()
            ? std::min(raster_height, frame.pm.height() - top)
            : 0;
        for (size_t y = 0; y < height; ++y) {
            const uint8_t* raster = &gif_img->RasterBits[y * raster_width];
            for (size_t x = 0; x < width; ++x) {
                argb_t& pixel = frame.pm.at(x + left, y + top);
                const uint8_t color = raster[x];
                if (std::cmp_not_equal(color, ctl.TransparentColor) &&
                    std::cmp_less(color, color_map->ColorCount)) {
                    const GifColorType* rgb = &color_map->Colors[color];
                    pixel.a = argb_t::max;
                    pixel.r = rgb->Red;
                    pixel.g = rgb->Green;
                    pixel.b = rgb->Blue;
                }
            }
        }

        if (ctl.DisposalMode == DISPOSE_DO_NOT && index + 1 < frames.size()) {
            const Pixmap& curr = frame.pm;
            Pixmap& next = frames[index + 1].pm;
            next.copy(curr, { .x = 0, .y = 0 });
        }

        if (ctl.DelayTime != 0) {
            // hundreds of second to ms
            frame.duration = static_cast<size_t>(ctl.DelayTime) * 10;
        } else {
            frame.duration = 100;
        }
    }
};

// register format in factory
ImageFormatGif format_gif;

} // anonymous namespace

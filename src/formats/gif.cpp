// SPDX-License-Identifier: MIT
// GIF format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <gif_lib.h>

#include <cstring>

// register format in factory
class ImageGif;
static const ImageLoader::Registrator<ImageGif>
    image_format_registartion("GIF", ImageLoader::Priority::Normal);

/** Memory buffer reader. */
struct BufferReader {
    BufferReader(const std::vector<uint8_t>& raw_data)
        : data(raw_data)
    {
    }

    // GIF reader callback, see `InputFunc` in gif_lib.h
    static int read(GifFileType* gif, GifByteType* dst, int sz)
    {
        BufferReader* reader = reinterpret_cast<BufferReader*>(gif->UserData);
        if (reader && sz >= 0 && reader->position + sz <= reader->data.size()) {
            std::memcpy(dst, reader->data.data() + reader->position, sz);
            reader->position += sz;
            return sz;
        }
        return -1;
    }

    const std::vector<uint8_t>& data;
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

    operator GifFileType*() { return gif; }
    GifFileType* operator->() { return gif; }

    GifFileType* gif;
};

/* GIF image. */
class ImageGif : public Image {
private:
    // GIF signatures
    static constexpr const uint8_t signature[] = { 'G', 'I', 'F' };

    /**
     * Decode single GIF frame.
     * @param gif GIF decoder
     * @param index number of the frame to load
     */
    void decode_frame(Gif& gif, const size_t index)
    {
        Frame& frame = frames[index];

        GraphicsControlBlock ctl {};
        ctl.TransparentColor = NO_TRANSPARENT_COLOR;
        DGifSavedExtensionToGCB(gif, index, &ctl);

        // handle disposition
        if (ctl.DisposalMode == DISPOSE_PREVIOUS && index + 1 < frames.size()) {
            Pixmap& curr = frame.pm;
            Pixmap& next = frames[index + 1].pm;
            next.copy(curr, 0, 0);
        }

        const SavedImage* gif_img = &gif->SavedImages[index];
        const GifImageDesc* desc = &gif_img->ImageDesc;
        const ColorMapObject* color_map =
            desc->ColorMap ? desc->ColorMap : gif->SColorMap;

        const size_t width = std::min(static_cast<size_t>(desc->Width),
                                      frame.pm.width() - desc->Left);
        const size_t height = std::min(static_cast<size_t>(desc->Height),
                                       frame.pm.height() - desc->Top);
        for (size_t y = 0; y < height; ++y) {
            const uint8_t* raster = &gif_img->RasterBits[y * desc->Width];
            for (size_t x = 0; x < width; ++x) {
                argb_t& pixel = frame.pm.at(x + desc->Left, y + desc->Top);
                const uint8_t color = raster[x];
                if (color != ctl.TransparentColor &&
                    color < color_map->ColorCount) {
                    const GifColorType* rgb = &color_map->Colors[color];
                    pixel.a = argb_t::max;
                    pixel.r = rgb->Red;
                    pixel.g = rgb->Green;
                    pixel.b = rgb->Blue;
                }
            }
        }

        if (ctl.DisposalMode == DISPOSE_DO_NOT && index + 1 < frames.size()) {
            Pixmap& curr = frame.pm;
            Pixmap& next = frames[index + 1].pm;
            next.copy(curr, 0, 0);
        }

        if (ctl.DelayTime != 0) {
            frame.duration = ctl.DelayTime * 10; // hundreds of second to ms
        } else {
            frame.duration = 100;
        }
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(signature) ||
            std::memcmp(data.data(), signature, sizeof(signature))) {
            return false;
        }

        // open decoder
        int err;
        BufferReader buf_reader(data);
        Gif gif(DGifOpen(&buf_reader, &BufferReader::read, &err));
        if (!gif) {
            return false;
        }
        if (DGifSlurp(gif) != GIF_OK) {
            return false;
        }

        // allocate and decode frames
        frames.resize(gif->ImageCount);
        for (auto& it : frames) {
            it.pm.create(Pixmap::ARGB, gif->SWidth, gif->SHeight);
        }
        for (size_t i = 0; i < frames.size(); ++i) {
            decode_frame(gif, i);
        }

        format = "GIF";
        if (gif->ImageCount > 1) {
            format += " animation";
        }

        return true;
    }
};

// SPDX-License-Identifier: MIT
// Portable Network Graphics (PNG) format support.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "png.hpp"

#include "../imageloader.hpp"

#include <png.h>
#include <setjmp.h>

#include <cstring>
#include <format>

// support for legacy libpng
#ifdef PNG_APNG_SUPPORTED
#ifndef PNG_fcTL_DISPOSE_OP_PREVIOUS
#define PNG_fcTL_DISPOSE_OP_PREVIOUS PNG_DISPOSE_OP_PREVIOUS
#endif
#ifndef PNG_fcTL_DISPOSE_OP_BACKGROUND
#define PNG_fcTL_DISPOSE_OP_BACKGROUND PNG_DISPOSE_OP_BACKGROUND
#endif
#ifndef PNG_fcTL_BLEND_OP_OVER
#define PNG_fcTL_BLEND_OP_OVER PNG_BLEND_OP_OVER
#endif
#ifndef PNG_fcTL_DISPOSE_OP_NONE
#define PNG_fcTL_DISPOSE_OP_NONE PNG_DISPOSE_OP_NONE
#endif
#endif // PNG_APNG_SUPPORTED

// register format in factory
class ImagePng;
static const ImageLoader::Registrator<ImagePng>
    image_format_registartion("PNG", ImageLoader::Priority::Highest);

/** Memory buffer reader. */
struct BufferReader {
    BufferReader(const std::vector<uint8_t>& raw_data)
        : data(raw_data)
    {
    }

    // PNG reader callback, see `png_rw_ptr` in png.h
    static void read(png_structp png, png_bytep buffer, size_t size)
    {
        BufferReader* reader =
            reinterpret_cast<BufferReader*>(png_get_io_ptr(png));
        if (reader && reader->position + size <= reader->data.size()) {
            std::memcpy(buffer, reader->data.data() + reader->position, size);
            reader->position += size;
        } else {
            png_error(png, "No data in PNG reader");
        }
    }

    const std::vector<uint8_t>& data;
    size_t position = 0;
};

/** PNG decoder/encoder wrapper. */
class PngObject {
public:
    PngObject(const bool rm)
        : read_mode(rm)
        , info(nullptr)
    {
        if (read_mode) {
            png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                         nullptr, nullptr);
        } else {
            png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                          nullptr, nullptr);
        }
        if (png) {
            info = png_create_info_struct(png);
        }
    }

    ~PngObject()
    {
        if (png) {
            if (read_mode) {
                png_destroy_read_struct(&png, info ? &info : nullptr, nullptr);
            } else {
                png_destroy_write_struct(&png, info ? &info : nullptr);
            }
        }
    }

    operator png_struct*() { return png; }
    operator png_info*() { return info; }

    bool read_mode;
    png_struct* png;
    png_info* info;
};

/* Portable Network Graphics (PNG) image. */
class ImagePng : public Image {
private:
    /**
     * Bind pixmap with PNG line-reading decoder.
     * @param pm pixmap to bind
     * @return array of pointers to pixmap data
     */
    static std::vector<png_bytep> bind_pixmap(Pixmap& pm)
    {
        std::vector<png_bytep> pbind(pm.height(), 0);
        for (size_t i = 0; i < pm.height(); ++i) {
            pbind[i] = reinterpret_cast<png_bytep>(&pm.at(0, i));
        }
        return pbind;
    }

#ifdef PNG_APNG_SUPPORTED
    /**
     * Decode single PNG frame.
     * @param png PNG decoder
     * @param index number of the frame to load
     */
    void decode_frame(PngObject& png, const size_t index)
    {
        // get frame params
        png_uint_32 width = 0;
        png_uint_32 height = 0;
        png_uint_32 offset_x = 0;
        png_uint_32 offset_y = 0;
        png_uint_16 delay_num = 0;
        png_uint_16 delay_den = 0;
        png_byte dispose = 0;
        png_byte blend = 0;
        if (png_get_valid(png, png, PNG_INFO_acTL)) {
            png_read_frame_head(png, png);
        }

        if (png_get_valid(png, png, PNG_INFO_fcTL)) {
            png_get_next_frame_fcTL(png, png, &width, &height, &offset_x,
                                    &offset_y, &delay_num, &delay_den, &dispose,
                                    &blend);
        }

        // fixup frame params
        if (width == 0) {
            width = png_get_image_width(png, png);
        }
        if (height == 0) {
            height = png_get_image_height(png, png);
        }
        // calculate frame duration in milliseconds
        if (delay_den == 0) {
            delay_den = 100;
        }
        if (delay_num == 0) {
            delay_num = 100;
        }
        frames[index].duration = delay_num * 1000 / delay_den;

        // read frame to temporary pixmap
        Pixmap pm;
        pm.create(Pixmap::ARGB, width, height);
        std::vector<png_bytep> bind = bind_pixmap(pm);
        png_read_image(png, bind.data());

        // handle disposition
        if (dispose == PNG_fcTL_DISPOSE_OP_PREVIOUS) {
            if (index == 0) {
                dispose = PNG_fcTL_DISPOSE_OP_BACKGROUND;
            } else if (index + 1 < frames.size()) {
                Pixmap& curr = frames[index].pm;
                Pixmap& next = frames[index + 1].pm;
                next.copy(curr, 0, 0);
            }
        }

        // put frame on final pixmap
        if (blend == PNG_fcTL_BLEND_OP_OVER) {
            frames[index].pm.blend(pm, offset_x, offset_y);
        } else {
            frames[index].pm.copy(pm, offset_x, offset_y);
        }

        // handle dispose
        if (dispose == PNG_fcTL_DISPOSE_OP_NONE && index + 1 < frames.size()) {
            Pixmap& curr = frames[index].pm;
            Pixmap& next = frames[index + 1].pm;
            next.copy(curr, 0, 0);
        }
    }

    /**
     * Decode multi framed image.
     * @param png PNG decoder
     */
    void decode_multiple(PngObject& png)
    {
        // allocate frames
        const png_uint_32 width = png_get_image_width(png, png);
        const png_uint_32 height = png_get_image_height(png, png);
        const png_uint_32 nframes = png_get_num_frames(png, png);
        frames.resize(nframes);
        for (auto& it : frames) {
            it.pm.create(Pixmap::ARGB, width, height);
        }

        // decode frames
        for (png_uint_32 i = 0; i < nframes; ++i) {
            decode_frame(png, i);
        }

        if (png_get_first_frame_is_hidden(png, png) && nframes > 1) {
            frames.erase(frames.begin());
        }
    }
#endif // PNG_APNG_SUPPORTED

    /**
     * Decode single framed image.
     * @param png PNG decoder
     */
    void decode_single(PngObject& png)
    {
        const png_uint_32 width = png_get_image_width(png, png);
        const png_uint_32 height = png_get_image_height(png, png);

        frames.resize(1);
        frames[0].pm.create(Pixmap::ARGB, width, height);

        std::vector<png_bytep> bind = bind_pixmap(frames[0].pm);
        png_read_image(png, bind.data());
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (png_sig_cmp(data.data(), 0, data.size()) != 0) {
            return false;
        }

        // create decoder
        PngObject png(true);
        if (!png.png || !png.info) {
            return false;
        }

        // setup error handling
        if (setjmp(png_jmpbuf(png))) {
            return false;
        }

        // register reader and get general image info
        BufferReader buf_reader(data);
        png_set_read_fn(png, &buf_reader, &BufferReader::read);
        png_read_info(png, png);

        // setup decoder
        const png_byte color_type = png_get_color_type(png, png);
        const png_byte bit_depth = png_get_bit_depth(png, png);
        if (png_get_interlace_type(png, png) != PNG_INTERLACE_NONE) {
            png_set_interlace_handling(png);
        }
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png);
        }
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            png_set_gray_to_rgb(png);
            if (bit_depth < 8) {
                png_set_expand_gray_1_2_4_to_8(png);
            }
        }
        if (png_get_valid(png, png, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png);
        }
        if (bit_depth == 16) {
            png_set_strip_16(png);
        }
        png_set_filler(png, 0xff, PNG_FILLER_AFTER);
        png_set_packing(png);
        png_set_packswap(png);
        png_set_bgr(png);
        png_set_expand(png);

        png_read_update_info(png, png);

        // decode image
#ifdef PNG_APNG_SUPPORTED
        if (png_get_valid(png, png, PNG_INFO_acTL) &&
            png_get_num_frames(png, png) > 1) {
            decode_multiple(png);
        } else {
            decode_single(png);
        }
#else
        decode_single(png);
#endif // PNG_APNG_SUPPORTED

        // read text info
        png_text* txt;
        int total;
        if (png_get_text(png, png, &txt, &total)) {
            for (int i = 0; i < total; ++i) {
                meta.insert(std::make_pair(txt[i].key, txt[i].text));
            }
        }

        format = std::format("PNG {}bit", bit_depth * 4);

        return true;
    }
};

namespace Png {

// PNG writer callback, see `png_rw_ptr` in png.h
static void png_encoder_write(png_structp png, png_bytep buffer, size_t size)
{
    std::vector<uint8_t>* out =
        reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png));
    out->reserve(out->size() + size);
    out->insert(out->end(), buffer, buffer + size);
}

// // PNG flusher callback, see `png_flush_ptr` in png.h
static void png_encoder_flush(png_structp) { }

std::vector<uint8_t> encode(const Pixmap& pm)
{
    // create encoder
    PngObject png(false);
    if (!png.png || !png.info) {
        return {};
    }

    // setup error handling
    if (setjmp(png_jmpbuf(png))) {
        return {};
    }

    // bind pixmap
    std::vector<png_bytep> bind(pm.height(), 0);
    for (size_t i = 0; i < pm.height(); ++i) {
        bind[i] =
            reinterpret_cast<png_bytep>(const_cast<argb_t*>(&pm.at(0, i)));
    }

    // setup output: 8bit RGBA
    png_set_IHDR(png, png, pm.width(), pm.height(), 8, PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_bgr(png);

    // encode png
    std::vector<uint8_t> data;
    png_set_write_fn(png, &data, &png_encoder_write, &png_encoder_flush);
    png_write_info(png, png);
    png_write_image(png, bind.data());
    png_write_end(png, nullptr);

    return data;
}

}; // namespace Png

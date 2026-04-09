// SPDX-License-Identifier: MIT
// Raw camera image format.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <libraw.h>
#pragma GCC diagnostic pop

#include <cstring>
#include <memory>

class ImageFormatRaw : public ImageFormat {
public:
    ImageFormatRaw()
        : ImageFormat(Priority::Normal, "raw")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        // open decoder
        LibRaw decoder(0);

        if (!FormatFactory::self().fix_orientation) {
            decoder.imgdata.params.user_flip = 0;
        }

        if (decoder.open_buffer(data.data, data.size) != LIBRAW_SUCCESS ||
            decoder.unpack() != LIBRAW_SUCCESS) {
            return nullptr;
        }
        decoder.output_params_ptr()->output_bps = 8; // 8-bit color
        if (decoder.dcraw_process() != LIBRAW_SUCCESS) {
            return nullptr;
        }

        // decode image
        RawImage img(decoder.dcraw_make_mem_image(), &libraw_dcraw_clear_mem);
        if (img->type != LIBRAW_IMAGE_BITMAP || img->colors != 3 ||
            img->bits != 8) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;
        pm.create(Pixmap::RGB, img->width, img->height);

        // copy data to image frame
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                const uint8_t* src = img->data + (y * img->width + x) * 3;
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                dst.r = src[0];
                dst.g = src[1];
                dst.b = src[2];
            }
        }

        image->format = "RAW";

        return image;
    }

    Pixmap preview(const Data& data, const size_t sz,
                   const bool max_sz) override
    {
        LibRaw decoder(0);

        if (!FormatFactory::self().fix_orientation) {
            decoder.imgdata.params.user_flip = 0;
        }

        // read thumbnail
        if (decoder.open_buffer(data.data, data.size) != LIBRAW_SUCCESS ||
            decoder.unpack_thumb() != LIBRAW_SUCCESS) {
            return ImageFormat::preview(data, sz, max_sz);
        }

        RawImage raw_thumb(decoder.dcraw_make_mem_thumb(),
                           &libraw_dcraw_clear_mem);
        if (!raw_thumb) {
            return ImageFormat::preview(data, sz, max_sz);
        }

        const Data thumb_data {
            .data = raw_thumb->data,
            .size = raw_thumb->data_size,
        };

        const ImagePtr image = FormatFactory::self().decode(thumb_data);
        if (image) {
            return make_thumb(image->frames[0].pm, sz, max_sz);
        }

        Pixmap pm;
        pm.create(Pixmap::RGB, raw_thumb->width, raw_thumb->height);
        const uint8_t* tdata = raw_thumb->data;
        for (size_t y = 0; y < pm.height(); ++y) {
            for (size_t x = 0; x < pm.width(); ++x) {
                pm.at(x, y) = { argb_t::max, tdata[0], tdata[1], tdata[2] };
                tdata += 3;
            }
        }
        Pixmap thumb = make_thumb(pm, sz, max_sz);
        if (FormatFactory::self().fix_orientation) {
            ImageFormat::fix_orientation(
                thumb, decoder.imgdata.thumbs_list.thumblist[0].tflip);
        }

        return thumb;
    }

    // ignore, done by decoder
    void fix_orientation(ImagePtr&, const int) const override {}

private:
    using RawImage = std::unique_ptr<libraw_processed_image_t,
                                     decltype(&libraw_dcraw_clear_mem)>;
};

// register format in factory
static ImageFormatRaw format_raw;

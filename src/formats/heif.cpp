// SPDX-License-Identifier: MIT
// HEIF image format.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <libheif/heif.h>

#include <cstring>
#include <memory>
#include <utility>

class ImageFormatHeif : public ImageFormat {
public:
    ImageFormatHeif()
        : ImageFormat(Priority::Normal, "heif")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        if (heif_check_filetype(data.data, data.size) !=
            heif_filetype_yes_supported) {
            return nullptr;
        }

        // open decoder
        const HeifContext hctx(heif_context_alloc(), &heif_context_free);
        if (!hctx) {
            return nullptr;
        }

        // decode image
        heif_error err;
        err = heif_context_read_from_memory(hctx.get(), data.data, data.size,
                                            nullptr);
        if (err.code != heif_error_Ok) {
            return nullptr;
        }

        heif_image_handle* pih = nullptr;
        err = heif_context_get_primary_image_handle(hctx.get(), &pih);
        if (err.code != heif_error_Ok) {
            return nullptr;
        }
        const HeifImageHandle himh(pih, &heif_image_handle_release);

        HeifDecOpts hopt(heif_decoding_options_alloc(),
                         &heif_decoding_options_free);
        if (hopt && !FormatFactory::self().fix_orientation) {
            hopt->ignore_transformations = 1;
        }

        heif_image* him = nullptr;
        err = heif_decode_image(himh.get(), &him, heif_colorspace_RGB,
                                heif_chroma_interleaved_RGBA, hopt.get());
        if (err.code != heif_error_Ok) {
            return nullptr;
        }
        const HeifImage himg(him, &heif_image_release);

        int stride;
        const uint8_t* decoded = heif_image_get_plane_readonly(
            himg.get(), heif_channel_interleaved, &stride);
        if (!decoded) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;

        // put decoded data into pixmap
        pm.create(heif_image_handle_has_alpha_channel(himh.get()) ? Pixmap::ARGB
                                                                  : Pixmap::RGB,
                  heif_image_get_primary_width(himg.get()),
                  heif_image_get_primary_height(himg.get()));
        if (std::cmp_equal(stride, pm.stride())) {
            std::memcpy(pm.ptr(0, 0), decoded, pm.stride() * pm.height());
        } else {
            for (size_t y = 0; y < pm.height(); ++y) {
                const uint8_t* src = decoded + y * stride;
                std::memcpy(pm.ptr(0, y), src, pm.stride());
            }
        }
        pm.abgr_to_argb();

        image->format = "HEIF";
        return image;
    }

    // ignore, done by decoder
    void fix_orientation(ImagePtr&, const int) const override {}

private:
    // HEIF decoder wrappers
    using HeifContext =
        std::unique_ptr<heif_context, decltype(&heif_context_free)>;
    using HeifDecOpts = std::unique_ptr<heif_decoding_options,
                                        decltype(&heif_decoding_options_free)>;
    using HeifImageHandle =
        std::unique_ptr<heif_image_handle,
                        decltype(&heif_image_handle_release)>;
    using HeifImage =
        std::unique_ptr<heif_image, decltype(&heif_image_release)>;
};

// register format in factory
static ImageFormatHeif format_heif;

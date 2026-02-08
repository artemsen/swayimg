// SPDX-License-Identifier: MIT
// HEIF format decoder.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <libheif/heif.h>

#include <cstring>
#include <memory>

// register format in factory
class ImageHeif;
static const ImageLoader::Registrator<ImageHeif>
    image_format_registartion("HEIF", ImageLoader::Priority::Normal);

/* HEIF image. */
class ImageHeif : public Image {
private:
    // HEIF decoder wrappers
    using HeifContext =
        std::unique_ptr<heif_context, decltype(&heif_context_free)>;
    using HeifImageHandle =
        std::unique_ptr<heif_image_handle,
                        decltype(&heif_image_handle_release)>;
    using HeifImage =
        std::unique_ptr<heif_image, decltype(&heif_image_release)>;

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        if (heif_check_filetype(data.data(), data.size()) !=
            heif_filetype_yes_supported) {
            return false;
        }

        // open decoder
        HeifContext hctx(heif_context_alloc(), &heif_context_free);
        if (!hctx) {
            return false;
        }

        // decode image
        heif_error err;
        err = heif_context_read_from_memory(hctx.get(), data.data(),
                                            data.size(), nullptr);
        if (err.code != heif_error_Ok) {
            return false;
        }

        heif_image_handle* pih = nullptr;
        err = heif_context_get_primary_image_handle(hctx.get(), &pih);
        if (err.code != heif_error_Ok) {
            return false;
        }
        HeifImageHandle himh(pih, &heif_image_handle_release);

        heif_image* him = nullptr;
        err = heif_decode_image(himh.get(), &him, heif_colorspace_RGB,
                                heif_chroma_interleaved_RGBA, nullptr);
        if (err.code != heif_error_Ok) {
            return false;
        }
        HeifImage himg(him, &heif_image_release);

        int stride;
        const uint8_t* decoded = heif_image_get_plane_readonly(
            himg.get(), heif_channel_interleaved, &stride);
        if (!decoded) {
            return false;
        }

        // put decoded data into pixmap
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(heif_image_handle_has_alpha_channel(himh.get()) ? Pixmap::ARGB
                                                                  : Pixmap::RGB,
                  heif_image_get_primary_width(himg.get()),
                  heif_image_get_primary_height(himg.get()));
        if (stride == static_cast<int>(pm.stride())) {
            std::memcpy(pm.ptr(0, 0), decoded, pm.stride() * pm.height());
        } else {
            for (size_t y = 0; y < pm.height(); ++y) {
                const uint8_t* src = decoded + y * stride;
                std::memcpy(pm.ptr(0, y), src, pm.stride());
            }
        }
        pm.abgr_to_argb();

        format = "HEIF";
        return true;
    }
};

// SPDX-License-Identifier: MIT
// AV1 (AVIF/AVIFS) format decoder.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <avif/avif.h>

#include <cstring>
#include <format>
#include <memory>

// register format in factory
class ImageAvif;
static const ImageLoader::Registrator<ImageAvif>
    image_format_registartion("AVIF", ImageLoader::Priority::High);

/* AVIF image. */
class ImageAvif : public Image {
private:
    // AVIF signature
    static constexpr const uint8_t signature[] = { 'f', 't', 'y', 'p' };
    static constexpr const size_t signature_offset = 4;

    // AVIF decoder wrapper
    using AvifDecoder =
        std::unique_ptr<avifDecoder, decltype(&avifDecoderDestroy)>;

    /**
     * Decode single frame.
     * @param avif decoder instance
     * @param pm destination pixmap
     * @return decoding status
     */
    avifResult decode_frame(AvifDecoder& avif, Pixmap& pm) const
    {
        avifResult rc;
        avifRGBImage rgb;

        avifRGBImageSetDefaults(&rgb, avif->image);

        rgb.depth = 8;
        rgb.format = AVIF_RGB_FORMAT_BGRA;

        rc = avifRGBImageAllocatePixels(&rgb);
        if (rc != AVIF_RESULT_OK) {
            return rc;
        }

        rc = avifImageYUVToRGB(avif->image, &rgb);
        if (rc == AVIF_RESULT_OK) {
            pm.create(avif->alphaPresent ? Pixmap::ARGB : Pixmap::RGB,
                      rgb.width, rgb.height);
            std::memcpy(&pm.at(0, 0), rgb.pixels,
                        rgb.width * rgb.height * sizeof(argb_t));
        }

        avifRGBImageFreePixels(&rgb);

        return rc;
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < signature_offset + sizeof(signature) ||
            std::memcmp(data.data() + signature_offset, signature,
                        sizeof(signature))) {
            return false;
        }

        // open decoder
        AvifDecoder avif(avifDecoderCreate(), &avifDecoderDestroy);
        if (!avif) {
            return false;
        }
        avifResult rc;
        rc = avifDecoderSetIOMemory(avif.get(), data.data(), data.size());
        if (rc != AVIF_RESULT_OK) {
            return false;
        }
        rc = avifDecoderParse(avif.get());
        if (rc != AVIF_RESULT_OK) {
            return false;
        }

        // allocate frames
        frames.resize(avif->imageCount);

        // decode images
        if (avif->imageCount == 1) {
            // single image
            rc = avifDecoderNextImage(avif.get());
            if (rc != AVIF_RESULT_OK) {
                return false;
            }
            rc = decode_frame(avif, frames[0].pm);
            if (rc != AVIF_RESULT_OK) {
                return false;
            }
        } else {
            // multiple images
            for (int i = 0; i < avif->imageCount; ++i) {
                Frame& frame = frames[i];

                rc = avifDecoderNthImage(avif.get(), i);
                if (rc != AVIF_RESULT_OK) {
                    return false;
                }
                rc = decode_frame(avif, frame.pm);
                if (rc != AVIF_RESULT_OK) {
                    return false;
                }

                avifImageTiming timing;
                rc = avifDecoderNthImageTiming(avif.get(), i, &timing);
                if (rc != AVIF_RESULT_OK) {
                    return false;
                }
                frame.duration =
                    1000.0 / timing.timescale * timing.durationInTimescales;
            }
        }

        format = std::format("AV1 {}bpc {}", avif->image->depth,
                             avifPixelFormatToString(avif->image->yuvFormat));

        return true;
    }
};

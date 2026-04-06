// SPDX-License-Identifier: MIT
// AV1 (AVIF/AVIFS) image format.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <avif/avif.h>

#include <format>
#include <memory>

class ImageFormatAvif : public ImageFormat {
public:
    ImageFormatAvif()
        : ImageFormat(Priority::High, "avif")
    {
    }

    using AvifDecoder =
        std::unique_ptr<avifDecoder, decltype(&avifDecoderDestroy)>;

    ImagePtr decode(const Data& data) override
    {
        if (!check_signature(data, { 'f', 't', 'y', 'p' }, 4)) {
            return nullptr;
        }

        avifResult rc;

        // open decoder
        AvifDecoder avif(avifDecoderCreate(), &avifDecoderDestroy);
        if (!avif) {
            return nullptr;
        }
        rc = avifDecoderSetIOMemory(avif.get(), data.data, data.size);
        if (rc != AVIF_RESULT_OK) {
            return nullptr;
        }
        rc = avifDecoderParse(avif.get());
        if (rc != AVIF_RESULT_OK) {
            return nullptr;
        }

        // allocate image and frames
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(avif->imageCount);

        // decode images
        if (avif->imageCount == 1) {
            // single image
            rc = avifDecoderNextImage(avif.get());
            if (rc != AVIF_RESULT_OK) {
                return nullptr;
            }
            rc = decode_frame(avif, image->frames[0].pm);
            if (rc != AVIF_RESULT_OK) {
                return nullptr;
            }
        } else {
            // multiple images
            for (int i = 0; i < avif->imageCount; ++i) {
                Image::Frame& frame = image->frames[i];

                rc = avifDecoderNthImage(avif.get(), i);
                if (rc != AVIF_RESULT_OK) {
                    return nullptr;
                }
                rc = decode_frame(avif, frame.pm);
                if (rc != AVIF_RESULT_OK) {
                    return nullptr;
                }

                avifImageTiming timing;
                rc = avifDecoderNthImageTiming(avif.get(), i, &timing);
                if (rc != AVIF_RESULT_OK) {
                    return nullptr;
                }
                frame.duration =
                    1000.0 / timing.timescale * timing.durationInTimescales;
            }
        }

        image->format =
            std::format("AV1 {}bpc {}", avif->image->depth,
                        avifPixelFormatToString(avif->image->yuvFormat));

        return image;
    }

private:
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
};

// register format in factory
static ImageFormatAvif format_avif;

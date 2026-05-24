// SPDX-License-Identifier: MIT
// JPEG 2000 image format.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"
#include "../log.hpp"

#include <openjpeg.h>

#include <algorithm>
#include <format>
#include <utility>

class ImageFormatJpeg2000 : public ImageFormat {
public:
    ImageFormatJpeg2000()
        : ImageFormat(Priority::Low, "jpeg2000")
    {
    }

    ImagePtr decode(const Data& data) override
    {
        // check signature and get codec type
        OPJ_CODEC_FORMAT codec_fmt;
        if (check_signature(data, JP2_RFC3745) ||
            check_signature(data, JP2_MAGIC)) {
            codec_fmt = OPJ_CODEC_JP2;
        } else if (check_signature(data, J2K_STREAM)) {
            codec_fmt = OPJ_CODEC_J2K;
        } else {
            return nullptr;
        }

        // setup decoder parameters
        opj_dparameters_t opj_params;
        const OpjCodec opj_codec(opj_create_decompress(codec_fmt),
                                 &opj_destroy_codec);
        if (!opj_codec) {
            return nullptr;
        }
        opj_set_default_decoder_parameters(&opj_params);
        if (!opj_setup_decoder(opj_codec.get(), &opj_params)) {
            return nullptr;
        }

        // setup massage handler
        opj_set_info_handler(opj_codec.get(), nullptr, nullptr);
        opj_set_warning_handler(opj_codec.get(), nullptr, nullptr);
        opj_set_error_handler(opj_codec.get(), &error_callback, nullptr);

        // enable multithreading support
        if (opj_has_thread_support()) {
            opj_codec_set_threads(opj_codec.get(),
                                  std::min(opj_get_num_cpus(), 4));
        }

        // setup custom memory stream
        BufferIO bio(data);
        const OpjStream opj_stream(opj_stream_create(data.size, OPJ_TRUE),
                                   &opj_stream_destroy);
        if (!opj_stream) {
            return nullptr;
        }
        opj_stream_set_read_function(opj_stream.get(), &BufferIO::read);
        opj_stream_set_skip_function(opj_stream.get(), &BufferIO::skip);
        opj_stream_set_seek_function(opj_stream.get(), &BufferIO::seek);
        opj_stream_set_user_data(opj_stream.get(), &bio, nullptr);
        opj_stream_set_user_data_length(opj_stream.get(), data.size);

        // decode image
        opj_image_t* opj_imgptr = nullptr;
        if (!opj_read_header(opj_stream.get(), opj_codec.get(), &opj_imgptr)) {
            return nullptr;
        }
        OpjImage opj_image(opj_imgptr, &opj_image_destroy);
        if (!opj_set_decode_area(opj_codec.get(), opj_image.get(), 0, 0,
                                 opj_image->x1, opj_image->y1) ||
            !opj_decode(opj_codec.get(), opj_stream.get(), opj_image.get()) ||
            !opj_end_decompress(opj_codec.get(), opj_stream.get())) {
            return nullptr;
        }

        // get color space
        Jp2ColorSpace cspace;
        if (opj_image->color_space == OPJ_CLRSPC_SRGB) {
            cspace = Jp2ColorSpace::SRGB;
        } else if (opj_image->color_space == OPJ_CLRSPC_GRAY ||
                   opj_image->numcomps <= 2) {
            cspace = Jp2ColorSpace::Grayscale;
        } else if (opj_image->color_space == OPJ_CLRSPC_EYCC) {
            cspace = Jp2ColorSpace::EYCC;
        } else if (opj_image->color_space == OPJ_CLRSPC_CMYK) {
            cspace = Jp2ColorSpace::CMYK;
        } else if (opj_image->color_space == OPJ_CLRSPC_SYCC ||
                   (opj_image->numcomps == 3 &&
                    opj_image->comps[0].dx == opj_image->comps[0].dy &&
                    opj_image->comps[1].dx != 1)) {
            const opj_image_comp_t& comp0 = opj_image->comps[0];
            const opj_image_comp_t& comp1 = opj_image->comps[1];
            const opj_image_comp_t& comp2 = opj_image->comps[2];
            if (comp0.dx == 1 && comp1.dx == 2 && comp2.dx == 2 &&
                comp0.dy == 1 && comp1.dy == 2 && comp2.dy == 2) {
                cspace = Jp2ColorSpace::YUV420;
            } else if (comp0.dx == 1 && comp1.dx == 2 && comp2.dx == 2 &&
                       comp0.dy == 1 && comp1.dy == 1 && comp2.dy == 1) {
                cspace = Jp2ColorSpace::YUV422;
            } else if (comp0.dx == 1 && comp1.dx == 1 && comp2.dx == 1 &&
                       comp0.dy == 1 && comp1.dy == 1 && comp2.dy == 1) {
                cspace = Jp2ColorSpace::YUV444;
            } else {
                cspace = Jp2ColorSpace::SRGB; // use as fallback
            }
        } else if (opj_image->numcomps >= 3 &&
                   opj_image->comps[0].dx == opj_image->comps[1].dx &&
                   opj_image->comps[1].dx == opj_image->comps[2].dx &&
                   opj_image->comps[0].dy == opj_image->comps[1].dy &&
                   opj_image->comps[1].dy == opj_image->comps[2].dy &&
                   opj_image->comps[0].prec == opj_image->comps[1].prec &&
                   opj_image->comps[1].prec == opj_image->comps[2].prec &&
                   opj_image->comps[0].sgnd == opj_image->comps[1].sgnd &&
                   opj_image->comps[1].sgnd == opj_image->comps[2].sgnd) {
            cspace = Jp2ColorSpace::SRGB;
        } else {
            cspace = Jp2ColorSpace::Grayscale;
        }

        // scale precision to 8 bit per component
        const OPJ_UINT32 origin_prec = opj_image->comps[0].prec;
        for (size_t i = 0; i < opj_image->numcomps; ++i) {
            scale_component(opj_image->comps[i]);
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& pm = image->frames[0].pm;

        // load image to RGB pixmap
        const char* cs_name = nullptr;
        switch (cspace) {
            case Jp2ColorSpace::Grayscale:
                load_grayscale(*opj_image.get(), pm);
                cs_name = "Grayscale";
                break;
            case Jp2ColorSpace::SRGB:
                load_srgb(*opj_image.get(), pm);
                cs_name = "SRGB";
                break;
            case Jp2ColorSpace::YUV420:
                load_yuv420(*opj_image.get(), pm);
                cs_name = "YUV 4:2:0";
                break;
            case Jp2ColorSpace::YUV422:
                load_yuv422(*opj_image.get(), pm);
                cs_name = "YUV 4:2:2";
                break;
            case Jp2ColorSpace::YUV444:
                load_yuv444(*opj_image.get(), pm);
                cs_name = "YUV 4:4:4";
                break;
            case Jp2ColorSpace::EYCC:
                // don't have any sample for debug
                cs_name = "e-YCC";
                Log::error("{}: Unsupported color space {}", LOG_PREFIX,
                           cs_name);
                return nullptr;
            case Jp2ColorSpace::CMYK:
                // don't have any sample for debug
                cs_name = "CMYK";
                Log::error("{}: Unsupported color space {}", LOG_PREFIX,
                           cs_name);
                return nullptr;
        }

        image->format = std::format("JPEG2000 {}bit {}",
                                    opj_image->numcomps * origin_prec, cs_name);

        return image;
    }

private:
    // JP2 signatures
    static constexpr const uint8_t JP2_RFC3745[] = { 0x00, 0x00, 0x00, 0x0c,
                                                     0x6a, 0x50, 0x20, 0x20,
                                                     0x0d, 0x0a, 0x87, 0x0a };
    static constexpr const uint8_t JP2_MAGIC[] = { 0x0d, 0x0a, 0x87, 0x0a };
    static constexpr const uint8_t J2K_STREAM[] = { 0xff, 0x4f, 0xff, 0x51 };

    // Prefix used in log output
    static constexpr const char* LOG_PREFIX = "JPEG2000";

    // Wrappers around libopenjp2 objects
    using OpjCodec = std::unique_ptr<opj_codec_t, decltype(&opj_destroy_codec)>;
    using OpjStream =
        std::unique_ptr<opj_stream_t, decltype(&opj_stream_destroy)>;
    using OpjImage = std::unique_ptr<opj_image_t, decltype(&opj_image_destroy)>;

    // Color spaces
    enum class Jp2ColorSpace : uint8_t {
        Grayscale,
        SRGB,
        YUV420,
        YUV422,
        YUV444,
        EYCC,
        CMYK
    };

private:
    // error callback, see opj_msg_callback for details
    static void error_callback(const char* msg, void*)
    {
        Log::error("{}: {}", LOG_PREFIX, msg);
    }

    /**
     * Scale precision (bits per component) to 8 bits.
     * @param component JP2 component
     */
    static void scale_component(opj_image_comp_t& component)
    {
        constexpr const OPJ_UINT32 precision = 8;
        if (component.prec == precision) {
            return;
        }

        const size_t len = static_cast<size_t>(component.w) * component.h;

        if (component.prec > precision) {
            // downscale
            const OPJ_UINT32 shift = component.prec - precision;
            if (component.sgnd) {
                for (size_t i = 0; i < len; ++i) {
                    component.data[i] >>= shift;
                }
            } else {
                OPJ_UINT32* data =
                    reinterpret_cast<OPJ_UINT32*>(component.data);
                for (size_t i = 0; i < len; ++i) {
                    data[i] >>= shift;
                }
            }
        } else {
            // upscale
            if (component.sgnd) {
                const OPJ_INT32 new_max = 1 << (precision - 1);
                const OPJ_INT32 old_max = 1 << (component.prec - 1);
                OPJ_INT32* data = component.data;
                for (size_t i = 0; i < len; ++i) {
                    data[i] = (data[i] * new_max) / old_max;
                }
            } else {
                const OPJ_INT32 new_max = (1 << precision) - 1;
                const OPJ_INT32 old_max = (1 << component.prec) - 1;
                OPJ_UINT32* data =
                    reinterpret_cast<OPJ_UINT32*>(component.data);
                for (size_t i = 0; i < len; ++i) {
                    data[i] = (data[i] * new_max) / old_max;
                }
            }
        }
        component.prec = precision;
    }

    /**
     * Convert YUV color to RGB.
     * @param y,cb,cr YUV components
     * @return ARGB color
     */
    static inline argb_t yuv_to_rgb(const OPJ_INT32 y, const OPJ_INT32 cb,
                                    const OPJ_INT32 cr)
    {
        constexpr const OPJ_INT32 offset = 128; // 8-bit components
        const OPJ_INT32 cb_off = cb - offset;
        const OPJ_INT32 cr_off = cr - offset;
        const OPJ_INT32 r = y + 1.402 * cr_off;
        const OPJ_INT32 g = y - 0.344 * cb_off - 0.714 * cr_off;
        const OPJ_INT32 b = y + 1.772 * cb_off;
        return { argb_t::max,
                 static_cast<argb_t::channel>(std::clamp(r, 0, 255)),
                 static_cast<argb_t::channel>(std::clamp(g, 0, 255)),
                 static_cast<argb_t::channel>(std::clamp(b, 0, 255)) };
    }

    /**
     * Load pixmap from JP2 grayscale image.
     * @param img JP2 image
     * @param pm destination pixmap
     */
    static void load_grayscale(const opj_image_t& img, Pixmap& pm)
    {
        pm.create(Pixmap::RGB, img.comps[0].w, img.comps[0].h);
        for (size_t y = 0; y < img.comps[0].h; ++y) {
            const size_t offset_y = y * img.comps[0].w;
            for (size_t x = 0; x < img.comps[0].w; ++x) {
                const size_t offset = offset_y + x;
                argb_t& dst = pm.at(x, y);
                dst.a = argb_t::max;
                dst.r = dst.g = dst.b = img.comps[0].data[offset];
            }
        }
    }

    /**
     * Load pixmap from JP2 SRGB image.
     * @param img JP2 image
     * @param pm destination pixmap
     */
    static void load_srgb(const opj_image_t& img, Pixmap& pm)
    {
        const bool use_alpha = img.numcomps > 3;
        pm.create(use_alpha ? Pixmap::ARGB : Pixmap::RGB, img.comps[0].w,
                  img.comps[0].h);
        for (size_t y = 0; y < img.comps[0].h; ++y) {
            const size_t offset_y = y * img.comps[0].w;
            for (size_t x = 0; x < img.comps[0].w; ++x) {
                const size_t offset = offset_y + x;
                argb_t& dst = pm.at(x, y);
                dst.r = img.comps[0].data[offset];
                dst.g = img.comps[1].data[offset];
                dst.b = img.comps[2].data[offset];
                dst.a = use_alpha ? img.comps[3].data[offset] : argb_t::max;
            }
        }
    }

    /**
     * Load pixmap from JP2 YUV 4:2:0 image.
     * @param img JP2 image
     * @param pm destination pixmap
     */
    static void load_yuv420(const opj_image_t& img, Pixmap& pm)
    {
        const OPJ_UINT32 width = img.comps[0].w;
        const OPJ_UINT32 height = img.comps[0].h;
        const OPJ_UINT32 comp12w = img.comps[1].w;

        // if img.x0 is odd, then first column shall use Cb/Cr = 0
        const OPJ_UINT32 off_x = img.x0 & 1;
        const OPJ_UINT32 max_w = width - off_x;
        // if img.y0 is odd, then first line shall use Cb/Cr = 0
        const OPJ_UINT32 off_y = img.y0 & 1;
        const OPJ_UINT32 max_h = height - off_y;

        const OPJ_INT32* y = img.comps[0].data;
        const OPJ_INT32* cb = img.comps[1].data;
        const OPJ_INT32* cr = img.comps[2].data;

        pm.create(Pixmap::RGB, width, height);
        argb_t* px = &pm.at(0, 0);

        if (off_y > 0) {
            for (OPJ_UINT32 j = 0; j < width; ++j) {
                *px = yuv_to_rgb(*y, 0, 0);
                ++px;
                ++y;
            }
        }
        OPJ_UINT32 yy;
        for (yy = 0; yy < (max_h & ~static_cast<OPJ_UINT32>(1)); yy += 2) {
            const OPJ_INT32* next_y = y + width;
            argb_t* next_px = px + width;

            if (off_x) {
                *px = yuv_to_rgb(*y, 0, 0);
                ++px;
                ++y;
                *next_px = yuv_to_rgb(*next_y, *cb, *cr);
                ++next_px;
                ++next_y;
            }

            OPJ_UINT32 xx;
            for (xx = 0; xx < (max_w & ~static_cast<OPJ_UINT32>(1)); xx += 2) {
                *px = yuv_to_rgb(*y, *cb, *cr);
                ++px;
                ++y;
                *px = yuv_to_rgb(*y, *cb, *cr);
                ++px;
                ++y;

                *next_px = yuv_to_rgb(*next_y, *cb, *cr);
                ++next_px;
                ++next_y;
                *next_px = yuv_to_rgb(*next_y, *cb, *cr);
                ++next_px;
                ++next_y;

                ++cb;
                ++cr;
            }
            if (xx < max_w) {
                if (comp12w == xx / 2) {
                    *px = yuv_to_rgb(*y, 0, 0);
                    *next_px = yuv_to_rgb(*next_y, 0, 0);
                } else {
                    *px = yuv_to_rgb(*y, *cb, *cr);
                    *next_px = yuv_to_rgb(*next_y, *cb, *cr);
                }
                ++px;
                ++next_px;
                ++y;
                ++next_y;

                if (comp12w > xx / 2) {
                    ++cb;
                    ++cr;
                }
            }
            y += width;
            px += width;
        }
        if (yy < max_h) {
            if (off_x > 0) {
                *px = yuv_to_rgb(*y, 0, 0);
                ++px;
                ++y;
            }
            for (yy = 0; yy < (max_w & ~static_cast<OPJ_UINT32>(1)); yy += 2) {
                *px = yuv_to_rgb(*y, *cb, *cr);
                ++px;
                ++y;
                *px = yuv_to_rgb(*y, *cb, *cr);
                ++px;
                ++y;
                ++cb;
                ++cr;
            }
            if (yy < max_w) {
                if (comp12w == yy / 2) {
                    *px = yuv_to_rgb(*y, *cb, *cr);
                } else {
                    *px = yuv_to_rgb(*y, *cb, *cr);
                }
            }
        }
    }

    /**
     * Load pixmap from JP2 YUV 4:2:2 image.
     * @param img JP2 image
     * @param pm destination pixmap
     */
    static void load_yuv422(const opj_image_t& img, Pixmap& pm)
    {
        // if img.x0 is odd, then first column shall use Cb/Cr = 0
        const OPJ_UINT32 off_x = img.x0 & 1;
        const OPJ_UINT32 max_w = img.comps[0].w - off_x;

        const OPJ_INT32* y = img.comps[0].data;
        const OPJ_INT32* cb = img.comps[1].data;
        const OPJ_INT32* cr = img.comps[2].data;

        pm.create(Pixmap::RGB, img.comps[0].w, img.comps[0].h);
        argb_t* px = &pm.at(0, 0);

        for (OPJ_UINT32 yy = 0; yy < img.comps[0].h; ++yy) {
            if (off_x) {
                *px = yuv_to_rgb(*y, 0, 0);
                ++px;
                ++y;
            }
            OPJ_UINT32 xx;
            for (xx = 0; xx < (max_w & ~static_cast<OPJ_UINT32>(1)); xx += 2) {
                *px = yuv_to_rgb(*y, *cb, *cr);
                ++px;
                ++y;
                *px = yuv_to_rgb(*y, *cb, *cr);
                ++px;
                ++y;
                ++cb;
                ++cr;
            }
            if (xx < max_w) {
                if (img.comps[1].w == xx / 2) {
                    *px = yuv_to_rgb(*y, 0, 0);
                } else {
                    *px = yuv_to_rgb(*y, *cb, *cr);
                }
                ++px;
                ++y;
                if (img.comps[1].w > xx / 2) {
                    ++cb;
                    ++cr;
                }
            }
        }
    }

    /**
     * Load pixmap from JP2 YUV 4:4:4 image.
     * @param img JP2 image
     * @param pm destination pixmap
     */
    static void load_yuv444(const opj_image_t& img, Pixmap& pm)
    {
        const OPJ_INT32* y = img.comps[0].data;
        const OPJ_INT32* cb = img.comps[1].data;
        const OPJ_INT32* cr = img.comps[2].data;

        pm.create(Pixmap::RGB, img.comps[0].w, img.comps[0].h);
        pm.foreach([&](argb_t& px) {
            px = yuv_to_rgb(*y, *cb, *cr);
            ++y;
            ++cb;
            ++cr;
        });
    }

    /** Memory buffer I/O. */
    struct BufferIO {
        BufferIO(const Data& raw_data)
            : data(raw_data)
        {
        }

        // read callback, see opj_stream_read_fn for details
        static OPJ_SIZE_T read(void* buffer, OPJ_SIZE_T size, void* data)
        {
            BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            if (bufio->position >= bufio->data.size) {
                return static_cast<OPJ_SIZE_T>(-1);
            }

            const size_t rest = bufio->data.size - bufio->position;
            if (size > rest) {
                size = rest;
            }
            std::memcpy(buffer, bufio->data.data + bufio->position, size);
            bufio->position += size;
            return size;
        }

        // read callback, see opj_stream_skip_fn for details
        static OPJ_OFF_T skip(OPJ_OFF_T bytes, void* data)
        {
            BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            if (bufio->position + bytes >= bufio->data.size) {
                return static_cast<OPJ_SIZE_T>(-1);
            }
            bufio->position += bytes;
            return bufio->position;
        }

        // read callback, see opj_stream_seek_fn for details
        static OPJ_BOOL seek(OPJ_OFF_T offset, void* data)
        {
            BufferIO* bufio = reinterpret_cast<BufferIO*>(data);
            if (std::cmp_greater_equal(offset, bufio->data.size)) {
                return OPJ_FALSE;
            }
            bufio->position = offset;
            return OPJ_TRUE;
        }

        const Data& data;
        size_t position = 0;
    };
};

// register format in factory
static ImageFormatJpeg2000 format_jpeg2000;

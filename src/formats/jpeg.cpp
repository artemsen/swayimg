// SPDX-License-Identifier: MIT
// JPEG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <jpeglib.h>
#include <setjmp.h>

#include <format>

// register format in factory
class ImageJpg;
static const ImageLoader::Registrator<ImageJpg>
    image_format_registartion("JPG", ImageLoader::Priority::Highest);

/* JPEG image. */
class ImageJpg : public Image {
private:
    // JPEG signature
    static constexpr const uint8_t signature[] = { 0xff, 0xd8 };

    /** JPG error description. */
    struct Error {
        jpeg_error_mgr manager;
        jmp_buf jump;
    };

    /** JPEG error callback. */
    static void jpg_error_exit(j_common_ptr jpg)
    {
        Error* err = reinterpret_cast<Error*>(jpg->err);
        char msg[JMSG_LENGTH_MAX] = { 0 };
        (*(jpg->err->format_message))(jpg, msg);
        fprintf(stderr, "JPEG: %s\n", msg);
        longjmp(err->jump, 1);
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < sizeof(signature) || data[0] != signature[0] ||
            data[1] != signature[1]) {
            return false;
        }

        jpeg_decompress_struct jpg;

        // setup error handling
        Error err;
        jpg.err = jpeg_std_error(&err.manager);
        err.manager.error_exit = jpg_error_exit;
        if (setjmp(err.jump)) {
            jpeg_destroy_decompress(&jpg);
            return false;
        }

        jpeg_create_decompress(&jpg);
        jpeg_mem_src(&jpg, data.data(), data.size());
        jpeg_read_header(&jpg, TRUE);

#ifdef LIBJPEG_TURBO_VERSION
        switch (jpg.jpeg_color_space) {
            case JCS_CMYK:
            case JCS_YCCK:
                jpg.out_color_space = JCS_CMYK;
                break;
            case JCS_UNKNOWN:
                break;
            default:
                jpg.out_color_space = JCS_EXT_BGRA;
                break;
        }
#endif // LIBJPEG_TURBO_VERSION

        jpeg_start_decompress(&jpg);

        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::RGB, jpg.output_width, jpg.output_height);

        while (jpg.output_scanline < jpg.output_height) {
            argb_t* line = &pm.at(0, jpg.output_scanline);
            jpeg_read_scanlines(&jpg, reinterpret_cast<JSAMPARRAY>(&line), 1);

            // convert to argb
            if (jpg.out_color_components == 1) { // grayscale
                argb_t* pixel = line;
                for (int x = jpg.output_width - 1; x >= 0; --x) {
                    const uint8_t color =
                        *(reinterpret_cast<const uint8_t*>(line) + x);
                    pixel[x].a = argb_t::max;
                    pixel[x].r = color;
                    pixel[x].g = color;
                    pixel[x].b = color;
                }
            } else if (jpg.out_color_components == 3) { // rgb
                argb_t* pixel = line;
                for (int x = jpg.output_width - 1; x >= 0; --x) {
                    const uint8_t* color =
                        reinterpret_cast<const uint8_t*>(line) + x * 3;
                    pixel[x].a = argb_t::max;
                    pixel[x].r = color[0];
                    pixel[x].g = color[1];
                    pixel[x].b = color[2];
                }
            } else if (jpg.out_color_space == JCS_CMYK) {
                argb_t* pixel = line;
                for (size_t x = 0; x < pm.width(); ++x) {
                    const uint8_t* color =
                        reinterpret_cast<const uint8_t*>(line) + x * 4;
                    const double c = color[0];
                    const double m = color[1];
                    const double y = color[2];
                    const double k = color[3];
                    pixel[x].a = argb_t::max;
                    pixel[x].r = c * k / argb_t::max;
                    pixel[x].g = m * k / argb_t::max;
                    pixel[x].b = y * k / argb_t::max;
                }
            }
        }

        format = std::format("JPEG {}bit", jpg.num_components * 8);

        jpeg_finish_decompress(&jpg);
        jpeg_destroy_decompress(&jpg);

        return true;
    }
};

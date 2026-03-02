// SPDX-License-Identifier: MIT
// SVG format decoder.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wexpansion-to-defined"
#include <librsvg/rsvg.h>
#pragma GCC diagnostic pop

#include <cstring>
#include <format>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// register format in factory
class ImageSvg;
static const ImageLoader::Registrator<ImageSvg>
    image_format_registartion("SVG", ImageLoader::Priority::Low);

/* SVG image. */
class ImageSvg : public Image {
private:
    // Max offset of the root svg node in xml file
    static constexpr size_t MAX_SIGNATURE_OFFSET = 1024;

    // Canvas sizes
    static constexpr size_t CANVAS_SIZE_MIN_PX = 500;
    static constexpr size_t CANVAS_SIZE_MAX_PX = 2000;
    static constexpr size_t CANVAS_SIZE_DEF_PX = 1000;

    // Cairo wrappers
    using Cairo = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;
    using CairoSurface =
        std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;

    /**
     * Check if data is SVG.
     * @param data image data
     * @return true if it is an SVG format
     */
    static bool is_svg(const std::vector<uint8_t>& data)
    {
        const uint8_t signature[] = { '<', 's', 'v', 'g' };
        if (data.size() < sizeof(signature)) {
            return false;
        }

        const size_t max_offset =
            std::min(MAX_SIGNATURE_OFFSET, data.size() - sizeof(signature));

        bool found = false;
        for (size_t i = 0; !found && i < max_offset; ++i) {
            found = std::memcmp(&data[i], signature, sizeof(signature));
        }
        return found;
    }

    /**
     * Get canvas size.
     * @return output canvas rectangle
     */
    RsvgRectangle get_canvas() const
    {
        RsvgRectangle canvas {};

        RsvgLength svg_w, svg_h;
        RsvgRectangle viewbox;
        gboolean width_ok = TRUE, height_ok = TRUE, viewbox_ok = TRUE;
        rsvg_handle_get_intrinsic_dimensions(svg, &width_ok, &svg_w, &height_ok,
                                             &svg_h, &viewbox_ok, &viewbox);

        if (viewbox_ok) {
            canvas = viewbox;
        } else if (width_ok && height_ok) {
            canvas.width = svg_w.length;
            canvas.height = svg_h.length;
            if (svg_w.unit == RSVG_UNIT_PERCENT) {
                canvas.width *= CANVAS_SIZE_DEF_PX;
                canvas.height *= CANVAS_SIZE_DEF_PX;
            }
        } else {
            canvas.width = CANVAS_SIZE_DEF_PX;
            canvas.height = CANVAS_SIZE_DEF_PX;
        }

        if (canvas.width < CANVAS_SIZE_MIN_PX ||
            canvas.height < CANVAS_SIZE_MIN_PX) {
            const double scale = static_cast<double>(CANVAS_SIZE_MIN_PX) /
                std::max(canvas.width, canvas.height);
            canvas.width *= scale;
            canvas.height *= scale;
        }
        if (canvas.width > CANVAS_SIZE_MAX_PX ||
            canvas.height > CANVAS_SIZE_MAX_PX) {
            const double scale = static_cast<double>(CANVAS_SIZE_MAX_PX) /
                std::max(canvas.width, canvas.height);
            canvas.width *= scale;
            canvas.height *= scale;
        }

        return canvas;
    }

    /**
     * Set format description.
     */
    void set_format()
    {
        RsvgLength svg_w, svg_h;
        RsvgRectangle viewbox;
        gboolean width_ok = TRUE, height_ok = TRUE, viewbox_ok = TRUE;
        rsvg_handle_get_intrinsic_dimensions(svg, &width_ok, &svg_w, &height_ok,
                                             &svg_h, &viewbox_ok, &viewbox);
        const char* units = nullptr;
        if (width_ok && height_ok && svg_w.length != 1.0 &&
            svg_h.length != 1.0) {
            switch (svg_w.unit) {
                case RSVG_UNIT_PERCENT:
                    svg_w.length *= 100;
                    svg_h.length *= 100;
                    units = "%";
                    break;
                case RSVG_UNIT_PX:
                    units = "px";
                    break;
                case RSVG_UNIT_EM:
                    units = "em";
                    break;
                case RSVG_UNIT_EX:
                    units = "ex";
                    break;
                case RSVG_UNIT_IN:
                    units = "in";
                    break;
                case RSVG_UNIT_CM:
                    units = "cm";
                    break;
                case RSVG_UNIT_MM:
                    units = "mm";
                    break;
                case RSVG_UNIT_PT:
                    units = "pt";
                    break;
                case RSVG_UNIT_PC:
                    units = "pc";
                    break;
                case RSVG_UNIT_CH:
                    units = "ch";
                    break;
            }
        } else if (viewbox_ok) {
            svg_w.length = viewbox.width;
            svg_h.length = viewbox.height;
            units = "px";
        } else {
            svg_w.length = 100;
            svg_h.length = 100;
            units = "%";
        }

        format = "SVG";
        if (units) {
            format += std::format(" {}x{}{}", static_cast<int>(svg_w.length),
                                  static_cast<int>(svg_h.length), units);
        }
    }

public:
    ~ImageSvg()
    {
        if (svg) {
            g_object_unref(svg);
        }
    }

    void flip_vertical() override
    {
        Image::flip_vertical();
        svg_flip_v = !svg_flip_v;
    }

    void flip_horizontal() override
    {
        Image::flip_horizontal();
        svg_flip_h = !svg_flip_h;
    }

    void rotate(const size_t angle) override
    {
        Image::rotate(angle);
        svg_rotation += angle;
        svg_rotation %= 360;
    }

    void draw(const size_t, Pixmap& target, const double scale, const ssize_t x,
              const ssize_t y) override
    {
        Pixmap& pm = frames[0].pm;
        const RsvgRectangle viewbox = {
            .x = x + scale * svg_offset_x,
            .y = y - scale * svg_offset_y,
            .width = scale * pm.width(),
            .height = scale * pm.height(),
        };

        // prepare cairo surface
        CairoSurface surface(
            cairo_image_surface_create_for_data(
                reinterpret_cast<unsigned char*>(&target.at(0, 0)),
                CAIRO_FORMAT_ARGB32, target.width(), target.height(),
                target.stride()),
            &cairo_surface_destroy);
        if (!surface ||
            cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS) {
            return;
        }
        Cairo cairo(cairo_create(surface.get()), &cairo_destroy);
        if (!cairo || cairo_status(cairo.get()) != CAIRO_STATUS_SUCCESS) {
            return;
        }

        // transform svg
        if (svg_rotation || svg_flip_h || svg_flip_v) {
            cairo_translate(cairo.get(), viewbox.width / 2 + x,
                            viewbox.height / 2 + y);

            if (svg_rotation) {
                cairo_rotate(cairo.get(),
                             static_cast<double>(svg_rotation) * M_PI / 180.0);
                if (svg_rotation == 90 || svg_rotation == 270) {
                    // rescale to match landscape viewbox size
                    const double scale =
                        static_cast<double>(pm.height()) / pm.width();
                    cairo_scale(cairo.get(), scale, scale);
                }
            }

            if (svg_flip_h) {
                cairo_scale(cairo.get(), -1.0, 1.0);
            }
            if (svg_flip_v) {
                cairo_scale(cairo.get(), 1.0, -1.0);
            }

            cairo_translate(cairo.get(), -viewbox.width / 2 - x,
                            -viewbox.height / 2 - y);
        }

        // render svg to cairo surface
        rsvg_handle_render_document(svg, cairo.get(), &viewbox, nullptr);
    }

    bool load(const std::vector<uint8_t>& data) override
    {
        if (!is_svg(data)) {
            return false;
        }

        // open decoder
        svg = rsvg_handle_new_from_data(data.data(), data.size(), nullptr);
        if (!svg) {
            return false;
        }

        // get canvas size and offsets
        const RsvgRectangle canvas = get_canvas();
        if (canvas.x) {
            svg_offset_x = canvas.width / canvas.x;
        }
        if (canvas.y) {
            svg_offset_y = canvas.height / canvas.y;
        }

        // render to pixmap that will be used in the export action
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::ARGB, canvas.width, canvas.height);
        draw(0, pm, 1.0, 0, 0);

        set_format();

        return true;
    }

private:
    RsvgHandle* svg = nullptr; ///< RSVG handle containing the image data
    double svg_offset_x = 0.0; ///< Horizontal offset relative to canvas
    double svg_offset_y = 0.0; ///< Vertical offset relative to canvas
    size_t svg_rotation = 0;   ///< Rotation in degrees (90/180/270)
    bool svg_flip_v = false;   ///< Whether to flip the image vertically
    bool svg_flip_h = false;   ///< Whether to flip the image horizontally
};

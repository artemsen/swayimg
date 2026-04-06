// SPDX-License-Identifier: MIT
// OTF/TTF fonts format.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../application.hpp"
#include "../font.hpp"
#include "../imageformat.hpp"

class ImageFormatTtf : public ImageFormat {
public:
    ImageFormatTtf()
        : ImageFormat(Priority::Low, "ttf")
    {
    }

    // Text color
    static constexpr const argb_t COLOR { argb_t::max, argb_t::max, argb_t::max,
                                          argb_t::max };
    // Text to render
    static constexpr const char* TEXT =
        "The quick brown fox jumps over the lazy dog 0123456789";

    ImagePtr decode(const Data& data) override
    {
        if (!check_signature(data, { 0x00, 0x01, 0x00, 0x00 })) {
            return nullptr;
        }

        Font font;
        if (!font.load(data.data, data.size)) {
            return nullptr;
        }

        // allocate image and frame
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);

        // create canvas with window size
        Pixmap& pm = image->frames[0].pm;
        const Size wnd_size = Application().get_ui()->get_window_size();
        pm.create(Pixmap::ARGB, wnd_size.width, wnd_size.height);

        // render text
        size_t y = 0;
        for (size_t i = 1; y < pm.height(); ++i) {
            font.set_size(12 + i * i);
            const Pixmap pm_text = font.render(TEXT);
            pm.mask(pm_text, { 0, static_cast<ssize_t>(y) }, COLOR);
            y += pm_text.height();
        }

        image->format = "Font";
        const char* name = font.name();
        if (name) {
            image->format += ' ';
            image->format += name;
        }

        return image;
    }
};

// register format in factory
static ImageFormatTtf format_ttf;

// SPDX-License-Identifier: MIT
// OTF/TTF fonts format decoder.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../application.hpp"
#include "../font.hpp"
#include "../imageloader.hpp"

// register format in factory
class ImageOttf;
static const ImageLoader::Registrator<ImageOttf>
    image_format_registartion("TTF/OTF", ImageLoader::Priority::Low);

/* Font preview image. */
class ImageOttf : public Image {
private:
    // TTF/OTF signature
    static constexpr const uint8_t signature[] = { 0x00, 0x01, 0x00, 0x00 };

    // Text color
    static constexpr const argb_t COLOR { argb_t::max, argb_t::max, argb_t::max,
                                          argb_t::max };
    // Text to render
    static constexpr const char* TEXT =
        "The quick brown fox jumps over the lazy dog";

public:
    bool load(const Data& data) override
    {
        // check signature
        if (data.size < sizeof(signature) ||
            std::memcmp(data.data, signature, sizeof(signature))) {
            return false;
        }

        Font font;
        if (!font.load(data.data, data.size)) {
            return false;
        }

        // create canvas with window size
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        const Size wnd_size = Application().get_ui()->get_window_size();
        pm.create(Pixmap::ARGB, wnd_size.width, wnd_size.height);

        // render text
        size_t y = 0;
        for (size_t i = 0; y < pm.height(); ++i) {
            font.set_size(12 + i * 14);
            const Pixmap pm_text = font.render(TEXT);
            pm.mask(pm_text, { 0, static_cast<ssize_t>(y) }, COLOR);
            y += pm_text.height();
        }

        format = "Font";
        const char* name = font.name();
        if (name) {
            format += ' ';
            format += name;
        }

        return true;
    }
};

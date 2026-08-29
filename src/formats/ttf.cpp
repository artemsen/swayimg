// SPDX-License-Identifier: MIT
// OTF/TTF fonts format.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../application.hpp"
#include "../font.hpp"
#include "../imageformat.hpp"

namespace {

class ImageFormatTtf : public ImageFormat {
public:
    ImageFormatTtf() noexcept
        : ImageFormat(Priority::Low, "ttf")
    {
    }

    // Supported font file signatures
    static constexpr const uint8_t SIGNATURE_TTF[] = { 0x00, 0x01, 0x00, 0x00 };
    static constexpr const uint8_t SIGNATURE_OTF[] = { 0x4f, 0x54, 0x54, 0x4f };
    static constexpr const uint8_t SIGNATURE_WOFF[] = { 0x77, 0x4f, 0x46 };

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        if (!check_signature(data, SIGNATURE_TTF) &&
            !check_signature(data, SIGNATURE_OTF) &&
            !check_signature(data, SIGNATURE_WOFF)) {
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
        const Size wnd_size = Application::get_ui()->get_window_size();
        pm.create(Pixmap::ARGB, wnd_size.width, wnd_size.height);
        pm.fill({ 0, 0, pm.width(), pm.height() }, bkg);

        // render text
        size_t y = 0;
        for (size_t i = 1; y < pm.height(); ++i) {
            font.set_size(12 + i * i);
            const Pixmap pm_text = font.render(text);
            pm.mask(pm_text, { .x = 0, .y = static_cast<ssize_t>(y) }, color);
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

    void set_config(Config& params) override
    {
        ImageFormat::set_config(params);
        params.get("text", text, 1);
        params.get("color", color);
        params.get("background", bkg);
    }

private:
    // Text and its color
    std::string text = "The quick brown fox jumps over the lazy dog 0123456789";
    argb_t color = { argb_t::max, argb_t::max, argb_t::max, argb_t::max };
    argb_t bkg = { argb_t::min, argb_t::min, argb_t::min, argb_t::min };
};

// register format in factory
ImageFormatTtf format_ttf;

} // anonymous namespace

// SPDX-License-Identifier: MIT
// Font render.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.hpp"

#include "application.hpp"
#include "log.hpp"

#include <fontconfig/fontconfig.h>

#include <memory>

constexpr size_t POINT_FACTOR = 64;  // default points per pixel (26.6 format)
constexpr size_t MAX_TEXT_LEN = 120; // max length of text line (characters)

/** Font config wrapper.*/
class FontConfig {
public:
    using FcConfigPtr = std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)>;
    using FcPatternPtr =
        std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)>;

    FontConfig() { FcInit(); }
    ~FontConfig() { FcFini(); }

    /**
     * Get path to the font file by its name.
     * @param name font name
     * @return path to the file or empty string if font not found
     */
    std::string get_font_file(const char* name) const
    {
        FcConfigPtr fc =
            FcConfigPtr(FcInitLoadConfigAndFonts(), &FcConfigDestroy);
        if (!fc) {
            return {};
        }

        FcPatternPtr fc_name =
            FcPatternPtr(FcNameParse(reinterpret_cast<const FcChar8*>(name)),
                         FcPatternDestroy);
        if (!fc_name) {
            return {};
        }
        FcConfigSubstitute(fc.get(), fc_name.get(), FcMatchPattern);
        FcDefaultSubstitute(fc_name.get());

        FcResult result;
        FcPatternPtr fc_font = FcPatternPtr(
            FcFontMatch(fc.get(), fc_name.get(), &result), FcPatternDestroy);
        if (fc_font) {
            FcChar8* path = nullptr;
            if (FcPatternGetString(fc_font.get(), FC_FILE, 0, &path) ==
                FcResultMatch) {
                return reinterpret_cast<const char*>(path);
            }
        }

        return {};
    }
};

Font::~Font()
{
    if (ft_face) {
        FT_Done_Face(ft_face);
    }
    if (ft_lib) {
        FT_Done_FreeType(ft_lib);
    }
}

void Font::initialize()
{
    set_face(face);
}

void Font::set_face(const std::string& name)
{
    // get font file via Fontconfig
    const std::string path = FontConfig().get_font_file(name.c_str());
    if (path.empty()) {
        Log::error("Unable to find font {}", name);
        return;
    }

    // init FreeType
    FT_Error rc;
    if (!ft_lib) {
        rc = FT_Init_FreeType(&ft_lib);
        if (rc != 0) {
            Log::error("Unable to initialize freetype ({})", rc);
            return;
        }
    }

    // load font
    FT_Face new_face = nullptr;
    rc = FT_New_Face(ft_lib, path.c_str(), 0, &new_face);
    if (rc != 0) {
        Log::error("Unable to load font from %s (%d)", path, rc);
        return;
    }
    if (ft_face) {
        FT_Done_Face(ft_face);
    }
    ft_face = new_face;

    face = name;
    set_size(size);
}

void Font::set_scale(const double scale)
{
    this->scale = scale;
    set_size(size);
}

void Font::set_size(const size_t size)
{
    this->size = size;
    FT_Set_Pixel_Sizes(ft_face, 0, size * scale);
    Application::get_text().refresh();
    Application::self().redraw();
}

Pixmap Font::render(const std::string& text) const
{
    size_t len = text.length();
    if (len == 0) {
        return {};
    }

    // convert text to wide-character string
    std::wstring wide(len + 1, 0);
    len = std::mbstowcs(wide.data(), text.c_str(), len * sizeof(wide[0]));
    if (len == static_cast<size_t>(-1)) {
        abort();
        return {};
    }
    wide.resize(len);
    if (len > MAX_TEXT_LEN) {
        wide.resize(MAX_TEXT_LEN - 1);
        wide += L'…';
    }

    // calculate total width in pixels
    size_t width = 0;
    for (const wchar_t ch : wide) {
        if (FT_Load_Char(ft_face, ch, FT_LOAD_RENDER) == 0) {
            const FT_GlyphSlot glyph = ft_face->glyph;
            width += glyph->advance.x / POINT_FACTOR;
        }
    }

    // calculate text height in pixels
    const size_t height_base = ft_face->size->metrics.height / POINT_FACTOR;
    const size_t height = height_base + height_base / 3; // dirty hack

    // horizontal padding
    const size_t hpadding = height_base / 3;

    Pixmap pm;
    pm.create(Pixmap::GS, width + hpadding * 2, height);

    // draw glyphs
    size_t x = hpadding;
    for (const wchar_t ch : wide) {
        if (FT_Load_Char(ft_face, ch, FT_LOAD_RENDER) != 0) {
            continue; // something wrong
        }
        const FT_GlyphSlot glyph = ft_face->glyph;
        const FT_Bitmap* bmp = &glyph->bitmap;
        const size_t x_start = x + glyph->bitmap_left;
        const size_t y_start = height_base - glyph->bitmap_top;
        for (size_t y = 0; y < bmp->rows; ++y) {
            if (y + y_start < pm.height() &&
                x_start + bmp->width < pm.width()) {
                uint8_t* dst =
                    reinterpret_cast<uint8_t*>(pm.ptr(x_start, y + y_start));
                std::memcpy(dst, &bmp->buffer[y * bmp->pitch], bmp->width);
            }
        }
        x += glyph->advance.x / POINT_FACTOR;
    }

    return pm;
}

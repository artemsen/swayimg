// SPDX-License-Identifier: MIT
// Font render.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.hpp"

#include "defaults.hpp"
#include "log.hpp"

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <memory>

namespace {

constexpr const char FALLBACK_CHR = '?'; // character used for absent glyphs

constexpr size_t POINT_FACTOR = 64;  // default points per pixel (26.6 format)
constexpr size_t MAX_TEXT_LEN = 120; // max length of text line (characters)

/** Convert FreeType 26.6 fixed point value to whole pixels. */
constexpr size_t to_pixels(const FT_Pos value)
{
    return value / POINT_FACTOR;
}

constexpr size_t h_padding(const size_t height_base)
{
    return height_base / 3; // dirty hack
}

/** Line height: base height with room below for descenders. */
constexpr size_t line_height(const size_t height_base)
{
    return height_base + h_padding(height_base);
}

/** Font config wrapper.*/
class FontConfig {
public:
    using FcConfigPtr = std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)>;
    using FcPatternPtr =
        std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)>;

    FontConfig()
    {
        if (!FcInit()) {
            Log::error("Unable to initialize FontConfig");
        }
    }

    ~FontConfig() { FcFini(); }

    /**
     * Get path to the font file by its name.
     * @param name font name
     * @return path to the file or empty string if font not found
     */
    static std::filesystem::path get_font_file(const char* name)
    {
        const FcConfigPtr fc =
            FcConfigPtr(FcInitLoadConfigAndFonts(), &FcConfigDestroy);
        if (!fc) {
            return {};
        }

        const FcPatternPtr fc_name =
            FcPatternPtr(FcNameParse(reinterpret_cast<const FcChar8*>(name)),
                         FcPatternDestroy);
        if (!fc_name) {
            return {};
        }
        FcConfigSubstitute(fc.get(), fc_name.get(), FcMatchPattern);
        FcDefaultSubstitute(fc_name.get());

        FcResult result;
        const FcPatternPtr fc_font = FcPatternPtr(
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

/** FreeType lib wrapper.*/
struct FreeTypeLib {
    FreeTypeLib() noexcept
    {
        FT_Error rc;
        rc = FT_Init_FreeType(&lib);
        if (rc != 0) {
            Log::error("Unable to initialize FreeType: {}",
                       FreeTypeLib::error_text(rc));
        }
    }

    ~FreeTypeLib()
    {
        if (lib) {
            FT_Done_FreeType(lib);
        }
    }

    /**
     * Get error message for specified code.
     * @param rc error code
     * @return error message
     */
    static const char* error_text(const FT_Error rc)
    {
        const char* text = FT_Error_String(rc);
        return text ? text : "unknown error";
    }

    operator FT_Library() { return lib; }

private:
    FT_Library lib = nullptr; ///< Font lib instance
};

FreeTypeLib ft_lib;

} // anonymous namespace

Font::Font()
    : size(Defaults::text::size)
{
}

Font::~Font()
{
    if (ft_face) {
        FT_Done_Face(ft_face);
    }
}

bool Font::load(const std::string& name)
{
    if (!ft_lib) {
        return false;
    }

    // get font file via FontConfig
    const FontConfig fcinit;
    const std::filesystem::path path = FontConfig::get_font_file(name.c_str());
    if (path.empty()) {
        Log::error("Unable to find font {}", name);
        return false;
    }

    return load(path);
}

bool Font::load(const std::filesystem::path& path)
{
    if (!ft_lib) {
        return false;
    }

    FT_Face face;
    const FT_Error rc = FT_New_Face(ft_lib, path.c_str(), 0, &face);
    if (rc != 0) {
        Log::error("Unable to load font from {}: {}", path.string(),
                   FreeTypeLib::error_text(rc));
        return false;
    }
    set_face(face);

    return true;
}

bool Font::load(const uint8_t* data, const size_t data_size)
{
    if (!ft_lib) {
        return false;
    }

    FT_Face face;
    const FT_Error rc = FT_New_Memory_Face(ft_lib, data, data_size, 0, &face);
    if (rc != 0) {
        Log::error("Unable to load font: {}", FreeTypeLib::error_text(rc));
        return false;
    }
    set_face(face);

    return true;
}

void Font::set_face(FT_Face face)
{
    if (ft_face) {
        FT_Done_Face(ft_face);
    }
    ft_face = face;

    set_size(size);
}

const char* Font::name() const
{
    return ft_face ? ft_face->family_name : nullptr;
}

void Font::set_size(const size_t size)
{
    this->size = size;
    // all rendering changes (font face, size, scale) go through here,
    // so the cache must be emptied to match the new pixel sizes
    glyph_cache.clear();
    if (ft_face) {
        FT_Set_Pixel_Sizes(ft_face, 0, size * scale);
    }
}

void Font::set_scale(const double scale)
{
    this->scale = scale;
    set_size(size);
}

std::wstring Font::to_wide(const std::string& text)
{
    size_t len = text.length();
    std::wstring wide(len + 1, 0);

    len = std::mbstowcs(wide.data(), text.c_str(), len * sizeof(wide[0]));
    if (len != std::wstring::npos) {
        wide.resize(len);
    } else {
        // something wrong with locale, try to convert ASCII
        wide.clear();
        for (const auto chr : text) {
            wide += chr < ' ' || chr > '~' ? FALLBACK_CHR : chr;
        }
        len = wide.length();
    }

    if (len > MAX_TEXT_LEN) {
        wide.resize(MAX_TEXT_LEN - 1);
        wide += L'…';
    }

    return wide;
}

const Pixmap& Font::get_glyph(const wchar_t ch)
{
    const auto [it, created] = glyph_cache.try_emplace(ch);
    if (!created) {
        return it->second;
    }

    // fallback glyph is used for absent or broken characters
    const FT_UInt index = FT_Get_Char_Index(ft_face, ch);
    if (index == 0 ||
        FT_Load_Glyph(ft_face, index, FT_LOAD_DEFAULT) != FT_Err_Ok) {
        FT_Load_Char(ft_face, FALLBACK_CHR, FT_LOAD_DEFAULT);
    }

    rasterize(it->second);
    return it->second;
}

void Font::rasterize(Pixmap& raster)
{
    FT_GlyphSlot slot = ft_face->glyph;

    // the image spans the whole advance cell, so render() steps the pen
    // by its width
    const size_t height_base = to_pixels(ft_face->size->metrics.height);
    raster.create(Pixmap::GS, to_pixels(slot->advance.x),
                  line_height(height_base));

    if (slot->format == FT_GLYPH_FORMAT_OUTLINE &&
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != FT_Err_Ok) {
        return; // broken glyph: blank cell, the pen still steps
    }

    const FT_Bitmap& bitmap = slot->bitmap;

    // pixels left of the pen (e.g. italic overhangs) have no room in
    // a pen-stamped image, clamp them to the pen position
    const size_t x_start = slot->bitmap_left > 0 ? slot->bitmap_left : 0;
    const ssize_t y_start =
        static_cast<ssize_t>(height_base) - slot->bitmap_top;

    // copy the visible part of the bitmap, ink outside the cell is cut
    const ssize_t first = std::max<ssize_t>(y_start, 0);
    const ssize_t last =
        std::min<ssize_t>(y_start + bitmap.rows, raster.height());
    if (first < last && x_start < raster.width()) {
        const size_t copy_width =
            std::min<size_t>(bitmap.width, raster.width() - x_start);
        uint8_t* dst = static_cast<uint8_t*>(raster.ptr(x_start, first));
        const uint8_t* src = &bitmap.buffer[(first - y_start) * bitmap.pitch];
        for (ssize_t y = first; y < last; ++y) {
            std::memcpy(dst, src, copy_width);
            dst += raster.stride();
            src += bitmap.pitch;
        }
    }
}

Pixmap Font::render(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    if (!ft_face && !load(std::string(Defaults::text::font))) {
        return {};
    }

    const std::wstring wide = to_wide(text);

    // calculate total width in pixels
    size_t width = 0;
    for (const wchar_t ch : wide) {
        width += get_glyph(ch).width();
    }

    const size_t height_base = to_pixels(ft_face->size->metrics.height);
    const size_t hpadding = h_padding(height_base);

    Pixmap pm;
    pm.create(Pixmap::GS, width + hpadding * 2, line_height(height_base));

    // draw glyphs: the images are already placed relative to the pen
    size_t x = hpadding;
    for (const wchar_t ch : wide) {
        const Pixmap& glyph = get_glyph(ch);
        if (glyph) {
            const size_t copy_width = std::min(glyph.width(), pm.width() - x);
            for (size_t y = 0; y < pm.height(); ++y) {
                std::memcpy(pm.ptr(x, y), glyph.ptr(0, y), copy_width);
            }
        }
        x += glyph.width();
    }

    return pm;
}

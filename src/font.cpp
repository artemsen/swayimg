// SPDX-License-Identifier: MIT
// Font render.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.hpp"

#include "defaults.hpp"
#include "log.hpp"

#include <fontconfig/fontconfig.h>

#include <memory>

namespace {

/** Character used for absent glyphs. */
constexpr const wchar_t FALLBACK_CHR = L'?';

/** Default points per pixel (26.6 format). */
constexpr size_t POINT_FACTOR = 64;

/** Font config wrapper.*/
class FontConfig {
public:
    using PatternPtr = std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)>;
    using CharSetPtr = std::unique_ptr<FcCharSet, decltype(&FcCharSetDestroy)>;
    using FontSetPtr = std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)>;

    FontConfig() noexcept
    {
        if (FcInit()) {
            config = FcInitLoadConfigAndFonts();
        }
        if (!config) {
            Log::error("Unable to initialize FontConfig");
        }
    }

    ~FontConfig()
    {
        if (config) {
            FcConfigDestroy(config);
        }
        FcFini();
    }

    /**
     * Get global instance of font config.
     * @return font config instance
     */
    static FontConfig& self()
    {
        static FontConfig singleton;
        return singleton;
    }

    /**
     * Get path to the font file by its name.
     * @param name font name
     * @return path to the file or empty string if font not found
     */
    std::filesystem::path get_font_file(const char* name)
    {
        assert(config);

        const PatternPtr name_pat =
            PatternPtr(FcNameParse(reinterpret_cast<const FcChar8*>(name)),
                       FcPatternDestroy);
        if (!name_pat) {
            return {};
        }
        FcConfigSubstitute(config, name_pat.get(), FcMatchPattern);
        FcDefaultSubstitute(name_pat.get());

        FcResult result;
        const PatternPtr font_pat = PatternPtr(
            FcFontMatch(config, name_pat.get(), &result), FcPatternDestroy);
        if (font_pat) {
            FcChar8* path = nullptr;
            if (FcPatternGetString(font_pat.get(), FC_FILE, 0, &path) ==
                FcResultMatch) {
                return reinterpret_cast<const char*>(path);
            }
        }

        return {};
    }

    /**
     * Get list of font files that may contain the specified glyph.
     * A font may still lack the glyph despite matching the charset, so several
     * candidates are returned in order of preference to try one by one.
     * @param ch needed char
     * @return ordered list of font file paths (empty if no candidates)
     */
    std::vector<std::filesystem::path> get_font_files(const wchar_t ch)
    {
        assert(config);

        // create a charset object and add our character to it
        const CharSetPtr charset(FcCharSetCreate(), FcCharSetDestroy);
        if (!charset || !FcCharSetAddChar(charset.get(), ch)) {
            return {};
        }

        // create a pattern representing our search criteria
        const PatternPtr search =
            PatternPtr(FcPatternCreate(), FcPatternDestroy);
        if (!search ||
            !FcPatternAddCharSet(search.get(), FC_CHARSET, charset.get())) {
            return {};
        }

        FcConfigSubstitute(config, search.get(), FcMatchPattern);
        FcDefaultSubstitute(search.get());

        // get ordered list of matching fonts
        FcResult result;
        FontSetPtr matches(
            FcFontSort(config, search.get(), FcFalse, nullptr, &result),
            FcFontSetDestroy);
        if (!matches) {
            return {};
        }

        std::vector<std::filesystem::path> files;
        files.reserve(matches->nfont);

        for (int i = 0; i < matches->nfont; ++i) {
            FcChar8* path = nullptr;
            if (FcPatternGetString(matches->fonts[i], FC_FILE, 0, &path) ==
                FcResultMatch) {
                const std::filesystem::path file(
                    reinterpret_cast<const char*>(path));
                // avoid duplicates (same file matched with different styles)
                if (std::find(files.begin(), files.end(), file) ==
                    files.end()) {
                    files.push_back(file);
                }
            }
        }

        return files;
    }

    /**
     * Check if font config is available.
     * @return true if font config is loaded
     */
    operator bool() const { return !!config; }

private:
    FcConfig* config { nullptr }; ///< Font config instance
};

/** FreeType lib wrapper.*/
struct FreeType {
    FreeType() noexcept
    {
        FT_Error rc;
        rc = FT_Init_FreeType(&lib);
        if (rc != 0) {
            Log::error("Unable to initialize FreeType: {}",
                       FreeType::error_text(rc));
        }
    }

    ~FreeType()
    {
        if (lib) {
            FT_Done_FreeType(lib);
        }
    }

    /**
     * Get global instance of font free lib.
     * @return font free lib instance
     */
    static FreeType& self()
    {
        static FreeType singleton;
        return singleton;
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
    FT_Library lib { nullptr }; ///< Font lib instance
};

/**
 * Rasterize the glyph from the loaded slot.
 * @param glyph loaded glyph slot
 * @return rasterized pixmap
 */
Pixmap rasterize_glyph(FT_GlyphSlot glyph)
{
    Pixmap pm;

    const FT_Bitmap& bitmap = glyph->bitmap;

    // color glyphs are stored as BGRA (blue, green, red, alpha) bitmaps,
    // which matches the native argb_t layout, everything else is a mask
    const bool color = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;

    // calculate pixmap size in pixels
    const size_t width = glyph->advance.x / POINT_FACTOR;
    const size_t base_h = glyph->face->size->metrics.height / POINT_FACTOR;
    const size_t height = base_h + base_h / 3; // dirty hack

    // calculate intersection glyph bitmap with destination pixmap
    const Rectangle rc_glyph(glyph->bitmap_left,
                             static_cast<ssize_t>(base_h) - glyph->bitmap_top,
                             bitmap.width, bitmap.rows);
    const Rectangle rc_pm = rc_glyph.intersect({ 0, 0, width, height });
    if (!rc_pm) {
        // space
        pm.create(Pixmap::GS, width, height);
        return pm;
    }

    pm.create(color ? Pixmap::ARGB : Pixmap::GS, width, height);

    // copy bitmap to the target pixmap
    const size_t copy_width = rc_pm.width * (color ? sizeof(argb_t) : 1);
    for (size_t y = 0; y < rc_pm.height; ++y) {
        const uint8_t* src = &bitmap.buffer[y * bitmap.pitch];
        uint8_t* dst = static_cast<uint8_t*>(pm.ptr(rc_pm.x, rc_pm.y + y));
        std::memcpy(dst, src, copy_width);
    }

    return pm;
}

} // anonymous namespace

Font::Font() noexcept
    : size(Defaults::text::size)
{
}

bool Font::load(const std::string& name)
{
    FontConfig& fc = FontConfig::self();
    if (!fc) {
        return false;
    }

    // get font file via FontConfig
    const FontConfig fcinit;
    const std::filesystem::path path = fc.get_font_file(name.c_str());
    if (path.empty()) {
        Log::error("Unable to find font {}", name);
        return false;
    }

    return load(path);
}

bool Font::load(const std::filesystem::path& path)
{
    FreeType& ft = FreeType::self();
    if (!ft) {
        return false;
    }

    FT_Face face;
    const FT_Error rc = FT_New_Face(ft, path.c_str(), 0, &face);
    if (rc != 0) {
        Log::error("Unable to load font from {}: {}", path.string(),
                   FreeType::error_text(rc));
        return false;
    }
    set_face(face);

    return true;
}

bool Font::load(const uint8_t* data, const size_t data_size)
{
    FreeType& ft = FreeType::self();
    if (!ft) {
        return false;
    }

    FT_Face face;
    const FT_Error rc = FT_New_Memory_Face(ft, data, data_size, 0, &face);
    if (rc != 0) {
        Log::error("Unable to load font: {}", FreeType::error_text(rc));
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

    if (ft_face) {
        glyphs.clear();
        FT_Set_Pixel_Sizes(ft_face, 0, size * scale);
    }
}

void Font::set_scale(const double scale)
{
    this->scale = scale;
    set_size(size);
}

const Pixmap* Font::get_glyph(const wchar_t ch)
{
    if (!ft_face) {
        return nullptr; // font not yet loaded
    }

    // search in cache
    const auto it = glyphs.find(ch);
    if (it != glyphs.end()) {
        return &it->second;
    }

    // load and rasterize glyph
    const FT_UInt index = FT_Get_Char_Index(ft_face, ch);
    if (index != 0 &&
        FT_Load_Glyph(ft_face, index, FT_LOAD_RENDER | FT_LOAD_COLOR) ==
            FT_Err_Ok) {
        auto [it, _] = glyphs.insert({ ch, rasterize_glyph(ft_face->glyph) });
        return &it->second;
    }

    return nullptr; // glyph not found
}

FontSet::FontSet() noexcept
    : size(Defaults::text::size)
{
}

bool FontSet::replace(const std::string& name)
{
    if (!fonts.empty()) {
        // update existing font instance
        return fonts.front()->load(name);
    }

    // load new font instance
    FontPtr font = std::make_unique<Font>();
    if (font->load(name)) {
        font->set_size(size);
        font->set_scale(scale);
        fonts.emplace_back(std::move(font));
        return true;
    }

    return false;
}

void FontSet::set_size(const size_t size)
{
    this->size = size;
    for (auto& font : fonts) {
        font->set_size(size);
    }
}

void FontSet::set_scale(const double scale)
{
    this->scale = scale;
    for (auto& font : fonts) {
        font->set_scale(scale);
    }
}

const Pixmap* FontSet::get_glyph(const wchar_t ch)
{
    // load the default font if it is not already loaded
    if (fonts.empty()) {
        replace(Defaults::text::font);
    }

    // search in loaded fonts
    for (auto& font : fonts) {
        const Pixmap* glyph = font->get_glyph(ch);
        if (glyph) {
            return glyph;
        }
    }

    // search for new font with the specified character support
    FontConfig& fc = FontConfig::self();
    if (fc) {
        for (const auto& path : fc.get_font_files(ch)) {
            FontPtr font = std::make_unique<Font>();
            if (font->load(path)) {
                font->set_size(size);
                font->set_scale(scale);
                const Pixmap* glyph = font->get_glyph(ch);
                if (glyph) {
                    fonts.emplace_back(std::move(font));
                    return glyph;
                }
            }
        }
    }

    // use fallback character from default font
    return fonts.empty() ? nullptr : fonts.front()->get_glyph(FALLBACK_CHR);
}

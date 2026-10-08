// SPDX-License-Identifier: MIT
// Font render.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"

// freetype stuff
#include <ft2build.h>
#include FT_FREETYPE_H

#include <filesystem>
#include <string>
#include <unordered_map>

/** Font render. */
class Font {
public:
    Font();
    ~Font();

    /**
     * Check if font was loaded.
     * @return true if font was loaded
     */
    operator bool() const { return ft_face; }

    /**
     * Load font by name.
     * @param name font face name
     * @return false if font wasn't loaded
     */
    bool load(const std::string& name);

    /**
     * Load font from file.
     * @param path path to font file
     * @return false if font wasn't loaded
     */
    bool load(const std::filesystem::path& path);

    /**
     * Load font from memory buffer.
     * @param data font data buffer
     * @param data_size buffer size
     * @return false if font wasn't loaded
     */
    bool load(const uint8_t* data, const size_t data_size);

    /**
     * Get font name.
     * @return font name
     */
    [[nodiscard]] const char* name() const;

    /**
     * Set font size.
     * @param size new font size in pixels
     */
    void set_size(const size_t size);

    /**
     * Set font scale based on Wayland scale.
     * @param scale the scale factor to multiply by
     */
    void set_scale(const double scale);

    /**
     * Get glyph pixmap (grascale mask) for the character.
     * @param ch character to render
     * @return glyph pixmap
     */
    const Pixmap& get_glyph(const wchar_t ch);

private:
    /**
     * Set new font face.
     * @param face font face to set
     */
    void set_face(FT_Face face);

    /**
     * Rasterize the glyph for the specified character.
     * @param ch character to rasterize
     * @return glyph pixmap
     */
    const Pixmap& rasterize(const wchar_t ch);

private:
    FT_Face ft_face { nullptr }; ///< Font face instance
    size_t size;                 ///< Font size in pixels
    double scale { 1.0 };        ///< Font scale

    std::unordered_map<wchar_t, Pixmap> glyph_cache; ///< Rendered glyph images
};

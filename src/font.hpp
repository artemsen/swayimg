// SPDX-License-Identifier: MIT
// Font render.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"

// freetype stuff
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <filesystem>
#include <string>

/** Font render. */
class Font {
public:
    ~Font();

    /**
     * Check if font was loaded.
     * @return true if font was loaded
     */
    inline operator bool() const { return ft_face; }

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
     * Render single text line.
     * @param text string to print
     * @return masked surface
     */
    [[nodiscard]] Pixmap render(const std::string& text);

private:
    /**
     * Set new font face.
     * @param face font face to set
     */
    void set_face(FT_Face face);

private:
    FT_Face ft_face = nullptr; ///< Font face instance

    size_t size = 24;   ///< Font size in pixels
    double scale = 1.0; ///< Font scale
};

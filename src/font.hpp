// SPDX-License-Identifier: MIT
// Font render.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"

// freetype stuff
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

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
     * Load font.
     * @param name font face name
     * @return false if font wasn't loaded
     */
    bool load(const std::string& name);

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
    Pixmap render(const std::string& text) const;

private:
    FT_Library ft_lib = nullptr; ///< Font lib instance
    FT_Face ft_face = nullptr;   ///< Font face instance

    size_t size = 24;   ///< Font size in pixels
    double scale = 1.0; ///< Font scale
};

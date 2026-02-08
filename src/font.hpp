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
     * Initialize font subsystem.
     */
    void initialize();

    /**
     * Set font fase.
     * @param name font face name
     */
    void set_face(const std::string& name);

    /**
     * Get font face name.
     * @return font face name
     */
    const std::string& get_face() const { return face; }

    /**
     * Set font size.
     * @param size new font size in pixels
     */
    void set_size(const size_t size);

    /**
     * Get font size.
     * @return font size in pixels
     */
    size_t get_size() const { return size; }

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

    std::string face = "monospace"; ///< Font face name
    size_t size = 32;               ///< Font size in pixels
    double scale = 1.0;             ///< Font scale
};

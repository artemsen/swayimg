// SPDX-License-Identifier: MIT
// Color types.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

/** ARGB color (BGRA in little endian). */
struct argb_t {
    uint8_t b = 0; // Blue channel
    uint8_t g = 0; // Green channel
    uint8_t r = 0; // Red channel
    uint8_t a = 0; // Alpha channel

    argb_t() = default;

    /**
     * Constructor.
     * @param color color in little-endian
     */
    argb_t(const uint32_t color)
        : b(color & max)
        , g((color >> 8) & max)
        , r((color >> 16) & max)
        , a((color >> 24) & max)
    {
    }

    /**
     * Constructor.
     * @param a,r,g,b color components
     */
    argb_t(const uint8_t a, const uint8_t r, const uint8_t g, const uint8_t b)
        : b(b)
        , g(g)
        , r(r)
        , a(a)
    {
    }

    /**
     * Check if color is set.
     * @return true if color has value
     */
    constexpr operator bool() const { return a && r && g && b; }

    /**
     * Blend current color (background) with specified one.
     * @param color foreground color to add
     */
    inline void blend(const argb_t& color) { *this = blend(*this, color); }

    /**
     * Blend colors.
     * @param bg background color
     * @param fg foreground color
     * @return result color
     */
    static inline argb_t blend(const argb_t& bg, const argb_t& fg)
    {
        if (fg.a == argb_t::min) {
            return bg; // fully transparent
        }
        if (fg.a == argb_t::max) {
            return fg; // fully opaque
        }

        const double alpha = static_cast<double>(fg.a) / max;
        const double alpha_inv = 1.0 - alpha;
        return { std::max(bg.a, fg.a),
                 static_cast<uint8_t>(alpha * fg.r + alpha_inv * bg.r),
                 static_cast<uint8_t>(alpha * fg.g + alpha_inv * bg.g),
                 static_cast<uint8_t>(alpha * fg.b + alpha_inv * bg.b) };
    }

    // Min and max channel color value
    static constexpr uint8_t min = std::numeric_limits<uint8_t>::min();
    static constexpr uint8_t max = std::numeric_limits<uint8_t>::max();
};

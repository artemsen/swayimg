// SPDX-License-Identifier: MIT
// Color types.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

/** ARGB color (BGRA in little endian). */
struct argb_t {
    using channel = uint8_t;

    channel b = 0; // Blue channel
    channel g = 0; // Green channel
    channel r = 0; // Red channel
    channel a = 0; // Alpha channel

    argb_t() = default;

    /**
     * Constructor.
     * @param color color in little-endian
     */
    constexpr argb_t(const uint32_t color)
        : b(color & max)
        , g((color >> 8) & max)
        , r((color >> 16) & max)
        , a((color >> 24) & max)
    {
    }

    /**
     * Constructor.
     * @param a,r,g,b color channels
     */
    constexpr argb_t(const channel a, const channel r, const channel g,
                     const channel b)
        : b(b)
        , g(g)
        , r(r)
        , a(a)
    {
    }

    /**
     * Cast color to integer value.
     */
    operator uint32_t() const { return a << 24 | r << 16 | g << 8 | b; }

    /**
     * Blend current color (background) with specified one.
     * @param color foreground color to add
     */
    void blend(const argb_t& color) { *this = blend(*this, color); }

    /**
     * Blend colors.
     * @param bg background color
     * @param fg foreground color
     * @return result color
     */
    static argb_t blend(const argb_t& bg, const argb_t& fg)
    {
        if (fg.a == argb_t::min) {
            return bg; // fully transparent
        }
        if (fg.a == argb_t::max) {
            return fg; // fully opaque
        }

        const double alpha = static_cast<double>(fg.a) / argb_t::max;
        const double alpha_inv = 1.0 - alpha;
        return { std::max(bg.a, fg.a),
                 static_cast<channel>(alpha * fg.r + alpha_inv * bg.r),
                 static_cast<channel>(alpha * fg.g + alpha_inv * bg.g),
                 static_cast<channel>(alpha * fg.b + alpha_inv * bg.b) };
    }

    // Min and max channel color value
    static constexpr channel min = std::numeric_limits<channel>::min();
    static constexpr channel max = std::numeric_limits<channel>::max();
};

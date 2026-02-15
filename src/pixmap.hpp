// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "color.hpp"
#include "geometry.hpp"

#include <cassert>
#include <cstring>
#include <functional>
#include <vector>

struct Pixmap {
    /** Pixmap formats. */
    enum Format : uint8_t { GS, RGB, ARGB };

    /**
     * Create pixmap.
     * @param format pixmap format
     * @param width,height pixel map size
     */
    void create(const Format format, const size_t width, const size_t height);

    /**
     * Attach pixmap to existing buffer.
     * @param format pixmap format
     * @param width,height pixel map size
     * @param data pointer to external data buffer
     * @param stride size of single row in bytes
     */
    void attach(const Format format, const size_t width, const size_t height,
                void* data, const size_t stride = 0);

    /**
     * Free pixmap.
     */
    void free();

    /**
     * Get pixmap width.
     * @return pixmap width in pixels
     */
    inline size_t width() const { return pm_width; }

    /**
     * Get pixmap height.
     * @return pixmap height in pixels
     */
    inline size_t height() const { return pm_height; }

    /**
     * Get stride (size of a sinlow line in bytes).
     * @return stride size in bytes
     */
    inline size_t stride() const { return pm_stride; }

    /**
     * Get pixmap format.
     * @return pixmap format
     */
    inline Format format() const { return pm_format; }

    /**
     * Check if there is data in the pixmap.
     */
    inline operator bool() const { return pm_width && pm_height; }

    /**
     * Cast to Size object.
     * @return Size object
     */
    inline operator Size() const { return { pm_width, pm_height }; }

    /**
     * Create attached submap from current pixmap.
     * @param rect submap area
     * @return attached pixmap
     */
    Pixmap submap(const Rectangle& rect);

    /**
     * Get pinter to the pixel at specified offset.
     * @param x,y pixel position within the pixmap
     * @return pointer to the pixel
     */
    inline void* ptr(const size_t x, const size_t y)
    {
        assert(x < pm_width);
        assert(y < pm_height);
        assert(pm_ext || !pm_data.empty());
        const size_t offset = y * pm_stride + x * pm_bpp;
        return pm_ext ? &pm_ext[offset] : &pm_data[offset];
    }

    /**
     * Get pinter to the pixel at specified offset.
     * @param x,y pixel position within the pixmap
     * @return pointer to the pixel
     */
    inline const void* ptr(const size_t x, const size_t y) const
    {
        assert(x < pm_width);
        assert(y < pm_height);
        assert(pm_ext || !pm_data.empty());
        const size_t offset = y * pm_stride + x * pm_bpp;
        return pm_ext ? &pm_ext[offset] : &pm_data[offset];
    }

    /**
     * Get single pixel at specified position.
     * @param x,y pixel position within the pixmap
     * @return reference to the pixel
     */
    inline argb_t& at(const size_t x, const size_t y)
    {
        assert(format() == Format::RGB || format() == Format::ARGB);
        return *reinterpret_cast<argb_t*>(ptr(x, y));
    }

    /**
     * Get single pixel at specified position.
     * @param x,y pixel position within the pixmap
     * @return const reference to the pixel
     */
    inline const argb_t& at(const size_t x, const size_t y) const
    {
        assert(format() == Format::RGB || format() == Format::ARGB);
        return *reinterpret_cast<const argb_t*>(ptr(x, y));
    }

    /**
     * Flip pixmap vertically.
     */
    void flip_vertical();

    /**
     * Flip pixmap horizontally.
     */
    void flip_horizontal();

    /**
     * Rotate pixmap.
     * @param angle rotation angle (only 90, 180, or 270)
     */
    void rotate(const size_t angle);

    /**
     * Fill area with specified color (ARGB only).
     * @param rect target rectangle area
     * @param color color to set
     */
    void fill(const Rectangle& rect, const argb_t& color);

    /**
     * Fill area with specified color using alpha blending (ARGB only).
     * @param rect target rectangle area
     * @param color color to blend
     */
    void fill_blend(const Rectangle& rect, const argb_t& color);

    /**
     * Draw grid (ARGB only).
     * @param rect target rectangle area
     * @param size tile size
     * @param clr0,clr1 grid colors
     */
    void grid(const Rectangle& rect, const size_t size, const argb_t& clr0,
              const argb_t& clr1);

    /**
     * Draw rectangle (ARGB only).
     * @param rect rectangle coordinates and size
     * @param color color to use
     * @param thickness line thickness, growing inside
     */
    void rectangle(const Rectangle& rect, const argb_t& color,
                   const size_t thickness);

    /**
     * Put masked pixmap.
     * @param pm grayscale pixmap used as mask
     * @param x,y target coordinates
     * @param color applied mask color
     */
    void mask(const Pixmap& pm, const ssize_t x, const ssize_t y,
              const argb_t color);

    /**
     * Copy one pixmap to another.
     * @param pm pixmap for overlay
     * @param x,y target coordinates
     */
    void copy(const Pixmap& pm, const ssize_t x, const ssize_t y);

    /**
     * Put one pixmap on another using alpha blending.
     * @param pm pixmap for overlay
     * @param x,y target coordinates
     */
    void blend(const Pixmap& pm, const ssize_t x, const ssize_t y);

    /**
     * Apply filter to transform the entire pixmap.
     * @param fn filter function
     */
    void foreach(const std::function<void(argb_t&)>& fn);

    /**
     * Convert ABGR to ARGB.
     */
    void abgr_to_argb();

private:
    Format pm_format = ARGB;      ///< Pixmap format
    size_t pm_bpp = 0;            ///< Bytes per pixel
    size_t pm_width = 0;          ///< Width in pixels
    size_t pm_height = 0;         ///< Height in pixels
    size_t pm_stride = 0;         ///< Row size in bytes
    std::vector<uint8_t> pm_data; ///< Pixel data (owned)
    uint8_t* pm_ext = nullptr;    ///< Pixel data (attached)
};

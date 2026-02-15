// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "pixmap.hpp"

void Pixmap::create(const Format format, const size_t width,
                    const size_t height)
{
    pm_format = format;
    pm_width = width;
    pm_height = height;

    switch (pm_format) {
        case Format::ARGB:
        case Format::RGB:
            pm_bpp = sizeof(argb_t);
            break;
        case Format::GS:
            pm_bpp = sizeof(uint8_t);
            break;
    }

    pm_stride = pm_width * pm_bpp;

    pm_ext = nullptr;
    pm_data.resize(pm_height * pm_stride);
}

void Pixmap::attach(const Format format, const size_t width,
                    const size_t height, void* data, const size_t stride)
{
    pm_format = format;
    pm_width = width;
    pm_height = height;

    switch (pm_format) {
        case Format::ARGB:
        case Format::RGB:
            pm_bpp = sizeof(argb_t);
            break;
        case Format::GS:
            pm_bpp = sizeof(uint8_t);
            break;
    }

    pm_stride = stride ? stride : pm_width * pm_bpp;

    pm_ext = reinterpret_cast<uint8_t*>(data);
    pm_data.clear();
}

void Pixmap::free()
{
    pm_width = 0;
    pm_height = 0;
    pm_bpp = 0;
    pm_stride = 0;
    pm_ext = nullptr;
    pm_data.clear();
}

Pixmap Pixmap::submap(const Rectangle& rect)
{
    Pixmap sub;

    const Rectangle full { 0, 0, pm_width, pm_height };
    const Rectangle visible = full.intersect(rect);

    if (visible) {
        sub.attach(pm_format, visible.width, visible.height,
                   ptr(visible.x, visible.y), pm_stride);
    }

    return sub;
}

void Pixmap::flip_vertical()
{
    assert(format() == Format::RGB || format() == Format::ARGB);

    const size_t line_sz = pm_width * pm_bpp;
    std::vector<uint8_t> buffer(line_sz);

    const size_t y_end = pm_height / 2;
    for (size_t y = 0; y < y_end; ++y) {
        void* src = ptr(0, y);
        void* dst = ptr(0, pm_height - y - 1);
        std::memcpy(buffer.data(), dst, line_sz);
        std::memcpy(dst, src, line_sz);
        std::memcpy(src, buffer.data(), line_sz);
    }
}

void Pixmap::flip_horizontal()
{
    assert(format() == Format::RGB || format() == Format::ARGB);

    const size_t x_end = pm_width / 2;
    uint8_t swap[sizeof(argb_t)];

    for (size_t y = 0; y < pm_height; ++y) {
        for (size_t x = 0; x < x_end; ++x) {
            void* left = ptr(x, y);
            void* right = ptr(pm_width - x - 1, y);
            std::memcpy(&swap, left, pm_bpp);
            std::memcpy(left, right, pm_bpp);
            std::memcpy(right, &swap, pm_bpp);
        }
    }
}

void Pixmap::rotate(const size_t angle)
{
    assert(format() == Format::RGB || format() == Format::ARGB);
    assert(angle == 90 || angle == 180 || angle == 270);

    if (angle == 180) {
        const size_t total_pixels = pm_width * pm_height;
        const size_t half_pixels = total_pixels / 2;
        argb_t* pixdata = &at(0, 0);
        for (size_t i = 0; i < half_pixels; ++i) {
            argb_t* color1 = &pixdata[i];
            argb_t* color2 = &pixdata[total_pixels - i - 1];
            const argb_t swap = *color1;
            *color1 = *color2;
            *color2 = swap;
        }
    } else if (angle == 90 || angle == 270) {
        // create temporary buffer with origin data
        Pixmap origin;
        origin.create(pm_format, pm_width, pm_height);
        std::memcpy(origin.ptr(0, 0), ptr(0, 0), pm_height * pm_stride);

        // change orientation
        pm_width = origin.pm_height;
        pm_height = origin.pm_width;
        pm_stride = pm_width * pm_bpp;

        // rotate
        for (size_t y = 0; y < pm_height; ++y) {
            for (size_t x = 0; x < pm_width; ++x) {
                size_t origin_x, origin_y;
                if (angle == 90) {
                    origin_x = y;
                    origin_y = origin.pm_height - 1 - x;
                } else {
                    origin_x = origin.pm_width - 1 - y;
                    origin_y = x;
                }
                at(x, y) = origin.at(origin_x, origin_y);
            }
        }
    }
}

void Pixmap::fill(const Rectangle& rect, const argb_t& color)
{
    assert(format() == Format::RGB || format() == Format::ARGB);

    const Rectangle visible = rect.intersect({ 0, 0, pm_width, pm_height });
    if (!visible) {
        return; // out of pixmap
    }

    // create pattern
    const size_t pattern_sz = visible.width * sizeof(argb_t);
    argb_t* pattern = &at(visible.x, visible.y);
    for (size_t x = 0; x < visible.width; ++x) {
        pattern[x] = color;
    }

    // copy pattern
    const size_t y_end = visible.y + visible.height;
    for (size_t y = visible.y + 1; y < y_end; ++y) {
        std::memcpy(&at(visible.x, y), pattern, pattern_sz);
    }
}

void Pixmap::fill_blend(const Rectangle& rect, const argb_t& color)
{
    assert(format() == Format::ARGB);

    const Rectangle visible = rect.intersect({ 0, 0, pm_width, pm_height });
    if (!visible) {
        return; // out of pixmap
    }

    const size_t x_end = visible.x + visible.width;
    const size_t y_end = visible.y + visible.height;
    for (size_t y = visible.y; y < y_end; ++y) {
        for (size_t x = visible.x; x < x_end; ++x) {
            at(x, y).blend(color);
        }
    }
}

void Pixmap::grid(const Rectangle& rect, const size_t size, const argb_t& clr0,
                  const argb_t& clr1)
{
    assert(format() == Format::ARGB);

    const Rectangle visible = rect.intersect({ 0, 0, pm_width, pm_height });
    if (!visible) {
        return; // out of pixmap
    }

    // fill patterns
    std::vector<argb_t> patterns[2];
    patterns[0].resize(visible.width);
    patterns[1].resize(visible.width);
    for (size_t i = 0; i < visible.width; ++i) {
        const size_t tile = (i / size) % 2;
        patterns[0][i] = tile ? clr0 : clr1;
        patterns[1][i] = tile ? clr1 : clr0;
    }

    // copy patterns
    const size_t pattern_sz = visible.width * sizeof(argb_t);
    for (size_t y = 0; y < visible.height; ++y) {
        const size_t shift = (y / size) % 2;
        void* line = ptr(visible.x, visible.y + y);
        std::memcpy(line, patterns[shift].data(), pattern_sz);
    }
}

void Pixmap::rectangle(const Rectangle& rect, const argb_t& color,
                       const size_t thickness)
{
    assert(format() == Format::ARGB);

    Rectangle line;

    // top
    line = rect;
    line.height = thickness;
    fill(line, color);

    // bottom
    line.y = rect.y + rect.height - thickness;
    fill(line, color);

    const ssize_t fill_v = static_cast<ssize_t>(rect.height) - thickness * 2;
    if (fill_v > 0) {
        line.y = rect.y + thickness;
        line.width = thickness;
        line.height = fill_v;

        // left
        line.x = rect.x;
        fill(line, color);

        // right
        line.x = rect.x + rect.width - thickness;
        fill(line, color);
    }
}

void Pixmap::mask(const Pixmap& pm, const ssize_t x, const ssize_t y,
                  const argb_t color)
{
    assert(format() == Format::RGB || format() == Format::ARGB);
    assert(pm.format() == Format::GS);

    const Rectangle image { x, y, pm.pm_width, pm.pm_height };
    const Rectangle visible = image.intersect({ 0, 0, pm_width, pm_height });
    if (!visible) {
        return; // out of pixmap
    }

    const size_t y_end = std::min(pm.height(), visible.height);
    const size_t x_end = std::min(pm.width(), visible.width);

    argb_t clr { 0, color.r, color.g, color.b };

    for (size_t y = 0; y < y_end; ++y) {
        for (size_t x = 0; x < x_end; ++x) {
            const uint8_t a = *reinterpret_cast<const uint8_t*>(pm.ptr(x, y));
            const argb_t over { a, color.r, color.g, color.b };
            at(x + visible.x, y + visible.y).blend(over);
        }
    }
}

void Pixmap::copy(const Pixmap& pm, const ssize_t x, const ssize_t y)
{
    assert(format() == Format::RGB || format() == Format::ARGB);
    assert(pm.format() == Format::RGB || pm.format() == Format::ARGB);

    const Rectangle image { x, y, pm.pm_width, pm.pm_height };
    const Rectangle visible = image.intersect({ 0, 0, pm_width, pm_height });
    if (!visible) {
        return; // out of pixmap
    }

    const size_t diff_x = visible.x - image.x;
    const size_t diff_y = visible.y - image.y;

    const size_t line_sz = visible.width * pm_bpp;
    for (size_t y = 0; y < visible.height; ++y) {
        const void* src = pm.ptr(diff_x, diff_y + y);
        void* dst = ptr(visible.x, visible.y + y);
        std::memcpy(dst, src, line_sz);
    }
}

void Pixmap::blend(const Pixmap& pm, const ssize_t x, const ssize_t y)
{
    assert(format() == Format::RGB || format() == Format::ARGB);
    assert(pm.format() == Format::RGB || pm.format() == Format::ARGB);
    assert(format() == Format::ARGB || pm.format() == Format::ARGB);

    const Rectangle image { x, y, pm.pm_width, pm.pm_height };
    const Rectangle visible = image.intersect({ 0, 0, pm_width, pm_height });
    if (!visible) {
        return; // out of pixmap
    }

    const size_t diff_x = visible.x - image.x;
    const size_t diff_y = visible.y - image.y;

    for (size_t y = 0; y < visible.height; ++y) {
        for (size_t x = 0; x < visible.width; ++x) {
            argb_t& pixel = at(visible.x + x, visible.y + y);
            pixel.blend(pm.at(diff_x + x, diff_y + y));
        }
    }
}

void Pixmap::foreach(const std::function<void(argb_t&)>& fn)
{
    assert(format() == Format::RGB || format() == Format::ARGB);

    for (size_t y = 0; y < pm_height; ++y) {
        for (size_t x = 0; x < pm_width; ++x) {
            fn(at(x, y));
        }
    }
}

void Pixmap::abgr_to_argb()
{
    foreach([](argb_t& color) {
        color.b = color.b ^ color.r;
        color.r = color.b ^ color.r;
        color.b = color.b ^ color.r;
    });
}

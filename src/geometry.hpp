// SPDX-License-Identifier: MIT
// Operations with rectangle.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <unistd.h>

#include <algorithm>
#include <limits>

/** Coordinates in 2D. */
struct Point {
    ssize_t x = invalid;
    ssize_t y = invalid;

    /**
     * Check if coordinates is valid.
     * @return true if coordinates is valid, false otherwise
     */
    inline operator bool() const { return x != invalid && y != invalid; }

    // Invalid position
    static constexpr ssize_t invalid = std::numeric_limits<ssize_t>::min();
};

/** Object size. */
struct Size {
    size_t width = invalid;
    size_t height = invalid;

    /**
     * Check if size is valid.
     * @return true if size is valid, false otherwise
     */
    inline operator bool() const
    {
        return width != invalid && height != invalid;
    }

    // Invalid size
    static constexpr size_t invalid = std::numeric_limits<size_t>::min();
};

/** Rectangle: position and size. */
struct Rectangle : public Point, public Size {
    Rectangle() = default;

    /**
     * Constructor.
     * @param x,y top left coordinates of rectangle
     * @param width,height rectangle size
     */
    Rectangle(const ssize_t x, const ssize_t y, const size_t width,
              const size_t height)
        : Point(x, y)
        , Size(width, height)
    {
    }

    /**
     * Check if rectangle is valid.
     * @return true if rectangle is valid, false otherwise
     */
    inline operator bool() const
    {
        return Point::operator bool() && Size::operator bool();
    }

    /**
     * Get intersection of two rectangles.
     * @param other rectangle for intersection calculation
     * @return intersection description
     */
    Rectangle intersect(const Rectangle& other) const
    {
        const ssize_t x1 = std::max(x, other.x);
        const ssize_t y1 = std::max(y, other.y);
        const ssize_t x2 = std::min(x + width, other.x + other.width);
        const ssize_t y2 = std::min(y + height, other.y + other.height);

        if (x2 <= x1 || y2 <= y1) {
            return {};
        }
        return { x1, y1, static_cast<size_t>(x2 - x1),
                 static_cast<size_t>(y2 - y1) };
    }
};

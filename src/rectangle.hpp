// SPDX-License-Identifier: MIT
// Operations with rectangle.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <unistd.h>

#include <algorithm>
#include <limits>

/** Rectangle: position and size. */
struct Rectangle {
    /**
     * Check if position is valid.
     * @return true if position is valid, false otherwise
     */
    inline bool has_pos() const { return x != bad_pos && y != bad_pos; }

    /**
     * Check if size is valid.
     * @return true if size is valid, false otherwise
     */
    inline bool has_size() const
    {
        return width != bad_size && height != bad_size;
    }

    /**
     * Check if rectangle is valid.
     * @return true if rectangle is valid, false otherwise
     */
    inline operator bool() const { return has_pos() && has_size(); }

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

    ssize_t x = bad_pos;      ///< Left coordinate of rectangle
    ssize_t y = bad_pos;      ///< Top coordinate of rectangle
    size_t width = bad_size;  ///< Width size
    size_t height = bad_size; ///< Height size

    // Invalid position and size
    static constexpr ssize_t bad_pos = std::numeric_limits<ssize_t>::min();
    static constexpr size_t bad_size = std::numeric_limits<size_t>::min();
};

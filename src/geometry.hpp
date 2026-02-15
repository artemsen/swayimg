// SPDX-License-Identifier: MIT
// Geometric primitives.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <unistd.h>

#include <limits>
#include <tuple>

/** Coordinates in 2D. */
struct Point {
    ssize_t x = invalid;
    ssize_t y = invalid;

    /**
     * Check if coordinates is valid.
     * @return true if coordinates is valid, false otherwise
     */
    inline operator bool() const { return x != invalid && y != invalid; }

    /**
     * Shift coordinates.
     */
    Point operator+(const Point& delta) const;

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

    /**
     * Scale size.
     */
    Size operator*(double factor) const;

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
              const size_t height);

    /**
     * Constructor.
     * @param pos top left coordinates of rectangle
     * @param size rectangle size
     */
    Rectangle(const Point& pos, const Size& size);

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
    Rectangle intersect(const Rectangle& other) const;

    /**
     * Cut out area from rectangle.
     * @param cut rectangle to cut out
     * @return tuple with rest parts: top, bottom, left, right
     */
    std::tuple<Rectangle, Rectangle, Rectangle, Rectangle>
    cutout(const Rectangle& cut) const;
};

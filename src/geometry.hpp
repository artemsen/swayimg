// SPDX-License-Identifier: MIT
// Geometric primitives.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <unistd.h>

#include <limits>
#include <tuple>

/** Coordinates in 2D. */
struct Point {
    ssize_t x = npos;
    ssize_t y = npos;

    /**
     * Check if coordinates is valid.
     * @return true if coordinates is valid, false otherwise
     */
    inline operator bool() const { return x != npos && y != npos; }

    /**
     * Shift coordinates.
     */
    Point operator+(const Point& delta) const;

    /**
     * Get delta (diff) between two points.
     */
    Point operator-(const Point& other) const;

    // Invalid position
    static constexpr ssize_t npos = std::numeric_limits<ssize_t>::min();
};

/** Object size. */
struct Size {
    size_t width = 0;
    size_t height = 0;

    /**
     * Check if size is valid.
     * @return true if size is valid, false otherwise
     */
    inline operator bool() const { return width > 0 && height > 0; }

    /**
     * Scale size.
     */
    Size operator*(double factor) const;
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

// SPDX-License-Identifier: MIT
// Geometric primitives.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "geometry.hpp"

#include <algorithm>

Point Point::operator+(const Point& delta) const
{
    return { x + delta.x, y + delta.y };
}

Size Size::operator*(double factor) const
{
    return { static_cast<size_t>(factor * width),
             static_cast<size_t>(factor * height) };
}

Rectangle::Rectangle(const ssize_t x, const ssize_t y, const size_t width,
                     const size_t height)
    : Point(x, y)
    , Size(width, height)
{
}

Rectangle::Rectangle(const Point& pos, const Size& size)
    : Point(pos)
    , Size(size)
{
}

Rectangle Rectangle::intersect(const Rectangle& other) const
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

std::tuple<Rectangle, Rectangle, Rectangle, Rectangle>
Rectangle::cutout(const Rectangle& cut) const
{
    Rectangle top, bottom, left, right;

    // top
    if (cut.y > y) {
        top.x = x;
        top.y = y;
        top.height = cut.y - y;
        top.width = width;
    }

    // bottom
    if (y + cut.y + cut.height < height) {
        bottom.x = x;
        bottom.y = cut.y + cut.height;
        bottom.width = width;
        bottom.height = height - bottom.y;
    }

    // left
    if (cut.x > x) {
        left.x = x;
        left.y = cut.y;
        left.width = cut.x - x;
        left.height = cut.height;
    }

    // right
    if (x + cut.x + cut.width < width) {
        right.x = cut.x + cut.width;
        right.y = cut.y;
        right.width = width - cut.x - cut.width;
        right.height = cut.height;
    }

    return std::make_tuple(top, bottom, left, right);
}

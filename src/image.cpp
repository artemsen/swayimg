// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.hpp"

#include "render.hpp"

#include <cassert>
#include <limits>

// Image entry index used for removed entries.
constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

void ImageEntry::remove()
{
    index = INVALID_INDEX;
}

ImageEntry::operator bool() const
{
    return index != INVALID_INDEX;
}

void Image::fix_orientation()
{
    const auto& it = meta.find("Exif.Image.Orientation");
    if (it != meta.end()) {
        const int orientation = std::atoi(it->second.c_str());
        switch (orientation) {
            case 2: // flipped back-to-front
                flip_horizontal();
                break;
            case 3: // upside down
                rotate(180);
                break;
            case 4: // flipped back-to-front and upside down
                flip_vertical();
                break;
            case 5: // flipped back-to-front and on its side
                flip_horizontal();
                rotate(90);
                break;
            case 6: // on its side
                rotate(90);
                break;
            case 7: // flipped back-to-front and on its far side
                flip_vertical();
                rotate(270);
                break;
            case 8: // on its far side
                rotate(270);
                break;
        }
    }
}

void Image::draw(const size_t frame, Pixmap& target, const double scale,
                 const ssize_t x, const ssize_t y)
{
    assert(frame < frames.size());
    Render::self().draw(target, frames[frame].pm, { x, y }, scale);
}

void Image::flip_vertical()
{
    for (auto& it : frames) {
        it.pm.flip_vertical();
    }
}

void Image::flip_horizontal()
{
    for (auto& it : frames) {
        it.pm.flip_horizontal();
    }
}

void Image::rotate(size_t angle)
{
    for (auto& it : frames) {
        it.pm.rotate(angle);
    }
}

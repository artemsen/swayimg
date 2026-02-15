// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.hpp"

#include "render.hpp"

#include <cassert>

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

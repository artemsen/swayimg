// SPDX-License-Identifier: MIT
// Thumbnail layout for gallery mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "layout.hpp"

#include "imagelist.hpp"

#include <cassert>

void Layout::update()
{
    if (!window) {
        return; // not initialized
    }

    ImageList& il = ImageList::self();
    ImageEntryPtr first_image = il.get(nullptr, ImageList::Dir::First);

    if (!sel_img) {
        sel_img = first_image;
    }

    columns = std::max(static_cast<size_t>(1),
                       window.width / (thumb_size + thumb_padding));
    rows = std::max(static_cast<size_t>(1),
                    window.height / (thumb_size + thumb_padding));

    // set preliminary position for the currently selected image
    ssize_t distance = il.distance(first_image, sel_img);
    sel_col = distance % columns;
    if (sel_row == rows) {
        sel_row = rows - 1;
    } else if (sel_row > rows) {
        sel_row = rows / 2;
    }

    // get the first visible image
    distance = sel_row * columns + sel_col;
    ImageEntryPtr first_visible = il.get(sel_img, -distance);
    if (!first_visible) {
        first_visible = first_image;
    }

    // get the last visible images
    const size_t max_thumb = rows * columns;
    ImageEntryPtr last_visible = il.get(first_visible, max_thumb - 1);
    if (!last_visible) {
        ImageEntryPtr last_image = il.get(nullptr, ImageList::Dir::Last);
        last_visible = last_image;
        if (first_visible != first_image) {
            // scroll to fill the entire window
            const size_t last_col = (il.size() - 1) % columns;
            distance = max_thumb - (columns - last_col);
            first_visible = il.get(last_visible, -distance);
            if (!first_visible) {
                first_visible = first_image;
            }
        }
    }

    sel_row = il.distance(first_visible, sel_img) / columns;

    assert(first_visible);
    assert(last_visible);

    // recalculate layout
    const size_t total_visible = il.distance(first_visible, last_visible) + 1;
    const size_t used_cols = std::min(columns, total_visible);
    const size_t used_rows = (total_visible + columns - 1) / columns;
    const size_t mid_x =
        std::min(window.width, used_cols * (thumb_size + thumb_padding));
    const size_t mid_y =
        std::min(window.height, used_rows * (thumb_size + thumb_padding));
    const size_t offset_x = (window.width - mid_x) / 2;
    const size_t offset_y = (window.height - mid_y) / 2;

    // fill thumbnails map
    scheme.clear();
    ImageEntryPtr img = first_visible;
    for (size_t i = 0; i < total_visible; ++i) {
        Thumbnail thumb;
        thumb.col = i % columns;
        thumb.row = i / columns;
        thumb.pos.x = offset_x + thumb.col * thumb_size;
        thumb.pos.x += thumb_padding * (thumb.col + 1);
        thumb.pos.y = offset_y + thumb.row * thumb_size;
        thumb.pos.y += thumb_padding * (thumb.row + 1);
        thumb.img = img;
        scheme.emplace_back(thumb);
        img = il.get(img, ImageList::Dir::Next);
    }

    assert(sel_img == scheme[sel_row * columns + sel_col].img);
}

void Layout::set_window_size(const Size& size)
{
    window = size;
    update();
}

void Layout::set_thumb_size(const size_t size)
{
    thumb_size = size;
    update();
}

void Layout::set_padding(const size_t padding)
{
    thumb_padding = padding;
    update();
}

void Layout::select(const ImageEntryPtr& image)
{
    sel_img = image;
    sel_row = std::numeric_limits<size_t>::max();
    update();
}

bool Layout::select(const Direction dir)
{
    assert(sel_img);

    ImageList& il = ImageList::self();
    ImageEntryPtr next = nullptr;
    ssize_t col = sel_col;
    ssize_t row = sel_row;

    switch (dir) {
        case Direction::First:
            next = il.get(nullptr, ImageList::Dir::First);
            row = 0;
            break;
        case Direction::Last:
            next = il.get(nullptr, ImageList::Dir::Last);
            row = 0;
            break;
        case Direction::Up:
            next = il.get(sel_img, -static_cast<ssize_t>(columns));
            if (!next) {
                next = il.get(nullptr, ImageList::Dir::First);
            }
            --row;
            break;
        case Direction::Down:
            next = il.get(sel_img, columns);
            if (!next) {
                next = il.get(nullptr, ImageList::Dir::Last);
            }
            ++row;
            break;
        case Direction::Left:
            next = il.get(sel_img, ImageList::Dir::Prev);
            --col;
            break;
        case Direction::Right:
            next = il.get(sel_img, ImageList::Dir::Next);
            ++col;
            break;
        case Direction::PgUp:
            next = il.get(sel_img, -static_cast<ssize_t>(columns * rows));
            if (!next) {
                next = il.get(nullptr, ImageList::Dir::First);
            }
            break;
        case Direction::PgDown:
            next = il.get(sel_img, columns * rows);
            if (!next) {
                next = il.get(nullptr, ImageList::Dir::Last);
            }
            break;
    }

    if (next == sel_img) {
        next = nullptr;
    }

    if (next) {
        if (col < 0) {
            --row;
        } else if (col >= static_cast<ssize_t>(columns)) {
            ++row;
        }
        if (row < 0) {
            sel_row = 0;
        } else {
            sel_row = row;
        }
        sel_img = next;
        update();
    }

    return !!next;
}

bool Layout::select(const Point& pos)
{
    const Thumbnail* thumb = at(pos);
    const bool new_select = thumb && thumb->img != sel_img;

    if (new_select) {
        sel_img = thumb->img;
        sel_col = thumb->col;
        sel_row = thumb->row;
    }

    return new_select;
}

const Layout::Thumbnail* Layout::at(const Point& pos) const
{
    for (const auto& it : scheme) {
        if (pos.x >= it.pos.x &&
            pos.x < it.pos.x + static_cast<ssize_t>(thumb_size) &&
            pos.y >= it.pos.y &&
            pos.y < it.pos.y + static_cast<ssize_t>(thumb_size)) {
            return &it;
        }
    }
    return nullptr;
}

ImageEntryPtr Layout::get_selected() const
{
    return sel_img;
}

const std::vector<Layout::Thumbnail>& Layout::get_scheme() const
{
    return scheme;
}

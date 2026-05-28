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

bool ImageEntry::is_special(const std::string& path)
{
    return path.starts_with(ImageEntry::SRC_STDIN) ||
        path.starts_with(ImageEntry::SRC_EXEC);
}

ImageEntryPtr ImageEntry::from_special(const std::filesystem::path& path)
{
    assert(ImageEntry::is_special(path));

    ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = path;
    entry->mtime = 0;
    entry->size = 0;
    entry->index = 0;
    return entry;
}

ImageEntryPtr ImageEntry::from_file(const std::filesystem::path& path)
{
    assert(path.is_absolute());
    assert(std::filesystem::is_regular_file(path));

    const auto fs_time = std::filesystem::last_write_time(path);
    auto sys_time =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fs_time - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    const std::time_t tt_time = std::chrono::system_clock::to_time_t(sys_time);

    ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = path;
    entry->mtime = tt_time;
    entry->size = std::filesystem::file_size(path);
    entry->index = 0;
    return entry;
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

void Image::rotate(const size_t angle)
{
    for (auto& it : frames) {
        it.pm.rotate(angle);
    }
}

// SPDX-License-Identifier: MIT
// Text overlay.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "text.hpp"

#include "application.hpp"

#include <ctime>
#include <format>

Text::Text()
{
    blocks[static_cast<uint8_t>(Position::TopLeft)].data = {
        { Line("File:"),        Line("{name}")                             },
        { Line("Format:"),      Line("{format}")                           },
        { Line("File size:"),   Line("{size}")                             },
        { Line("File time:"),   Line("{time}")                             },
        { Line("EXIF date:"),   Line("{meta.Exif.Photo.DateTimeOriginal}") },
        { Line("EXIF camera:"), Line("{meta.Exif.Image.Model}")            },
    };
    blocks[static_cast<uint8_t>(Position::TopRight)].data = {
        { Line("Image:"), Line("{list.index} of {list.total}")   },
        { Line("Frame:"), Line("{frame.index} of {frame.total}") },
        { Line("Size:"),  Line("{frame.width}x{frame.height}")   },
    };
    blocks[static_cast<uint8_t>(Position::BottomLeft)].data = {
        { Line("Scale:"), Line("{scale}") },
    };
    blocks[static_cast<uint8_t>(Position::BottomRight)].data = {
        { Line(""), Line("{status}") },
    };
}

void Text::set_scheme(const Position pos,
                      const std::vector<std::string>& scheme)
{
    Block& block = blocks[static_cast<uint8_t>(pos)];
    block.data.clear();
    block.data.reserve(scheme.size());

    for (auto& line : scheme) {
        std::string key, value;
        const size_t delim = line.find(':');
        if (delim == std::string::npos) {
            value = line;
        } else {
            key = line.substr(0, delim);
            value = line.substr(delim + 1);
        }
        block.data.push_back({ Line(key), Line(value) });
    }
}

void Text::reset(const ImagePtr& image)
{
    fields.clear();

    set_field(FIELD_FILE_PATH, image->entry->path);
    set_field(FIELD_FILE_DIR, image->entry->path.parent_path().filename());
    set_field(FIELD_FILE_NAME, image->entry->path.filename());
    set_field(FIELD_IMAGE_FORMAT, image->format);
    set_field(FIELD_FRAME_WIDTH, std::to_string(image->frames[0].pm.width()));
    set_field(FIELD_FRAME_HEIGHT, std::to_string(image->frames[0].pm.height()));
    set_field(FIELD_FRAME_INDEX, "1");
    set_field(FIELD_FRAME_TOTAL, std::to_string(image->frames.size()));

    set_field(FIELD_LIST_INDEX, std::to_string(image->entry->index));
    set_field(FIELD_LIST_TOTAL,
              std::to_string(Application::get_imagelist().size()));
    set_field(FIELD_SCALE, "");

    // human readable file size
    const size_t mib = 1024 * 1024;
    set_field(FIELD_FILE_SIZE,
              std::format("{:.02f} {}iB",
                          static_cast<float>(image->entry->size) /
                              (image->entry->size >= mib ? mib : 1024),
                          image->entry->size >= mib ? 'M' : 'K'));

    // human readable file modification time
    const std::tm* local_tm = localtime(&image->entry->mtime);
    char time_buff[32];
    std::strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %H:%M:%S", local_tm);
    set_field(FIELD_FILE_TIME, time_buff);

    // import meta info
    for (const auto& [key, value] : image->meta) {
        const std::string name = std::string(FIELD_META) + "." + key;
        set_field(name, value);
    }
}

void Text::set_field(const std::string& field, const std::string& value)
{
    fields.insert_or_assign(field, value);
}

void Text::update()
{
    for (auto& blk : blocks) {
        for (auto& kv : blk.data) {
            kv.key.update(fields);
            kv.value.update(fields);
        }
    }
}

void Text::Line::update(const std::map<std::string, std::string>& fields)
{
    std::string output = scheme;

    size_t br_open = output.find('{');
    while (br_open != std::string::npos) {
        const size_t br_close = output.find('}', br_open + 1);
        if (br_close == std::string::npos) {
            break;
        }

        const size_t len = br_close - br_open;
        const std::string name = output.substr(br_open + 1, len - 1);

        const auto it = fields.find(name);
        if (it != fields.end()) {
            output.replace(br_open, len + 1, it->second);
            br_open += it->second.length();
        } else {
            output.erase(br_open, len + 1);
        }

        br_open = output.find('{', br_open);
    }

    if (output != display) {
        display = output;
        if (display.empty()) {
            pm.free();
        } else {
            pm = Application::get_font().render(display);
        }
    }
}

void Text::refresh()
{
    Font& font = Application::get_font();

    for (auto& blk : blocks) {
        for (auto& [key, value] : blk.data) {
            if (!key.display.empty()) {
                key.pm = font.render(key.display);
            }
            if (!value.display.empty()) {
                value.pm = font.render(value.display);
            }
        }
    }
}

void Text::draw(Pixmap& target) const
{
    for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i) {
        const Position pos = static_cast<Position>(i);
        draw(blocks[i], pos, target);
    }
}

void Text::draw(const Block& block, const Position pos, Pixmap& target) const
{
    // get block size in pixels
    const auto [key_width, val_width] = block.width();
    const size_t total_width = key_width + val_width;
    const auto [single_height, total_height] = block.height();

    // calculate initial position
    ssize_t x = 0;
    ssize_t y = 0;
    switch (pos) {
        case Position::TopLeft:
            x = padding;
            y = padding;
            break;
        case Position::TopRight:
            x = static_cast<ssize_t>(target.width()) - total_width - padding;
            y = padding;
            break;
        case Position::BottomLeft:
            x = padding;
            y = static_cast<ssize_t>(target.height()) - total_height - padding;
            break;
        case Position::BottomRight:
            x = static_cast<ssize_t>(target.width()) - total_width - padding;
            y = static_cast<ssize_t>(target.height()) - total_height - padding;
            break;
    }
    x = std::max(static_cast<ssize_t>(0), x);
    y = std::max(static_cast<ssize_t>(0), y);

    // draw background
    if (background.a != argb_t::min) {
        target.fill_blend({ x, y, total_width, total_height }, background);
    }

    for (const auto& [key, value] : block.data) {
        if (!value.pm) {
            continue; // skip empty lines
        }

        // draw shadow
        if (shadow.a != argb_t::min) {
            const size_t offset =
                std::max(single_height / 24, static_cast<size_t>(1));
            if (key.pm) {
                target.mask(key.pm, x + offset, y + offset, shadow);
            }
            if (value.pm) {
                target.mask(value.pm, x + key_width + offset, y + offset,
                            shadow);
            }
        }

        // draw text with foreground color
        if (key.pm) {
            target.mask(key.pm, x, y, foreground);
        }
        if (value.pm) {
            target.mask(value.pm, x + key_width, y, foreground);
        }

        y += single_height;
    }
}

std::tuple<size_t, size_t> Text::Block::width() const
{
    size_t key_width = 0;
    size_t val_width = 0;

    for (const auto& [key, value] : data) {
        if (value.pm) {
            key_width = std::max(key_width, key.pm.width());
            val_width = std::max(val_width, value.pm.width());
        }
    }

    return std::make_tuple(key_width, val_width);
}

std::tuple<size_t, size_t> Text::Block::height() const
{
    size_t visible_lines = 0;
    size_t single_height = 0;
    for (const auto& [key, value] : data) {
        if (value.pm) {
            ++visible_lines;
            if (!single_height) {
                single_height = value.pm.height();
            }
        }
    }

    return std::make_tuple(single_height, single_height * visible_lines);
}

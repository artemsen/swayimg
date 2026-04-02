// SPDX-License-Identifier: MIT
// Text overlay.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "text.hpp"

#include "application.hpp"
#include "imagelist.hpp"

#include <algorithm>
#include <ctime>
#include <format>

Text& Text::self()
{
    static Text singleton;
    return singleton;
}

Text::Text()
{
    // default settings

    enable = true;

    overall_tm.delay = 5000;
    overall_tm.show = true;

    status_tm.delay = 3000;
    status_tm.show = true;

    spacing = 0;
    padding = 10;

    foreground = { 0xff, 0xcc, 0xcc, 0xcc };
    background = { 0x00, 0x00, 0x00, 0x00 };
    shadow = { 0xd0, 0x00, 0x00, 0x00 };
}

void Text::initialize()
{
    Application::self().add_fdpoll(overall_tm.fd, [this]() {
        overall_tm.fd.reset(0, 0);
        overall_tm.show = false;
        Application::redraw();
    });
    Application::self().add_fdpoll(status_tm.fd, [this]() {
        status.clear();
        status_tm.fd.reset(0, 0);
        status_tm.show = false;
        Application::redraw();
    });
}

void Text::set_scheme(const Position pos, const Scheme& scheme)
{
    Block& block = blocks[static_cast<size_t>(pos)];
    block.clear();
    block.reserve(scheme.size());

    for (auto& line : scheme) {
        std::string key, value;
        const size_t delim = line.find('\t');
        if (delim == std::string::npos) {
            value = line;
        } else {
            key = line.substr(0, delim);
            value = line.substr(delim + 1);
        }
        block.emplace_back(Line(std::move(key)), Line(std::move(value)));
    }
}

void Text::set_font(const std::string& name)
{
    if (font.load(name)) {
        refresh();
    }
}

void Text::set_size(const size_t size)
{
    font.set_size(size);
    refresh();
}

void Text::set_spacing(const ssize_t size)
{
    spacing = size;
    Application::redraw();
}

void Text::set_scale(const double scale)
{
    font.set_scale(scale);
    refresh();
}

void Text::set_padding(const size_t pad)
{
    padding = pad;
    Application::redraw();
}

void Text::set_foreground(const argb_t& color)
{
    foreground = color;
    Application::redraw();
}

void Text::set_background(const argb_t& color)
{
    background = color;
    Application::redraw();
}

void Text::set_shadow(const argb_t& color)
{
    shadow = color;
    Application::redraw();
}

void Text::set_overall_timer(const size_t timeout)
{
    overall_tm.show = true;
    overall_tm.delay = timeout;
    overall_tm.fd.reset(overall_tm.delay, 0);
    Application::redraw();
}

void Text::set_status_timer(const size_t timeout)
{
    status_tm.delay = timeout;
}

void Text::show()
{
    enable = true;
    overall_tm.show = true;
    overall_tm.fd.reset(overall_tm.delay, 0);
    Application::redraw();
}

void Text::hide()
{
    enable = false;
    overall_tm.show = false;
    overall_tm.fd.reset(0, 0);
    Application::redraw();
}

void Text::set_status(const std::string& msg)
{
    status.clear();

    size_t last = 0;
    size_t next = 0;
    while ((next = msg.find('\n', last)) != std::string::npos) {
        status.emplace_back(font.render(msg.substr(last, next - last)));
        last = next + 1;
    }
    status.emplace_back(font.render(msg.substr(last)));

    status_tm.show = true;
    status_tm.fd.reset(status_tm.delay, 0);

    Application::redraw();
}

void Text::reset(const ImagePtr& image)
{
    reset(image->entry);

    set_field(FIELD_IMAGE_FORMAT, image->format);
    set_field(FIELD_FRAME_TOTAL, std::to_string(image->frames.size()));

    // import meta info
    for (const auto& [key, value] : image->meta) {
        const std::string name = std::string(FIELD_META) + "." + key;
        set_field(name, value);
    }

    update();
}

void Text::reset(const ImageEntryPtr& entry)
{
    fields.clear();

    set_field(FIELD_FILE_PATH, entry->path);
    set_field(FIELD_FILE_DIR, entry->path.parent_path().filename());
    set_field(FIELD_FILE_NAME, entry->path.filename());
    set_field(FIELD_FRAME_WIDTH, {});
    set_field(FIELD_FRAME_HEIGHT, {});
    set_field(FIELD_FRAME_INDEX, {});
    set_field(FIELD_FRAME_TOTAL, {});
    set_field(FIELD_FILE_SIZE, std::to_string(entry->size));
    set_field(FIELD_LIST_INDEX, std::to_string(entry->index));
    set_field(FIELD_LIST_TOTAL, std::to_string(ImageList::self().size()));
    set_field(FIELD_SCALE, {});

    // human readable file size
    const size_t mib = 1024 * 1024;
    set_field(FIELD_FILE_SIZE_HR,
              std::format("{:.02f} {}iB",
                          static_cast<float>(entry->size) /
                              (entry->size >= mib ? mib : 1024),
                          entry->size >= mib ? 'M' : 'K'));

    // human readable file modification time
    const std::tm* local_tm = localtime(&entry->mtime);
    char time_buff[32];
    std::strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %H:%M:%S", local_tm);
    set_field(FIELD_FILE_TIME, time_buff);

    // restart timer
    if (enable && overall_tm.delay) {
        overall_tm.show = true;
        overall_tm.fd.reset(overall_tm.delay, 0);
    }

    update();
}

void Text::set_field(const std::string& field, const std::string& value)
{
    fields.insert_or_assign(field, value);
}

void Text::update()
{
    for (auto& block : blocks) {
        for (auto& kv : block) {
            kv.key.update(font, fields);
            kv.value.update(font, fields);
        }
    }
}

void Text::draw(Pixmap& target) const
{
    // show status message
    if (status_tm.show && !status.empty()) {
        // calculate total height
        const ssize_t lspacing =
            std::clamp(spacing, -static_cast<ssize_t>(status.front().height()),
                       static_cast<ssize_t>(status.front().height()));
        const size_t height = status.front().height() * status.size() +
            lspacing * (status.size() - 1);
        // draw status text
        Point pos(0, target.height() - height - padding);
        for (const auto& line : status) {
            pos.x = target.width() / 2 - line.width() / 2;
            draw(line, target, pos);
            pos.y += line.height() + lspacing;
        }
    }

    // show text layer
    if (overall_tm.show) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            draw(static_cast<Position>(i), target);
        }
    }
}

void Text::refresh()
{
    for (auto& block : blocks) {
        for (auto& [key, value] : block) {
            if (!key.display.empty()) {
                key.pm = font.render(key.display);
            }
            if (!value.display.empty()) {
                value.pm = font.render(value.display);
            }
        }
    }
    Application::redraw();
}

Text::Dimension Text::get_dimension(const Block& block) const
{
    Dimension dim {};

    size_t visible_lines = 0;
    for (const auto& [key, value] : block) {
        if (value.pm) {
            ++visible_lines;
            dim.key_width = std::max(dim.key_width, key.pm.width());
            dim.val_width = std::max(dim.val_width, value.pm.width());
            if (!dim.line_height) {
                dim.line_height = value.pm.height();
            }
        }
    }

    if (visible_lines) {
        dim.line_spacing =
            std::clamp(spacing, -static_cast<ssize_t>(dim.line_height),
                       static_cast<ssize_t>(dim.line_height));
        dim.total_width = dim.key_width + dim.val_width;
        dim.total_height = dim.line_height * visible_lines;
        dim.total_height += dim.line_spacing * (visible_lines - 1);
    }

    return dim;
}

void Text::draw(const Position pos, Pixmap& target) const
{
    const Block& block = blocks[static_cast<size_t>(pos)];
    const Dimension dim = get_dimension(block);

    // calculate initial position
    ssize_t x = 0;
    ssize_t y = 0;
    switch (pos) {
        case Position::TopLeft:
            x = padding;
            y = padding;
            break;
        case Position::TopRight:
            x = static_cast<ssize_t>(target.width()) - dim.total_width -
                padding;
            y = padding;
            break;
        case Position::BottomLeft:
            x = padding;
            y = static_cast<ssize_t>(target.height()) - dim.total_height -
                padding;
            break;
        case Position::BottomRight:
            x = static_cast<ssize_t>(target.width()) - dim.total_width -
                padding;
            y = static_cast<ssize_t>(target.height()) - dim.total_height -
                padding;
            break;
    }
    x = std::max(static_cast<ssize_t>(0), x);
    y = std::max(static_cast<ssize_t>(0), y);

    // draw background
    if (background.a != argb_t::min) {
        target.fill_blend({ x, y, dim.total_width, dim.total_height },
                          background);
    }

    for (const auto& [key, value] : block) {
        if (!value.pm) {
            continue; // skip empty lines
        }

        Point tpos { x, y };
        if (key.pm) {
            draw(key.pm, target, tpos);
        }
        if (value.pm) {
            tpos.x += dim.key_width;
            draw(value.pm, target, tpos);
        }

        y += dim.line_height;
        y += dim.line_spacing;
    }
}

void Text::draw(const Pixmap& text, Pixmap& target, const Point& pos) const
{
    // draw shadow
    if (shadow.a != argb_t::min) {
        const size_t offset =
            std::max(text.height() / 24, static_cast<size_t>(1));
        target.mask(text, pos + Point(offset, offset), shadow);
    }
    // draw text with foreground color
    target.mask(text, pos, foreground);
}

void Text::Line::update(Font& font,
                        const std::map<std::string, std::string>& fields)
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
            pm = font.render(display);
        }
    }
}

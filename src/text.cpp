// SPDX-License-Identifier: MIT
// Text overlay.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "text.hpp"

#include "application.hpp"
#include "defaults.hpp"
#include "font.hpp"
#include "imagelist.hpp"

#include <algorithm>
#include <ctime>
#include <format>

namespace {

/** Max length of text line (characters). */
constexpr size_t MAX_TEXT_LEN = 120;
/** Character used for unsupported chars with broken locale. */
constexpr wchar_t FALLBACK_CHR = L'?';
/** Text shadow offset factor. */
constexpr size_t SHADOW_FACTOR = 24;
/** Factor used to calculate horizontal margin for text line. */
constexpr size_t MARGIN_FACTOR = 6;

/** Global font instance. */
Font font;

/**
 * Convert text to wide-character string and trim to min acceptable lenght.
 * @param text string to encode
 * @return wide string
 */
std::wstring to_wide(const std::string& text)
{
    size_t len = text.length();
    std::wstring wide(len + 1, 0);

    len = std::mbstowcs(wide.data(), text.c_str(), len * sizeof(wide[0]));
    if (len != std::wstring::npos) {
        wide.resize(len);
    } else {
        // something wrong with locale, try to convert ASCII
        wide.clear();
        for (const auto chr : text) {
            wide += chr < ' ' || chr > '~' ? FALLBACK_CHR : chr;
        }
        len = wide.length();
    }

    if (len > MAX_TEXT_LEN) {
        wide.resize(MAX_TEXT_LEN - 1);
        wide += L'…';
    }

    return wide;
}

} // anonymous namespace

Text& Text::self()
{
    static Text singleton;
    return singleton;
}

Text::Text()
    : enable(Defaults::text::enable)
    , spacing(Defaults::text::spacing)
    , padding(Defaults::text::padding)
    , foreground(Defaults::text::foreground)
    , background(Defaults::text::background)
    , shadow(Defaults::text::shadow)
{
    overall_tm.delay = Defaults::text::overall;
    overall_tm.show = true;
    status_tm.delay = Defaults::text::status;
    status_tm.show = true;
}

void Text::initialize()
{
    Application::self().add_fdpoll(overall_tm.fd, [this] {
        overall_tm.fd.reset(0, 0);
        overall_tm.show = false;
        Application::redraw();
    });
    Application::self().add_fdpoll(status_tm.fd, [this] {
        status.clear();
        status_tm.fd.reset(0, 0);
        status_tm.show = false;
        Application::redraw();
    });
}

void Text::set_scheme(const Position pos, const Scheme& scheme)
{
    KeyValBlock& block = blocks[static_cast<size_t>(pos)];
    block.set_scheme(scheme);
}

void Text::set_font(const std::string& name)
{
    if (font.load(name)) {
        recalc();
    }
}

void Text::set_size(const size_t size)
{
    font.set_size(size);
    recalc();
}

void Text::set_spacing(const ssize_t size)
{
    spacing = size;
    recalc();
}

void Text::set_scale(const double scale)
{
    font.set_scale(scale);
    recalc();
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
    overall_tm.delay = timeout;

    if (enable) {
        overall_tm.show = true;
        overall_tm.fd.reset(overall_tm.delay, 0);
        Application::redraw();
    }
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

void Text::clear()
{
    fields.clear();
}

void Text::set_status(const std::string& msg)
{
    if (msg.empty()) {
        status.clear();
        status_tm.fd.reset(0, 0);
        status_tm.show = false;
    } else {
        status.set(msg);
        status.recalc(spacing);
        status_tm.show = true;
        status_tm.fd.reset(status_tm.delay, 0);
    }

    Application::redraw();
}

void Text::reset(const ImagePtr& image)
{
    assert(image);

    reset(image->entry);

    set_field(FIELD_IMAGE_FORMAT, image->format);
    set_field(FIELD_FRAME_TOTAL, std::to_wstring(image->frames.size()));

    // import meta info
    for (const auto& [key, value] : image->meta) {
        const std::string name = std::string(FIELD_META) + "." + key;
        set_field(name, value);
    }

    update();
}

void Text::reset(const ImageEntryPtr& entry)
{
    assert(entry);

    fields.clear();

    set_field(FIELD_FILE_PATH, entry->path);
    set_field(FIELD_FILE_DIR, entry->path.parent_path().filename());
    set_field(FIELD_FILE_NAME, entry->path.filename());
    set_field(FIELD_FILE_SIZE, std::to_wstring(entry->size));
    set_field(FIELD_LIST_INDEX, std::to_wstring(entry->index + 1));
    set_field(FIELD_LIST_TOTAL, std::to_wstring(ImageList::self().size()));

    // human readable file size
    const size_t mib = 1024UL * 1024UL;
    set_field(FIELD_FILE_SIZE_HR,
              std::format(L"{:.02f} {}iB",
                          static_cast<float>(entry->size) /
                              (entry->size >= mib ? mib : 1024),
                          entry->size >= mib ? L'M' : L'K'));

    // human readable file modification time
    const std::tm* local_tm = localtime(&entry->mtime);
    wchar_t time_buff[32];
    std::wcsftime(time_buff, sizeof(time_buff), L"%Y-%m-%d %H:%M:%S", local_tm);
    set_field(FIELD_FILE_TIME, time_buff);

    // restart timer
    if (enable && overall_tm.delay) {
        overall_tm.show = true;
        overall_tm.fd.reset(overall_tm.delay, 0);
    }

    update();
}

void Text::set_field(const std::wstring& field, const std::wstring& value)
{
    if (!value.empty()) {
        fields.insert_or_assign(field, value);
    } else {
        fields.erase(field);
    }
}

void Text::set_field(const std::string& field, const std::string& value)
{
    set_field(to_wide(field), to_wide(value));
}

void Text::update()
{
    for (auto& block : blocks) {
        block.update(fields);
        block.recalc(spacing);
    }
}

void Text::draw(Pixmap& target) const
{
    // show status message
    if (status_tm.show && !status.lines.empty()) {
        Point pos(0, target.height() - status.height - padding);
        if (background.a != argb_t::min) {
            // draw background
            pos.x = target.width() / 2 - status.width / 2;
            target.fill_blend({ pos.x, pos.y, status.width, status.height },
                              background);
        }
        // draw status text
        for (const auto& line : status.lines) {
            pos.x = target.width() / 2 - line.width() / 2;
            line.draw(target, pos, foreground);
            pos.y += status.line_height;
        }
    }

    // show text layer
    if (overall_tm.show && !fields.empty()) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            draw(static_cast<Position>(i), target);
        }
    }
}

void Text::recalc()
{
    for (auto& block : blocks) {
        block.recalc(spacing);
    }
    status.recalc(spacing);
    Application::redraw();
}

void Text::draw(const TextLine& line, Pixmap& target, const Point& pos) const
{
    assert(!line.text.empty());

    if (shadow.a != argb_t::min) {
        const ssize_t offset =
            std::max<ssize_t>(line.height() / SHADOW_FACTOR, 1);
        line.draw(target, pos + Point { .x = offset, .y = offset }, shadow);
    }

    line.draw(target, pos, foreground);
}

void Text::draw(const Position blkpos, Pixmap& target) const
{
    const KeyValBlock& block = blocks[static_cast<size_t>(blkpos)];

    // calculate initial position
    ssize_t x = 0;
    ssize_t y = 0;
    switch (blkpos) {
        case Position::TopLeft:
            x = padding;
            y = padding;
            break;
        case Position::TopRight:
            x = static_cast<ssize_t>(target.width()) - block.total_width -
                padding;
            y = padding;
            break;
        case Position::BottomLeft:
            x = padding;
            y = static_cast<ssize_t>(target.height()) - block.total_height -
                padding;
            break;
        case Position::BottomRight:
            x = static_cast<ssize_t>(target.width()) - block.total_width -
                padding;
            y = static_cast<ssize_t>(target.height()) - block.total_height -
                padding;
            break;
    }
    x = std::max(static_cast<ssize_t>(0), x);
    y = std::max(static_cast<ssize_t>(0), y);

    // draw background
    if (background.a != argb_t::min) {
        target.fill_blend({ x, y, block.total_width, block.total_height },
                          background);
    }

    // calculate shadow offset
    const ssize_t shadow_diff =
        std::max<ssize_t>(block.line_height / SHADOW_FACTOR, 1);
    const Point shadow_offset { .x = shadow_diff, .y = shadow_diff };

    // draw text lines
    for (const auto& line : block.lines) {
        if (line.value.text.empty()) {
            continue;
        }

        Point pos { .x = x, .y = y };

        // key
        if (!line.key.text.empty()) {
            if (shadow.a != argb_t::min) {
                line.key.draw(target, pos + shadow_offset, shadow);
            }
            line.key.draw(target, pos, foreground);
            pos.x += block.key_width;
        }

        // value
        if (shadow.a != argb_t::min) {
            line.value.draw(target, pos + shadow_offset, shadow);
        }
        line.value.draw(target, pos, foreground);

        y += block.line_height;
    }
}

size_t Text::TextLine::width() const
{
    size_t width = 0;
    for (const wchar_t ch : text) {
        width += font.get_glyph(ch).width();
    }
    if (width) {
        width += margin() * 2;
    }
    return width;
}

size_t Text::TextLine::height() const
{
    for (const wchar_t ch : text) {
        const size_t height = font.get_glyph(ch).height();
        if (height) {
            return height;
        }
    }
    return 0;
}

size_t Text::TextLine::margin() const
{
    return height() / MARGIN_FACTOR;
}

void Text::TextLine::draw(Pixmap& target, const Point& pos,
                          const argb_t color) const
{
    const ssize_t max_x = target.width();

    Point pt = pos;
    pt.x += margin();

    for (const wchar_t ch : text) {
        const Pixmap& glyph = font.get_glyph(ch);
        if (glyph) {
            target.mask(glyph, pt, color);
            pt.x += glyph.width();
            if (pt.x > max_x) {
                break;
            }
        }
    }
}

void Text::KeyValBlock::TemplateLine::update(const Fields& fields)
{
    text = templ;

    size_t br_open = 0;
    while ((br_open = text.find(L'{', br_open)) != std::wstring::npos) {
        // handle escaping case
        if (br_open + 1 < text.length() && text[br_open + 1] == L'{') {
            text.erase(br_open, 1);
            br_open += 1;
            continue;
        }

        // get position of closing bracket
        const size_t br_close = text.find(L'}', br_open + 1);
        if (br_close == std::string::npos) {
            break;
        }

        // get field name
        const size_t len = br_close - br_open;
        const std::wstring name = text.substr(br_open + 1, len - 1);

        // replace field value inside output string
        const auto it = fields.find(name);
        if (it != fields.end()) {
            text.replace(br_open, len + 1, it->second);
            br_open += it->second.length();
        } else {
            text.erase(br_open, len + 1);
        }
    }
}

void Text::KeyValBlock::set_scheme(const Scheme& scheme)
{
    lines.clear();
    lines.reserve(scheme.size());

    for (const auto& line : scheme) {
        if (line.empty()) {
            continue;
        }
        BlockLine kv;
        const size_t delim = line.find('\t');
        if (delim == std::string::npos) {
            kv.value.templ = to_wide(line);
        } else {
            kv.key.templ = to_wide(line.substr(0, delim));
            kv.value.templ = to_wide(line.substr(delim + 1));
        }
        lines.emplace_back(kv);
    }
}

void Text::KeyValBlock::update(const Fields& fields)
{
    for (auto& [key, value] : lines) {
        key.update(fields);
        value.update(fields);
    }
}

void Text::KeyValBlock::recalc(const ssize_t spacing)
{
    key_width = 0;
    value_width = 0;
    total_width = 0;
    total_height = 0;
    line_height = 0;

    size_t total_lines = 0;
    size_t glyph_height = 0;

    for (const auto& line : lines) {
        if (!line.value.text.empty()) {
            ++total_lines;
            key_width = std::max(key_width, line.key.width());
            value_width = std::max(value_width, line.value.width());
            if (glyph_height == 0) {
                glyph_height = line.value.height();
            }
        }
    }

    if (total_lines) {
        const ssize_t line_offset =
            std::clamp(spacing, -static_cast<ssize_t>(glyph_height),
                       static_cast<ssize_t>(glyph_height));
        line_height = glyph_height + line_offset;
        total_width = value_width + key_width;
        total_height = glyph_height * total_lines;
        total_height += line_offset * (total_lines - 1);
    }
}

void Text::StatusBlock::clear()
{
    lines.clear();
    width = 0;
    height = 0;
    line_height = 0;
}

void Text::StatusBlock::set(const std::string& msg)
{
    assert(!msg.empty());

    clear();

    size_t last = 0;
    size_t next = 0;
    while ((next = msg.find('\n', last)) != std::string::npos) {
        lines.emplace_back(to_wide(msg.substr(last, next - last)));
        last = next + 1;
    }
    lines.emplace_back(to_wide(msg.substr(last)));
}

void Text::StatusBlock::recalc(const ssize_t spacing)
{
    width = 0;
    height = 0;
    line_height = 0;

    if (lines.empty()) {
        return;
    }

    size_t total_lines = 0;
    size_t glyph_height = 0;
    for (const auto& line : lines) {
        ++total_lines;
        width = std::max(width, line.width());
        if (glyph_height == 0) {
            glyph_height = line.height();
        }
    }

    const ssize_t line_offset =
        std::clamp(spacing, -static_cast<ssize_t>(glyph_height),
                   static_cast<ssize_t>(glyph_height));
    line_height = glyph_height + line_offset;

    height = glyph_height * total_lines;
    height += line_offset * (total_lines - 1);
}

// SPDX-License-Identifier: MIT
// Image view mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.hpp"

#include "application.hpp"
#include "defaults.hpp"
#include "imageformat.hpp"
#include "log.hpp"
#include "render.hpp"
#include "resources.hpp"
#include "text.hpp"

#include <cmath>
#include <format>
#include <utility>

/** Max scale factor. */
constexpr double MAX_SCALE = 100.0;

Viewer& Viewer::self()
{
    static Viewer singleton;
    return singleton;
}

Viewer::Viewer()
    : auto_center(Defaults::viewer::auto_center)
    , imagelist_loop(Defaults::viewer::imagelist_loop)
    , default_scale(Defaults::viewer::scale)
    , default_pos(Defaults::viewer::position)
    , scale(1.0)
    , window_bkg(Defaults::viewer::window_bkg)
    , tr_chessboard(Defaults::viewer::tr_chessboard)
    , tr_cbsize(Defaults::viewer::tr_cbsize)
    , tr_cbcolor { Defaults::viewer::tr_cbcolor0,
                   Defaults::viewer::tr_cbcolor1 }
    , tr_bgcolor(Defaults::viewer::tr_bgcolor)
    , animation(Defaults::viewer::animation)
{
    image_pool.preload.capacity = Defaults::viewer::preload;
    image_pool.history.capacity = Defaults::viewer::history;

    pinch_factor = Defaults::viewer::pinch_factor;
    mark_color = Defaults::viewer::mark_color;

    text_scheme[static_cast<size_t>(Text::TopLeft)].assign(
        Defaults::viewer::text_scheme_tl.begin(),
        Defaults::viewer::text_scheme_tl.end());
    text_scheme[static_cast<size_t>(Text::TopRight)].assign(
        Defaults::viewer::text_scheme_tr.begin(),
        Defaults::viewer::text_scheme_tr.end());
    text_scheme[static_cast<size_t>(Text::BottomLeft)].assign(
        Defaults::viewer::text_scheme_bl.begin(),
        Defaults::viewer::text_scheme_bl.end());

    Defaults::viewer::bind_inputs(this);
}

bool Viewer::open(const ImageList::Dir dir)
{
    if (image) {
        const bool forward = dir == ImageList::Dir::Last ||
            dir == ImageList::Dir::Random || dir == ImageList::Dir::Next ||
            dir == ImageList::Dir::NextParent;
        const ImageEntryPtr next = ImageList::self().get(image->entry, dir);
        return open(next, forward);
    }
    return false;
}

bool Viewer::reload()
{
    if (image && !open(image->entry, true) && !open(image->entry, false)) {
        set_image(nullptr); // no more images to view
    }
    return !!image;
}

size_t Viewer::next_frame()
{
    if (image) {
        size_t index = frame_index + 1;
        if (index >= image->frames.size()) {
            index = 0;
        }
        set_frame(index);
        return index;
    }
    return 0;
}

size_t Viewer::prev_frame()
{
    if (image) {
        const size_t index =
            frame_index ? frame_index - 1 : image->frames.size() - 1;
        set_frame(index);
        return index;
    }
    return 0;
}

void Viewer::export_frame(const std::filesystem::path& path) const
{
    if (image) {
        const Pixmap& pm = image->frames[frame_index].pm;
        if (!pm.save(path)) {
            Text::self().set_status("Failed to export image");
        }
    }
}

void Viewer::set_scale(const Scale sc)
{
    if (!image) {
        return;
    }

    const Pixmap& pm = image->frames[frame_index].pm;
    const double ratio_w = static_cast<double>(window_size.width) / pm.width();
    const double ratio_h =
        static_cast<double>(window_size.height) / pm.height();

    double abs_sc = 1.0;
    switch (sc) {
        case Scale::Keep:
        case Scale::Optimal:
            abs_sc = std::min({ 1.0, ratio_w, ratio_h });
            break;
        case Scale::FitWindow:
            abs_sc = std::min(ratio_w, ratio_h);
            break;
        case Scale::FitWidth:
            abs_sc = ratio_w;
            break;
        case Scale::FitHeight:
            abs_sc = ratio_h;
            break;
        case Scale::FillWindow:
            abs_sc = std::max(ratio_w, ratio_h);
            break;
        case Scale::RealSize:
            abs_sc = 1.0; // 100 %
            break;
    }

    set_scale(abs_sc);
}

void Viewer::set_scale(const double sc, const Point& preserve)
{
    if (!image) {
        return;
    }

    double new_scale = sc;

    // check scale limits
    const Pixmap& pm = image->frames[frame_index].pm;
    const Size scaled = static_cast<Size>(pm) * new_scale;
    if (!scaled) {
        return;
    }
    if (sc > MAX_SCALE) {
        new_scale = MAX_SCALE;
    }

    // save base point of the image
    const Point bpt(preserve ? preserve.x : window_size.width / 2,
                    preserve ? preserve.y : window_size.height / 2);
    const double pr_x = (static_cast<double>(bpt.x) - position.x) / scale;
    const double pr_y = (static_cast<double>(bpt.y) - position.y) / scale;

    // set new scale
    scale = new_scale;

    // restore center
    position.x = bpt.x - pr_x * scale;
    position.y = bpt.y - pr_y * scale;

    update_text(TextUpdate::Scale);

    fixup_position();
}

void Viewer::reset()
{
    if (image) {
        if (std::holds_alternative<Scale>(default_scale)) {
            set_scale(std::get<Scale>(default_scale));
        } else {
            set_scale(std::get<double>(default_scale));
        }
        set_position(default_pos);
    }
}

void Viewer::set_position(const Position pos)
{
    if (!image) {
        return;
    }

    const Pixmap& pm = image->frames[frame_index].pm;
    const Size scaled = static_cast<Size>(pm) * scale;

    switch (pos) {
        case Position::Center:
            position.x = window_size.width / 2 - scaled.width / 2;
            position.y = window_size.height / 2 - scaled.height / 2;
            break;
        case Position::TopCenter:
            position.x = window_size.width / 2 - scaled.width / 2;
            position.y = 0;
            break;
        case Position::BottomCenter:
            position.x = window_size.width / 2 - scaled.width / 2;
            position.y = window_size.height - scaled.height;
            break;
        case Position::LeftCenter:
            position.x = 0;
            position.y = window_size.height / 2 - scaled.height / 2;
            break;
        case Position::RightCenter:
            position.x = window_size.width - scaled.width;
            position.y = window_size.height / 2 - scaled.height / 2;
            break;
        case Position::TopLeft:
            position.x = 0;
            position.y = 0;
            break;
        case Position::TopRight:
            position.x = window_size.width - scaled.width;
            position.y = 0;
            break;
        case Position::BottomLeft:
            position.x = 0;
            position.y = window_size.height - scaled.height;
            break;
        case Position::BottomRight:
            position.x = window_size.width - scaled.width;
            position.y = window_size.height - scaled.height;
            break;
    }

    Application::redraw();
}

void Viewer::set_position(const Point& pos)
{
    if (image) {
        position = pos;
        fixup_position();
    }
}

void Viewer::flip_vertical()
{
    if (image) {
        image->flip_vertical();
        Application::redraw();
    }
}

void Viewer::flip_horizontal()
{
    if (image) {
        image->flip_horizontal();
        Application::redraw();
    }
}

void Viewer::rotate(const size_t angle)
{
    assert(angle == 90 || angle == 180 || angle == 270);

    if (image) {
        image->rotate(angle);

        const Pixmap& pm = image->frames[frame_index].pm;
        const ssize_t diff = static_cast<ssize_t>(pm.width()) -
            static_cast<ssize_t>(pm.height());
        const ssize_t shift = (scale * diff) / 2;
        position.x -= shift;
        position.y += shift;

        update_text(TextUpdate::Frame);

        fixup_position();
    }
}

void Viewer::enable_animation(const bool enable)
{
    if (image) {
        const size_t duration = image->frames[frame_index].duration;
        animation = enable && image->frames.size() > 1 && duration;
        animation_timer.reset(enable ? duration : 0, 0);
    }
}

void Viewer::set_window_background(const argb_t& color)
{
    window_bkg = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Viewer::set_window_background(const Background mode)
{
    window_bkg = mode;
    if (is_active()) {
        Application::redraw();
    }
}

void Viewer::set_image_background(const argb_t& color)
{
    tr_chessboard = false;
    tr_bgcolor = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Viewer::set_image_chessboard(const size_t size, const argb_t& color1,
                                  const argb_t& color2)
{
    tr_chessboard = true;
    tr_cbsize = size;
    tr_cbcolor[0] = color1;
    tr_cbcolor[1] = color2;
    if (is_active()) {
        Application::redraw();
    }
}

void Viewer::set_preload_limit(const size_t size)
{
    image_pool.preload.capacity = size;
}

void Viewer::set_history_limit(const size_t size)
{
    image_pool.history.capacity = size;
}

void Viewer::bind_image_drag(const InputMouse& input)
{
    drag = input;
    bind_input(drag, []() {
        Application::get_ui()->set_cursor(Ui::CursorShape::Drag);
    });
}

void Viewer::initialize()
{
    Application::self().add_fdpoll(animation_timer, [this]() {
        next_frame();
        enable_animation(true);
    });
}

void Viewer::activate(const ImageEntryPtr& entry, const Size& wnd)
{
    AppMode::activate(entry, wnd);

    window_size = wnd;

    if (entry && image && image->entry == entry && !entry->removed) {
        set_image(image); // reinit state without reloading image
    } else if (!entry || (!open(entry, true) && !open(entry, false))) {
        set_image(nullptr);
    }
}

void Viewer::deactivate()
{
    preloader_stop();

    // restore cursor and content type
    Ui* ui = Application::self().get_ui();
    ui->set_ctype(Ui::ContentType::Static);
    ui->set_cursor(Ui::CursorShape::Default);
}

ImageEntryPtr Viewer::get_current()
{
    return image ? image->entry : nullptr;
}

bool Viewer::set_current(const ImageEntryPtr& entry)
{
    assert(entry && !entry->removed);

    ImagePtr new_image = nullptr;

    if (image && image->entry == entry) {
        // remove entry from cache in reloading mode
        const std::scoped_lock lock(image_pool.mutex);
        image_pool.history.get(entry);
        image_pool.preload.get(entry);
    } else {
        // get file from history/preload cache
        const std::scoped_lock lock(image_pool.mutex);
        new_image = image_pool.preload.get(entry);
        if (new_image) {
            Log::verbose("Got image {} from preloading cache",
                         entry->path.filename().string());
        } else {
            new_image = image_pool.history.get(entry);
            if (new_image) {
                Log::verbose("Got image {} from history cache",
                             entry->path.filename().string());
            }
        }
    }

    if (!new_image) {
        new_image = FormatFactory::self().load(entry);
        if (!new_image) {
            return false; // failed to load
        }
    }

    set_image(new_image);
    return true;
}

void Viewer::window_resize(const Size& wnd)
{
    window_size = wnd;
    if (image) {
        fixup_position();
    }
}

void Viewer::window_redraw(Pixmap& wnd)
{
    if (!image) {
        const argb_t* bkg = std::get_if<argb_t>(&window_bkg);
        draw_empty(wnd, bkg ? *bkg : Defaults::viewer::window_bkg);
        return;
    }

    const Pixmap& pm = image->frames[frame_index].pm;
    const Rectangle imgr = { position, static_cast<Size>(pm) * scale };

    // clear image background
    if (pm.format() == Pixmap::ARGB) {
        if (tr_chessboard) {
            wnd.grid(imgr, tr_cbsize, tr_cbcolor[0], tr_cbcolor[1]);
        } else {
            wnd.fill(imgr, tr_bgcolor);
        }
    }

    // put image on window surface
    image->draw(frame_index, wnd, scale, position.x, position.y);

    // fill window background
    if (std::holds_alternative<argb_t>(window_bkg)) {
        Render::self().fill_inverse(wnd, imgr, std::get<argb_t>(window_bkg));
    } else {
        switch (std::get<Background>(window_bkg)) {
            case Background::Mirror:
                Render::self().mirror_background(wnd, imgr);
                break;
            case Background::Extend:
                Render::self().extend_background(wnd, imgr);
                break;
            case Background::Auto:
                if (imgr.width > imgr.height) {
                    Render::self().mirror_background(wnd, imgr);
                } else {
                    Render::self().extend_background(wnd, imgr);
                }
                break;
        }
    }

    // mark icon
    if (image->entry->mark) {
        const ssize_t margin = 10;
        const ssize_t x = static_cast<ssize_t>(wnd.width()) -
            static_cast<ssize_t>(Resource::mark.width()) - margin;
        const ssize_t y = static_cast<ssize_t>(wnd.height()) -
            static_cast<ssize_t>(Resource::mark.height()) - margin;
        wnd.mask(Resource::mark, { x, y }, mark_color);
    }
}

void Viewer::handle_mmove(const InputMouse& input, const Point&,
                          const Point& delta)
{
    if (drag && drag == input) {
        set_position(position + delta);
    }
}

void Viewer::handle_pinch(const double scale_delta)
{
    set_scale(scale + scale_delta * pinch_factor);
}

void Viewer::handle_imagelist(const ImageListEvent event,
                              const std::vector<ImageEntryPtr>& entries)
{
    AppMode::handle_imagelist(event, entries);

    if (event == ImageListEvent::Modify || event == ImageListEvent::Remove) {
        // remove entries from cache
        const std::scoped_lock lock(image_pool.mutex);
        for (const auto& entry : entries) {
            image_pool.history.get(entry);
            image_pool.preload.get(entry);
        }
    }

    switch (event) {
        case ImageListEvent::Create:
            if (!image) {
                open(entries.front(), true);
            } else {
                preloader_start();
            }
            break;
        case ImageListEvent::Modify:
            if (image) {
                for (const auto& entry : entries) {
                    if (entry == image->entry) {
                        reload();
                        break;
                    }
                }
            }
            break;
        case ImageListEvent::Remove:
            if (image && image->entry->removed && !open(image->entry, true) &&
                !open(image->entry, false)) {
                set_image(nullptr); // no more images to view
            }
            break;
    }
}

bool Viewer::open(const ImageEntryPtr& entry, const bool forward)
{
    ImageList& il = ImageList::self();

    ImageEntryPtr next = entry;
    if (!next) {
        next = il.get(nullptr,
                      forward ? ImageList::Dir::First : ImageList::Dir::Last);
    } else if (next->removed) {
        next =
            il.get(next, forward ? ImageList::Dir::Next : ImageList::Dir::Prev);
    }

    while (next) {
        if (set_current(next)) {
            return true;
        }
        next = il.remove(next, forward);

        if (!next && imagelist_loop && il.size() > 0) {
            // reshuffle random on new loop
            if (il.get_order() == ImageList::Order::Random) {
                il.set_order(ImageList::Order::Random);
            }

            // start new loop
            const ImageList::Dir dir =
                forward ? ImageList::Dir::First : ImageList::Dir::Last;
            next = il.get(nullptr, dir);

            // avoid opening the same image in random mode
            if (next && next == entry &&
                il.get_order() == ImageList::Order::Random) {
                next = il.get(next, ImageList::Dir::Next);
            }
        }
    }
    return false;
}

void Viewer::set_image(const ImagePtr& img)
{
    if (image && image != img) {
        // put current image to history
        const std::scoped_lock lock(image_pool.mutex);
        image_pool.history.put(image);
    }

    image = img;
    frame_index = 0;

    if (!image) {
        switch_current();
        return;
    }

    const Pixmap& pm = image->frames[0].pm;

    const bool is_animation =
        image->frames.size() > 1 && image->frames[0].duration;

    // set content type
    Ui* ui = Application::self().get_ui();
    ui->set_ctype(is_animation ? Ui::ContentType::Animation
                               : Ui::ContentType::Static);

    if (std::holds_alternative<Scale>(default_scale) &&
        std::get<Scale>(default_scale) == Scale::Keep && previmg) {
        // handle "keep scale" mode
        const ssize_t diff_w = static_cast<ssize_t>(previmg.width) -
            static_cast<ssize_t>(pm.width());
        const ssize_t diff_h = static_cast<ssize_t>(previmg.height) -
            static_cast<ssize_t>(pm.height());
        position.x += std::floor(scale * diff_w) / 2.0;
        position.y += std::floor(scale * diff_h) / 2.0;
        fixup_position();
    } else {
        if (std::holds_alternative<Scale>(default_scale)) {
            set_scale(std::get<Scale>(default_scale));
        } else {
            set_scale(std::get<double>(default_scale));
        }

        set_position(default_pos);
    }

    previmg = pm;

    enable_animation(is_animation);

    preloader_start();

    switch_current();
    update_text(TextUpdate::All);
}

void Viewer::set_frame(const size_t index)
{
    if (image) {
        assert(index < image->frames.size());
        frame_index = index;
        update_text(TextUpdate::Frame);
        Application::redraw();
    }
}

void Viewer::update_text(const TextUpdate what) const
{
    assert(image);

    Text& text = Text::self();

    if (what == TextUpdate::All) {
        text.reset(image);
    }

    if (what == TextUpdate::All || what == TextUpdate::Frame) {
        const Pixmap& pm = image->frames[frame_index].pm;
        text.set_field(Text::FIELD_FRAME_INDEX,
                       std::to_string(frame_index + 1));
        text.set_field(Text::FIELD_FRAME_WIDTH, std::to_string(pm.width()));
        text.set_field(Text::FIELD_FRAME_HEIGHT, std::to_string(pm.height()));
    }

    if (what == TextUpdate::All || what == TextUpdate::Scale) {
        std::string tscale;
        if (scale >= 0.1) {
            tscale = std::format("{}%", static_cast<size_t>(scale * 100));
        } else {
            tscale = std::format("{:.02}%", scale * 100);
        }
        text.set_field(Text::FIELD_SCALE, tscale);
    }

    text.update();
}

ssize_t Viewer::fixup_position(const ssize_t pos, const size_t sz,
                               const size_t max_pos)
{
    ssize_t fixed = pos;

    if (sz <= max_pos) {
        // entire size fits, center it
        fixed = max_pos / 2 - sz / 2;
    } else {
        // prevent going outside the max position
        const ssize_t out_pos = fixed + sz;
        if (fixed > 0 && std::cmp_greater(out_pos, max_pos)) {
            fixed = 0;
        }
        if (pos < 0 && std::cmp_less(out_pos, max_pos)) {
            fixed = max_pos - sz;
        }
    }

    return fixed;
}

void Viewer::fixup_position()
{
    assert(image);

    const Pixmap& pm = image->frames[frame_index].pm;
    const Size scaled = static_cast<Size>(pm) * scale;

    if (auto_center) {
        position.x =
            fixup_position(position.x, scaled.width, window_size.width);
        position.y =
            fixup_position(position.y, scaled.height, window_size.height);
    } else {
        // don't let image to be far out of window
        if (position.x + static_cast<ssize_t>(scaled.width) < 0) {
            position.x = -static_cast<ssize_t>(scaled.width);
        }
        if (std::cmp_greater(position.x, window_size.width)) {
            position.x = window_size.width;
        }
        if (position.y + static_cast<ssize_t>(scaled.height) < 0) {
            position.y = -static_cast<ssize_t>(scaled.height);
        }
        if (std::cmp_greater(position.y, window_size.height)) {
            position.y = window_size.height;
        }
    }

    Application::redraw();
}

void Viewer::preloader_work()
{
    assert(image);

    ImageList& il = ImageList::self();
    ImagePtr current_image = image;
    ImageEntryPtr last_entry = image->entry;
    size_t counter = 0;

    while (!image_pool.stop && counter < image_pool.preload.capacity) {
        if (!image) {
            break; // no current image in viewer
        }
        if (current_image != image) {
            // current image has changed, restart preloading
            current_image = image;
            last_entry = image->entry;
            counter = 0;
        }

        ImageEntryPtr next_entry = il.get(last_entry, ImageList::Dir::Next);
        if (!next_entry && imagelist_loop) {
            next_entry = il.get(nullptr, ImageList::Dir::First);
        }
        if (!next_entry || next_entry == last_entry) {
            // no more images to preload
            const std::scoped_lock lock(image_pool.mutex);
            image_pool.preload.trim(counter);
            break;
        }

        ImagePtr next_image = nullptr;

        // get existing image form history/preload cache
        {
            const std::scoped_lock lock(image_pool.mutex);
            next_image = image_pool.preload.get(next_entry);
            if (!next_image) {
                next_image = image_pool.history.get(next_entry);
            }
        }

        // load image
        if (!next_image) {
            next_image = FormatFactory::self().load(next_entry);
        }

        if (!next_image) {
            il.remove(next_entry);
        } else {
            Log::verbose("Put image {} to cache",
                         next_entry->path.filename().string());
            const std::scoped_lock lock(image_pool.mutex);
            image_pool.preload.put(next_image);
            last_entry = next_image->entry;
            ++counter;
        }
    }

    image_pool.stop = true;
}

void Viewer::preloader_start()
{
    if (image_pool.thread.joinable()) {
        if (image_pool.stop) {
            image_pool.thread.join();
        } else {
            return; // already in progress
        }
    }
    image_pool.thread = std::thread(&Viewer::preloader_work, this);
}

void Viewer::preloader_stop()
{
    if (image_pool.thread.joinable()) {
        image_pool.stop = true;
        image_pool.thread.join();
    }
}

void Viewer::Cache::trim(const size_t size)
{
    cache.resize(size);
}

void Viewer::Cache::put(const ImagePtr& image)
{
    if (capacity) {
        if (cache.size() == capacity) {
            cache.pop_back();
        }
        cache.push_front(image);
    }
}

ImagePtr Viewer::Cache::get(const ImageEntryPtr& entry)
{
    auto it = std::find_if(cache.begin(), cache.end(),
                           [&entry](const ImagePtr& image) {
                               return image->entry == entry;
                           });
    if (it == cache.end()) {
        return nullptr;
    }
    ImagePtr image = *it;
    cache.erase(it);
    return image;
}

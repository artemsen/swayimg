// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.hpp"

#include "application.hpp"
#include "defaults.hpp"
#include "imageformat.hpp"
#include "imagelist.hpp"
#include "render.hpp"
#include "resources.hpp"
#include "text.hpp"

#include <sys/stat.h>

#include <array>
#include <utility>

// Limits for thumbnail size and other parameters
constexpr size_t THIMB_SIZE_MIN = 10;
constexpr size_t THIMB_SIZE_MAX = 10000;
constexpr size_t PADDING_SIZE_MAX = 1000;
constexpr size_t BORDER_SIZE_MAX = 100;
constexpr double SSCALE_MAX = 10.0;

/** Number of threads used for loading thumbnails. */
constexpr size_t THUMB_LOAD_THREADS = 4;

Gallery& Gallery::self()
{
    static Gallery singleton;
    return singleton;
}

Gallery::Gallery()
    : aspect(Defaults::gallery::aspect)
    , border_size(Defaults::gallery::border_size)
    , selected_scale(Defaults::gallery::selected_scale)
    , clr_window(Defaults::gallery::clr_window)
    , clr_background(Defaults::gallery::clr_background)
    , clr_select(Defaults::gallery::clr_select)
    , clr_border(Defaults::gallery::clr_border)
    , hover_select(Defaults::gallery::hover_select)
    , tpool(THUMB_LOAD_THREADS)
    , pstore_enable(Defaults::gallery::pstore_enable)
    , pstore_path(Defaults::gallery::pstore_path())
    , preload(Defaults::gallery::preload)
    , cache_size(Defaults::gallery::cache_size)
{
    pinch_factor = Defaults::gallery::pinch_factor;
    mark_color = Defaults::gallery::mark_color;

    text_scheme[static_cast<size_t>(Text::TopLeft)].assign(
        Defaults::gallery::text_scheme_tl.begin(),
        Defaults::gallery::text_scheme_tl.end());
    text_scheme[static_cast<size_t>(Text::TopRight)].assign(
        Defaults::gallery::text_scheme_tr.begin(),
        Defaults::gallery::text_scheme_tr.end());

    Defaults::gallery::bind_inputs(this);
}

bool Gallery::select(const Layout::Direction dir)
{
    assert(is_active());

    if (!layout.get_selected() || !layout.select(dir)) {
        return false;
    }
    refresh();
    switch_current();
    return true;
}

void Gallery::reload()
{
    // stop loader
    tpool.cancel();
    tpool.wait();

    {
        // clear cache
        const std::scoped_lock lock(mutex);
        cache.clear();
    }

    refresh();
}

void Gallery::set_thumb_aspect(const Aspect ratio)
{
    aspect = ratio;
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::set_thumb_size(const size_t size)
{
    layout.set_thumb_size(std::clamp(size, THIMB_SIZE_MIN, THIMB_SIZE_MAX));
    if (is_active()) {
        refresh();
        Application::redraw();
    }
}

void Gallery::set_padding_size(const size_t size)
{
    layout.set_padding(std::min(size, PADDING_SIZE_MAX));
    if (is_active()) {
        refresh();
        Application::redraw();
    }
}

void Gallery::set_border_size(const size_t size)
{
    border_size = std::min(size, BORDER_SIZE_MAX);
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::set_border_color(const argb_t& color)
{
    clr_border = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::set_selected_scale(const double scale)
{
    selected_scale = std::min(scale, SSCALE_MAX);
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::set_selected_color(const argb_t& color)
{
    clr_select = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::set_background_color(const argb_t& color)
{
    clr_background = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::set_window_color(const argb_t& color)
{
    clr_window = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Gallery::enable_hover(const bool enable)
{
    hover_select = enable;
}

void Gallery::set_cache_size(const size_t size)
{
    cache_size = size;
    if (is_active()) {
        refresh();
    }
}

void Gallery::enable_preload(const bool enable)
{
    preload = enable;
    if (is_active()) {
        refresh();
    }
}

void Gallery::enable_pstore(const bool enable)
{
    pstore_enable = enable;
}

void Gallery::set_pstore_path(const std::filesystem::path& path)
{
    pstore_path = path;
}

void Gallery::initialize() {}

void Gallery::activate(const ImageEntryPtr& entry, const Size& wnd)
{
    AppMode::activate(entry, wnd);
    layout.select(entry && !entry->removed ? entry : nullptr);
    layout.set_window_size(wnd);
    set_current(entry);
}

void Gallery::deactivate()
{
    tpool.cancel();
    tpool.wait();
}

ImageEntryPtr Gallery::get_current()
{
    return layout.get_selected();
}

bool Gallery::set_current(const ImageEntryPtr& entry)
{
    layout.select(entry && !entry->removed ? entry : nullptr);
    refresh();
    switch_current();
    return true;
}

void Gallery::window_resize(const Size& wnd)
{
    layout.set_window_size(wnd);
    refresh();
}

void Gallery::window_redraw(Pixmap& wnd)
{
    const ImageEntryPtr current = layout.get_selected();
    if (!current) {
        draw_empty(wnd, clr_window);
        return;
    }

    wnd.fill({ 0, 0, wnd.width(), wnd.height() }, clr_window);

    const auto& scheme = layout.get_scheme();
    size_t selected_scheme_idx = 0;
    size_t i = 0;
    // draw all exclude the currently selected
    for (const auto& it : scheme) {
        if (it.img == current) {
            selected_scheme_idx = i;
        } else {
            draw(it, wnd);
            i++;
        }
    }
    draw(scheme[selected_scheme_idx], wnd);
}

void Gallery::handle_mmove(const InputMouse&, const Point& pos, const Point&)
{
    if (hover_select && layout.select(pos)) {
        switch_current();
    }
}

void Gallery::handle_pinch(const double scale_delta)
{
    set_thumb_size(get_thumb_size() + scale_delta * pinch_factor);
}

void Gallery::handle_imagelist(const ImageListEvent event,
                               const std::vector<ImageEntryPtr>& entries)
{
    AppMode::handle_imagelist(event, entries);

    if (event == ImageListEvent::Modify || event == ImageListEvent::Remove) {
        // remove entry from cache
        const std::scoped_lock lock(mutex);
        for (const auto& entry : entries) {
            cache.erase(entry);
        }
    }

    if (event == ImageListEvent::Create) {
        if (!layout.get_selected()) {
            layout.select(entries.front());
            switch_current();
        }
    } else if (event == ImageListEvent::Remove) {
        const ImageEntryPtr entry = layout.get_selected();
        if (entry && entry->removed) {
            if (!layout.select(Layout::Right) && !layout.select(Layout::Left)) {
                layout.select(nullptr);
            }
            switch_current();
        }
    }

    layout.update();
    refresh();
    Application::redraw();
}

void Gallery::draw(const Layout::Thumbnail& tlay, Pixmap& wnd)
{
    const std::scoped_lock lock(mutex);

    const bool selected = (tlay.img == layout.get_selected());
    const Size wnd_size = wnd;
    const size_t tile_size = layout.get_thumb_size();
    const Pixmap* pm = get_thumbnail(tlay.img);

    // calculate tile position/size
    Rectangle tile {
        tlay.pos, { tile_size, tile_size }
    };
    if (selected) {
        tile.width *= selected_scale;
        tile.height *= selected_scale;
        tile.x -= tile.width / 2 - tile_size / 2;
        tile.y -= tile.height / 2 - tile_size / 2;

        // prevent going beyond the window
        if (tile.x + tile.width + border_size > wnd_size.width) {
            tile.x = wnd_size.width - tile.width - border_size;
        }
        if (std::cmp_less(tile.x, border_size)) {
            tile.x = border_size;
        }
        if (tile.y + tile.height + border_size > wnd_size.height) {
            tile.y = wnd_size.height - tile.height - border_size;
        }
        if (std::cmp_less(tile.y, border_size)) {
            tile.y = border_size;
        }
    }

    // draw background
    Rectangle bkg = tile;
    if (aspect == Aspect::Keep && pm && pm->width() != pm->height()) {
        const double ratio = static_cast<double>(pm->width()) / pm->height();
        if (ratio > 1.0) {
            bkg.height /= ratio;
        } else {
            bkg.width *= ratio;
        }
        bkg.x += tile.width / 2 - bkg.width / 2;
        bkg.y += tile.height / 2 - bkg.height / 2;
    }
    wnd.fill(bkg, selected ? clr_select : clr_background);

    if (pm) {
        // draw thumbnail
        const double scale_w = static_cast<double>(tile.width) / pm->width();
        const double scale_h = static_cast<double>(tile.height) / pm->height();
        const double scale = aspect == Aspect::Fill
            ? std::max(scale_w, scale_h)
            : std::min(scale_w, scale_h);

        const Point pos { static_cast<ssize_t>(tile.width / 2) -
                              static_cast<ssize_t>(scale * pm->width()) / 2,
                          static_cast<ssize_t>(tile.height / 2) -
                              static_cast<ssize_t>(scale * pm->height()) / 2 };

        Pixmap sub = wnd.submap(tile);
        Render::self().draw(sub, *pm, pos, scale);
    } else if (bkg.width > Resource::file.width() &&
               bkg.height > Resource::file.height()) {
        // draw icon
        const ssize_t x = bkg.x + static_cast<ssize_t>(bkg.width / 2) -
            static_cast<ssize_t>(Resource::file.width() / 2);
        const ssize_t y = bkg.y + static_cast<ssize_t>(bkg.height / 2) -
            static_cast<ssize_t>(Resource::file.height() / 2);
        wnd.mask(Resource::file, { x, y }, { 0x20, 0xff, 0xff, 0xff });
    }

    // draw border
    if (selected && clr_border.a && border_size) {
        Rectangle border = bkg;
        border.x -= border_size;
        border.y -= border_size;
        border.width += border_size * 2;
        border.height += border_size * 2;
        wnd.rectangle(border, clr_border, border_size);
    }

    // draw mark icon
    if (tlay.img->mark) {
        const ssize_t margin = 5;
        const ssize_t x = bkg.x + static_cast<ssize_t>(bkg.width) -
            static_cast<ssize_t>(Resource::mark.width()) - margin;
        const ssize_t y = bkg.y + static_cast<ssize_t>(bkg.height) -
            static_cast<ssize_t>(Resource::mark.height()) - margin;
        wnd.mask(Resource::mark, { x, y }, mark_color);
    }
}

void Gallery::refresh()
{
    const std::scoped_lock lock(mutex);
    clear_invisible();
    requeue_loading();
}

void Gallery::requeue_loading()
{
    tpool.cancel();

    ImageList& il = ImageList::self();
    const std::vector<Layout::Thumbnail>& scheme = layout.get_scheme();

    const size_t index_first = scheme.front().img->index;
    const size_t index_last = scheme.back().img->index;

    size_t preload_counter = preload ? cache_size : 0;

    ImageEntryPtr fwd = layout.get_selected();
    ImageEntryPtr back = il.get(fwd, ImageList::Dir::Prev);

    while (fwd || back) {
        if (fwd) {
            queue_thumbnail(fwd);
            fwd = il.get(fwd, ImageList::Dir::Next);
            if (fwd && fwd->index > index_last) {
                if (preload_counter) {
                    --preload_counter;
                } else {
                    fwd = nullptr;
                }
            }
        }
        if (back) {
            queue_thumbnail(back);
            back = il.get(back, ImageList::Dir::Prev);
            if (back && back->index < index_first) {
                if (preload_counter) {
                    --preload_counter;
                } else {
                    back = nullptr;
                }
            }
        }
    }
}

void Gallery::clear_invisible()
{
    const std::vector<Layout::Thumbnail>& scheme = layout.get_scheme();

    if (cache.size() > scheme.size() + cache_size) {
        const size_t visible_first = scheme.front().img->index;
        const size_t visible_last = scheme.back().img->index;
        const size_t store_min =
            visible_first > cache_size / 2 ? visible_first - cache_size / 2 : 1;
        const size_t store_max = visible_last + cache_size - cache_size / 2;

        std::erase_if(cache,
                      [store_min, store_max](
                          const std::pair<ImageEntryPtr, Pixmap>& key_value) {
                          return key_value.first->index < store_min ||
                              key_value.first->index > store_max;
                      });
    }
}

const Pixmap* Gallery::get_thumbnail(const ImageEntryPtr& entry)
{
    const auto it = cache.find(entry);
    return it == cache.end() ? nullptr : &it->second;
}

void Gallery::load_thumbnail(const ImageEntryPtr& entry)
{
    {
        const std::scoped_lock lock(mutex);
        active.insert(entry);
    }

    const size_t thumb_size = layout.get_thumb_size();

    Pixmap pm;

    if (pstore_enable) {
        pm = pstore_load(entry);
    }

    if (!pm) {
        pm = FormatFactory::self().preview(entry, thumb_size,
                                           aspect == Aspect::Fill);
        if (!pm) {
            Application::self().add_event(AppEvent::FileRemove { entry->path });
        } else if (pstore_enable) {
            pstore_save(entry, pm);
        }
    }

    const std::scoped_lock lock(mutex);
    active.erase(entry);
    if (pm) {
        cache.insert_or_assign(entry, pm);
        if (layout.is_visible(entry)) {
            Application::redraw();
        }
    }
}

void Gallery::queue_thumbnail(const ImageEntryPtr& entry)
{
    if (!get_thumbnail(entry) &&   // not yet loaded
        !active.contains(entry)) { // not currently loading
        tpool.add([this, entry]() {
            load_thumbnail(entry);
        });
    }
}

Pixmap Gallery::pstore_load(const ImageEntryPtr& entry) const
{
    std::filesystem::path thumb_path = pstore_path;
    thumb_path.concat(entry->path.string());

    if (!std::filesystem::exists(thumb_path)) {
        return {};
    }

    // check modification time
    struct stat st_image;
    struct stat st_thumb;
    if (stat(entry->path.c_str(), &st_image) == -1 ||
        stat(thumb_path.c_str(), &st_thumb) == -1 ||
        st_image.st_mtim.tv_sec > st_thumb.st_mtim.tv_sec) {
        return {};
    }

    // load thumbnail
    const ImageEntryPtr thumb_entry = std::make_shared<ImageEntry>();
    thumb_entry->path = thumb_path;
    const ImagePtr thumb_image = FormatFactory::self().load(thumb_entry);
    if (thumb_image) {
        return thumb_image->frames[0].pm;
    }

    return {};
}

void Gallery::pstore_save(const ImageEntryPtr& entry, const Pixmap& thumb) const
{
    std::filesystem::path thumb_path = pstore_path;
    thumb_path.concat(entry->path.string());
    std::filesystem::create_directories(thumb_path.parent_path());
    std::ignore = thumb.save(thumb_path);
}

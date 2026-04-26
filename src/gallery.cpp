// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.hpp"

#include "application.hpp"
#include "imageformat.hpp"
#include "imagelist.hpp"
#include "log.hpp"
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

/**
 * Get default path for persistent storage.
 * @return default path to persistent storage
 */
std::filesystem::path pstore_defpath()
{
    static constexpr std::array env_paths =
        std::to_array<std::pair<const char*, const char*>>({
            { "XDG_CACHE_HOME", "swayimg"        },
            { "HOME",           ".cache/swayimg" }
    });

    for (auto [env_name, postfix] : env_paths) {
        std::filesystem::path path;
        const char* env = std::getenv(env_name);
        if (!env) {
            continue;
        }
        // use only the first directory if prefix is a list
        const char* delim = strchr(env, ':');
        if (!delim) {
            path = env;
        } else {
            path = std::string(env, delim - 1);
        }

        path /= postfix;

        return std::filesystem::absolute(path).lexically_normal();
    }

    return {};
}

Gallery& Gallery::self()
{
    static Gallery singleton;
    return singleton;
}

Gallery::Gallery()
    : tpool(THUMB_LOAD_THREADS)
{
    // default settings

    aspect = Aspect::Fill;
    border_size = 5;
    selected_scale = 1.15;
    pinch_factor = 100.0;

    clr_window = { argb_t::max, 0x00, 0x00, 0x00 };
    clr_background = { argb_t::max, 0x20, 0x20, 0x20 };
    clr_select = { argb_t::max, 0x40, 0x40, 0x40 };
    clr_border = { argb_t::max, 0xaa, 0xaa, 0xaa };

    pstore_enable = false;
    pstore_path = pstore_defpath();

    preload = false;
    cache_size = 100;

    text_scheme[static_cast<size_t>(Text::TopLeft)] = { "File:\t{name}" };
    text_scheme[static_cast<size_t>(Text::TopRight)] = {
        "{list.index} of {list.total}"
    };

    // default key bindings: general management
    bind_input(InputKeyboard { XKB_KEY_Escape, KEYMOD_NONE }, []() {
        Application::self().exit(0);
    });
    bind_input(InputKeyboard { XKB_KEY_Return, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Viewer);
    });
    bind_input(InputKeyboard { XKB_KEY_s, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Slideshow);
    });
    bind_input(InputKeyboard { XKB_KEY_Insert, KEYMOD_NONE }, [this]() {
        const ImageEntryPtr entry = current_entry();
        entry->mark = !entry->mark;
        Application::redraw();
    });
    bind_input(InputKeyboard { XKB_KEY_f, KEYMOD_NONE }, []() {
        Ui* ui = Application::get_ui();
        ui->set_fullscreen(!ui->get_fullscreen());
    });
    bind_input(InputKeyboard { XKB_KEY_a, KEYMOD_NONE }, []() {
        bool& antialiasing = Render::self().antialiasing;
        antialiasing = !antialiasing;
        Application::redraw();
    });
    // scale
    bind_input(InputKeyboard { XKB_KEY_equal, KEYMOD_NONE }, [this]() {
        const size_t size = get_thumb_size();
        set_thumb_size(size + size / 10);
    });
    bind_input(InputKeyboard { XKB_KEY_plus, KEYMOD_SHIFT }, [this]() {
        const size_t size = get_thumb_size();
        set_thumb_size(size + size / 10);
    });
    bind_input(InputKeyboard { XKB_KEY_minus, KEYMOD_NONE }, [this]() {
        const size_t size = get_thumb_size();
        set_thumb_size(size - size / 10);
    });
    // image selection
    bind_input(InputKeyboard { XKB_KEY_Home, KEYMOD_NONE }, [this]() {
        select(Layout::First);
    });
    bind_input(InputKeyboard { XKB_KEY_End, KEYMOD_NONE }, [this]() {
        select(Layout::Last);
    });
    bind_input(InputKeyboard { XKB_KEY_Left, KEYMOD_NONE }, [this]() {
        select(Layout::Left);
    });
    bind_input(InputKeyboard { XKB_KEY_Right, KEYMOD_NONE }, [this]() {
        select(Layout::Right);
    });
    bind_input(InputKeyboard { XKB_KEY_Up, KEYMOD_NONE }, [this]() {
        select(Layout::Up);
    });
    bind_input(InputKeyboard { XKB_KEY_Down, KEYMOD_NONE }, [this]() {
        select(Layout::Down);
    });
    bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_NONE }, [this]() {
        select(Layout::PgDown);
    });
    bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_NONE }, [this]() {
        select(Layout::PgUp);
    });
    // text layer
    bind_input(InputKeyboard { XKB_KEY_t, KEYMOD_NONE }, []() {
        Text& text = Text::self();
        if (text.is_visible()) {
            text.hide();
        } else {
            text.show();
        }
    });
    // mouse
    bind_input(InputMouse { InputMouse::BUTTON_LEFT, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Viewer);
    });
    bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_CTRL }, [this]() {
        const size_t size = get_thumb_size();
        set_thumb_size(size + size / 10);
    });
    bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_CTRL }, [this]() {
        const size_t size = get_thumb_size();
        set_thumb_size(size - size / 10);
    });
    bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_NONE }, [this]() {
        select(Layout::Up);
    });
    bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_NONE }, [this]() {
        select(Layout::Down);
    });
    bind_input(InputMouse { InputMouse::SCROLL_LEFT, KEYMOD_NONE }, [this]() {
        select(Layout::Left);
    });
    bind_input(InputMouse { InputMouse::SCROLL_RIGHT, KEYMOD_NONE }, [this]() {
        select(Layout::Right);
    });
}

bool Gallery::select(const Layout::Direction dir)
{
    assert(is_active());

    if (!layout.select(dir)) {
        return false;
    }
    switch_current();
    return true;
}

void Gallery::reload()
{
    // stop loader
    tpool.cancel();
    tpool.wait();

    // clear cache
    const std::scoped_lock lock(mutex);
    cache.clear();

    Application::redraw();
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
        Application::redraw();
    }
}

void Gallery::set_padding_size(const size_t size)
{
    layout.set_padding(std::min(size, PADDING_SIZE_MAX));
    if (is_active()) {
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

void Gallery::set_cache_size(const size_t size)
{
    cache_size = size;
}

void Gallery::enable_preload(const bool enable)
{
    preload = enable;
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
    layout.set_window_size(wnd);
    layout.select(entry);
    switch_current();
}

void Gallery::deactivate()
{
    tpool.cancel();
    tpool.wait();
}

ImageEntryPtr Gallery::current_entry()
{
    return layout.get_selected();
}

void Gallery::window_resize(const Size& wnd)
{
    layout.set_window_size(wnd);
}

void Gallery::window_redraw(Pixmap& wnd)
{
    layout.update();
    wnd.fill({ 0, 0, wnd.width(), wnd.height() }, clr_window);

    const ImageEntryPtr current = layout.get_selected();

    // draw all exclude the currently selected
    for (const auto& it : layout.get_scheme()) {
        if (it.img != current) {
            draw(it, wnd);
        }
    }

    // draw only currently selected
    for (const auto& it : layout.get_scheme()) {
        if (it.img == current) {
            draw(it, wnd);
            break;
        }
    }

    load_thumbnails();
    clear_thumbnails();
}

void Gallery::handle_mmove(const InputMouse&, const Point& pos, const Point&)
{
    if (layout.select(pos)) {
        switch_current();
    }
}

void Gallery::handle_pinch(const double scale_delta)
{
    set_thumb_size(get_thumb_size() + scale_delta * pinch_factor);
}

void Gallery::handle_imagelist(const ImageListEvent event,
                               const ImageEntryPtr& entry)
{
    if (event == ImageListEvent::Modify || event == ImageListEvent::Remove) {
        // remove entry from cache
        const std::scoped_lock lock(mutex);
        auto it = std::find_if(cache.begin(), cache.end(),
                               [&entry](const ThumbEntry& thumb) {
                                   return entry == thumb.entry;
                               });
        if (it != cache.end()) {
            cache.erase(it);
        }
    }

    if (event == ImageListEvent::Remove && entry == layout.get_selected()) {
        if (!layout.select(Layout::Right) && !layout.select(Layout::Left)) {
            Log::info("No more images to view, exit");
            Application::self().exit(0);
            return;
        }
        switch_current();
    }
    layout.update();
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

void Gallery::load_thumbnails()
{
    const std::scoped_lock lock(mutex);

    tpool.cancel();
    queue.clear();

    ImageList& il = ImageList::self();
    const std::vector<Layout::Thumbnail>& scheme = layout.get_scheme();

    const size_t index_first = scheme.front().img->index;
    const size_t index_last = scheme.back().img->index;

    size_t preload_counter = preload ? cache_size : 0;

    auto check_and_load = [this](ImageEntryPtr& entry) {
        if (!get_thumbnail(entry) && !queue.contains(entry) &&
            !active.contains(entry)) {
            queue.insert(entry);
            tpool.add([this, entry]() {
                load_thumbnail(entry);
            });
        }
    };

    ImageEntryPtr fwd = layout.get_selected();
    ImageEntryPtr back = il.get(fwd, ImageList::Dir::Prev);

    while (fwd || back) {
        if (fwd) {
            check_and_load(fwd);
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
            check_and_load(back);
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

void Gallery::clear_thumbnails()
{
    const std::scoped_lock lock(mutex);

    const std::vector<Layout::Thumbnail>& scheme = layout.get_scheme();

    if (cache.size() > scheme.size() + cache_size) {
        const size_t visible_first = scheme.front().img->index;
        const size_t visible_last = scheme.back().img->index;
        const size_t store_min =
            visible_first > cache_size / 2 ? visible_first - cache_size / 2 : 1;
        const size_t store_max = visible_last + cache_size - cache_size / 2;

        std::erase_if(cache, [store_min, store_max](const ThumbEntry& thumb) {
            return thumb.entry->index < store_min ||
                thumb.entry->index > store_max;
        });
    }
}

const Pixmap* Gallery::get_thumbnail(const ImageEntryPtr& entry)
{
    const auto it = std::find_if(cache.begin(), cache.end(),
                                 [&entry](const ThumbEntry& th) {
                                     return entry == th.entry;
                                 });
    return it == cache.end() ? nullptr : &it->pm;
}

void Gallery::load_thumbnail(const ImageEntryPtr& entry)
{
    {
        const std::scoped_lock lock(mutex);
        active.insert(entry);
        queue.erase(entry);
    }

    const size_t thumb_size = layout.get_thumb_size();

    ThumbEntry thumb;
    thumb.entry = entry;

    if (pstore_enable) {
        thumb.pm = pstore_load(entry);
    }

    if (!thumb.pm) {
        thumb.pm = FormatFactory::self().preview(entry, thumb_size,
                                                 aspect == Aspect::Fill);
        if (!thumb.pm) {
            Application::self().add_event(AppEvent::FileRemove { entry->path });
        } else if (pstore_enable) {
            pstore_save(entry, thumb.pm);
        }
    }

    const std::scoped_lock lock(mutex);

    if (thumb.pm) {
        thumb.entry = entry;
        cache.emplace_back(thumb);
    }
    active.erase(entry);

    Application::redraw();
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

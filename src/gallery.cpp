// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "gallery.hpp"

#include "application.hpp"
#include "imagelist.hpp"
#include "imageloader.hpp"
#include "log.hpp"
#include "render.hpp"
#include "resources.hpp"
#include "text.hpp"

#include <sys/stat.h>
#include <thread>

// Limits for thumbnail size and other parameters
constexpr size_t THIMB_SIZE_MIN = 10;
constexpr size_t THIMB_SIZE_MAX = 10000;
constexpr size_t PADDING_SIZE_MAX = 1000;
constexpr size_t BORDER_SIZE_MAX = 100;
constexpr double SSCALE_MAX = 10.0;

/**
 * Calculate number of threads for loading thumbnails.
 * Audit R4: Scale with CPU cores (capped at 8).
 */
static size_t get_thumb_load_threads()
{
    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 1;  // Fallback if detection fails
    return std::min(8u, cores);  // Cap at 8 to avoid diminishing returns
}

/**
 * Get default path for persistent storage.
 * @return default path to persistent storage
 */
std::filesystem::path pstore_defpath()
{
    const std::pair<const char*, const char*> env_paths[] = {
        { "XDG_CACHE_HOME", "swayimg"        },
        { "HOME",           ".cache/swayimg" }
    };

    for (size_t i = 0; i < sizeof(env_paths) / sizeof(env_paths[0]); ++i) {
        std::filesystem::path path;

        const char* env = env_paths[i].first;
        if (env) {
            env = std::getenv(env);
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
        }
        path /= env_paths[i].second;

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
    : tpool(get_thumb_load_threads())
{
    // default settings

    aspect = Aspect::Fill;
    border_size = 5;
    selected_scale = 1.15;

    clr_window = { argb_t::max, 0x00, 0x00, 0x00 };
    clr_background = { argb_t::max, 0x20, 0x20, 0x20 };
    clr_select = { argb_t::max, 0x40, 0x40, 0x40 };
    clr_border = { argb_t::max, 0xaa, 0xaa, 0xaa };

    pstore_enable = false;
    pstore_path = pstore_defpath();

    preload = false;
    cache_size = 3500;  // Audit R2: increased from 100 to reduce first-scroll jank

    text_scheme[static_cast<size_t>(Text::TopLeft)] = { "File: {name}" };
    text_scheme[static_cast<size_t>(Text::TopRight)] = {
        "{list.index} of {list.total} {list.scanning}"
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
        ImageEntryPtr entry = current_entry();
        entry->mark = !entry->mark;
        Application::redraw();
    });
    bind_input(InputKeyboard { XKB_KEY_f, KEYMOD_NONE }, []() {
        Application::get_ui()->toggle_fullscreen();
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
    bind_input(InputKeyboard { XKB_KEY_i, KEYMOD_NONE }, []() {
        Text::self().show();
    });
    bind_input(InputKeyboard { XKB_KEY_i, KEYMOD_SHIFT }, []() {
        Text::self().hide();
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
    load_thumbnails();
    return true;
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

void Gallery::initialize() { }

void Gallery::activate(const ImageEntryPtr& entry, const Size& wnd)
{
    stopping = false;
    AppMode::activate(entry, wnd);
    layout.set_window_size(wnd);
    layout.select(entry);
    switch_current();
    load_thumbnails();
}

void Gallery::deactivate()
{
    stopping = true;
    tpool.cancel();
    tpool.wait();
    {
        std::lock_guard lock(completed_mutex);
        completed.clear();
        queued.clear();
    }
}

ImageEntryPtr Gallery::current_entry()
{
    return layout.get_selected();
}

void Gallery::window_resize(const Size& wnd)
{
    layout.set_window_size(wnd);
    AppMode::window_resize(wnd);
    load_thumbnails();
}

void Gallery::window_redraw(Pixmap& wnd)
{
    layout.update();
    drain_completed();

    wnd.fill({ 0, 0, wnd.width(), wnd.height() }, clr_window);

    const ImageEntryPtr current = layout.get_selected();

    // draw all exclude the currently selected
    for (auto& it : layout.get_scheme()) {
        if (it.img != current) {
            draw(it, wnd);
        }
    }

    // draw only currently selected
    for (auto& it : layout.get_scheme()) {
        if (it.img == current) {
            draw(it, wnd);
            break;
        }
    }

    clear_thumbnails();
}

#ifdef HAVE_VULKAN
#include "vulkan_pipeline.hpp"
#include "vulkan_texture.hpp"

void Gallery::window_redraw_vk(VkCommandBuffer cmd, TextureCache& texcache)
{
    layout.update();
    drain_completed();

    auto& pipe = VulkanPipeline::self();
    const Size wnd_size = Application::self().get_ui()->get_window_size();

    // Fill window background
    pipe.draw_fill(cmd, 0, 0, static_cast<float>(wnd_size.width),
                   static_cast<float>(wnd_size.height), clr_window);

    const ImageEntryPtr current = layout.get_selected();
    const size_t tile_size = layout.get_thumb_size();

    // Icon textures: create once on first use, then reuse from cache
    constexpr size_t FILE_ICON_KEY = SIZE_MAX - 1;
    constexpr size_t MARK_ICON_KEY = SIZE_MAX - 2;

    // File placeholder icon (white with low alpha) — static, never changes
    GpuTexture* file_icon_tex = texcache.get(FILE_ICON_KEY, 0);
    if (!file_icon_tex) {
        Pixmap file_pm;
        file_pm.create(Pixmap::ARGB, Resource::file.width(),
                       Resource::file.height());
        file_pm.mask(Resource::file, { 0, 0 }, { 0x20, 0xff, 0xff, 0xff });
        file_icon_tex = texcache.get_or_upload(file_pm, FILE_ICON_KEY, 0);
    }

    // Mark icon — static color, create once
    GpuTexture* mark_icon_tex = texcache.get(MARK_ICON_KEY, 0);
    if (!mark_icon_tex) {
        Pixmap mark_pm;
        mark_pm.create(Pixmap::ARGB, Resource::mark.width(),
                       Resource::mark.height());
        mark_pm.mask(Resource::mark, { 0, 0 }, mark_color);
        mark_icon_tex = texcache.get_or_upload(mark_pm, MARK_ICON_KEY, 0);
    }

    // Draw all thumbnails (non-selected first, then selected on top)
    auto draw_thumb = [&](const Layout::Thumbnail& tlay) {
        const bool selected = (tlay.img == current);
        const Pixmap* pm = get_thumbnail(tlay.img);

        // Calculate tile position/size
        Rectangle tile { tlay.pos, { tile_size, tile_size } };
        if (selected) {
            tile.width *= selected_scale;
            tile.height *= selected_scale;
            tile.x -= tile.width / 2 - tile_size / 2;
            tile.y -= tile.height / 2 - tile_size / 2;

            // prevent going beyond the window
            if (tile.x + tile.width + border_size > wnd_size.width) {
                tile.x = wnd_size.width - tile.width - border_size;
            }
            if (tile.x < static_cast<ssize_t>(border_size)) {
                tile.x = border_size;
            }
            if (tile.y + tile.height + border_size > wnd_size.height) {
                tile.y = wnd_size.height - tile.height - border_size;
            }
            if (tile.y < static_cast<ssize_t>(border_size)) {
                tile.y = border_size;
            }
        }

        // Background
        Rectangle bkg = tile;
        pipe.draw_fill(cmd, static_cast<float>(bkg.x),
                       static_cast<float>(bkg.y),
                       static_cast<float>(bkg.width),
                       static_cast<float>(bkg.height),
                       selected ? clr_select : clr_background);

        // Thumbnail image or file placeholder icon
        if (pm) {
            const double scale_w =
                static_cast<double>(tile.width) / pm->width();
            const double scale_h =
                static_cast<double>(tile.height) / pm->height();
            const double sc = aspect == Aspect::Fill
                ? std::max(scale_w, scale_h)
                : std::min(scale_w, scale_h);

            const float img_w = static_cast<float>(sc * pm->width());
            const float img_h = static_cast<float>(sc * pm->height());
            const float img_x = static_cast<float>(tile.x) +
                static_cast<float>(tile.width) / 2.0f - img_w / 2.0f;
            const float img_y = static_cast<float>(tile.y) +
                static_cast<float>(tile.height) / 2.0f - img_h / 2.0f;

            GpuTexture* tex = texcache.get_or_upload(
                *pm, reinterpret_cast<size_t>(tlay.img.get()), 0);
            if (tex) {
                // Clip image to tile boundary (Fill mode may overflow)
                const VkRect2D tile_scissor = {
                    .offset = { static_cast<int32_t>(tile.x),
                                static_cast<int32_t>(tile.y) },
                    .extent = { static_cast<uint32_t>(tile.width),
                                static_cast<uint32_t>(tile.height) },
                };
                vkCmdSetScissor(cmd, 0, 1, &tile_scissor);
                pipe.draw_image(cmd, *tex, img_x, img_y, img_w, img_h);
                // Restore full-window scissor
                const VkRect2D full_scissor = {
                    .offset = { 0, 0 },
                    .extent = { static_cast<uint32_t>(wnd_size.width),
                                static_cast<uint32_t>(wnd_size.height) },
                };
                vkCmdSetScissor(cmd, 0, 1, &full_scissor);
            }
        } else if (file_icon_tex &&
                   bkg.width > Resource::file.width() &&
                   bkg.height > Resource::file.height()) {
            // Draw file placeholder icon centered in tile
            const float ix = static_cast<float>(bkg.x) +
                static_cast<float>(bkg.width) / 2.0f -
                static_cast<float>(Resource::file.width()) / 2.0f;
            const float iy = static_cast<float>(bkg.y) +
                static_cast<float>(bkg.height) / 2.0f -
                static_cast<float>(Resource::file.height()) / 2.0f;
            pipe.draw_image(cmd, *file_icon_tex, ix, iy,
                            static_cast<float>(Resource::file.width()),
                            static_cast<float>(Resource::file.height()));
        }

        // Border for selected
        if (selected && clr_border.a && border_size) {
            const float bx = static_cast<float>(bkg.x - border_size);
            const float by = static_cast<float>(bkg.y - border_size);
            const float bw = static_cast<float>(bkg.width + border_size * 2);
            const float bh = static_cast<float>(bkg.height + border_size * 2);
            const float bs = static_cast<float>(border_size);
            // Top
            pipe.draw_fill(cmd, bx, by, bw, bs, clr_border);
            // Bottom
            pipe.draw_fill(cmd, bx, by + bh - bs, bw, bs, clr_border);
            // Left
            pipe.draw_fill(cmd, bx, by + bs, bs, bh - 2 * bs, clr_border);
            // Right
            pipe.draw_fill(cmd, bx + bw - bs, by + bs, bs, bh - 2 * bs,
                           clr_border);
        }

        // Mark icon
        if (tlay.img->mark && mark_icon_tex) {
            constexpr ssize_t margin = 5;
            const float mx = static_cast<float>(bkg.x) +
                static_cast<float>(bkg.width) -
                static_cast<float>(Resource::mark.width()) -
                static_cast<float>(margin);
            const float my = static_cast<float>(bkg.y) +
                static_cast<float>(bkg.height) -
                static_cast<float>(Resource::mark.height()) -
                static_cast<float>(margin);
            pipe.draw_image(cmd, *mark_icon_tex, mx, my,
                            static_cast<float>(Resource::mark.width()),
                            static_cast<float>(Resource::mark.height()));
        }
    };

    // Non-selected thumbnails first
    for (auto& it : layout.get_scheme()) {
        if (it.img != current) {
            draw_thumb(it);
        }
    }
    // Selected on top
    for (auto& it : layout.get_scheme()) {
        if (it.img == current) {
            draw_thumb(it);
            break;
        }
    }

    clear_thumbnails();
}
#endif // HAVE_VULKAN

void Gallery::handle_mmove(const InputMouse&, const Point& pos, const Point&)
{
    if (layout.select(pos)) {
        switch_current();
        load_thumbnails();
    }
}

void Gallery::handle_pinch(const double scale_delta)
{
    set_thumb_size(get_thumb_size() + scale_delta * 100.0);
}

void Gallery::handle_imagelist(const ImageListEvent event,
                               const ImageEntryPtr& entry)
{
    if (event == ImageListEvent::Modify || event == ImageListEvent::Remove) {
        // remove entry from cache
        std::lock_guard lock(mutex);
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
    load_thumbnails();
    Application::redraw();
}

void Gallery::drain_completed()
{
    std::lock_guard lock(completed_mutex);
    for (auto& thumb : completed) {
        cache.emplace_back(std::move(thumb));
    }
    completed.clear();
}

void Gallery::draw(const Layout::Thumbnail& tlay, Pixmap& wnd)
{
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
        if (tile.x < static_cast<ssize_t>(border_size)) {
            tile.x = border_size;
        }
        if (tile.y + tile.height + border_size > wnd_size.height) {
            tile.y = wnd_size.height - tile.height - border_size;
        }
        if (tile.y < static_cast<ssize_t>(border_size)) {
            tile.y = border_size;
        }
    }

    // draw background
    Rectangle bkg = tile;
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
    // Cancel stale preload tasks from previous position so visible entries
    // at the new position get immediate priority. Active tasks (already
    // running in worker threads) are not affected — they will finish and
    // be drained normally. Only pending queue entries are dropped.
    tpool.cancel();
    drain_completed();

    // Reset queued set. Cached entries are protected by the get_thumbnail()
    // check in check_and_load — they won't be re-queued. A few entries
    // currently being processed by workers may be re-queued, causing minor
    // duplicate work (at most tpool.size() entries).
    {
        std::lock_guard lock(completed_mutex);
        queued.clear();
    }

    ImageList& il = ImageList::self();
    il.ensure_indexed();

    const std::vector<Layout::Thumbnail>& scheme = layout.get_scheme();

    if (scheme.empty()) {
        return;
    }

    const size_t index_first = scheme.front().img->index;
    const size_t index_last = scheme.back().img->index;

    size_t preload_counter = preload ? cache_size : 0;
    // Submit uncached thumbnails. Visible entries use push_front() so they
    // jump ahead of any queued preload tasks.
    // Preload entries use add() (back of queue, low priority).
    auto check_and_load = [this](ImageEntryPtr& entry, bool high_priority) {
        if (get_thumbnail(entry)) {
            return;
        }
        std::lock_guard lock(completed_mutex);
        if (!queued.contains(entry)) {
            queued.insert(entry);
            if (high_priority) {
                tpool.push_front([this, entry]() {
                    load_thumbnail(entry);
                });
            } else {
                tpool.add([this, entry]() {
                    load_thumbnail(entry);
                });
            }
        }
    };

    // Pass 1: visible thumbnails (outward from selected — closest first)
    // Uses high priority (push_front) so they jump ahead of stale preloads.
    ImageEntryPtr fwd = layout.get_selected();
    ImageEntryPtr back = il.get(fwd, ImageList::Dir::Prev);

    while (fwd || back) {
        if (fwd) {
            if (fwd->index > index_last) {
                fwd = nullptr;
            } else {
                check_and_load(fwd, true);
                fwd = il.get(fwd, ImageList::Dir::Next);
            }
        }
        if (back) {
            if (back->index < index_first) {
                back = nullptr;
            } else {
                check_and_load(back, true);
                back = il.get(back, ImageList::Dir::Prev);
            }
        }
    }

    // Pass 2: preload thumbnails beyond visible area (low priority)
    if (preload_counter) {
        fwd = il.get(scheme.back().img, ImageList::Dir::Next);
        back = il.get(scheme.front().img, ImageList::Dir::Prev);

        while ((fwd || back) && preload_counter) {
            if (fwd) {
                check_and_load(fwd, false);
                fwd = il.get(fwd, ImageList::Dir::Next);
                --preload_counter;
                if (!preload_counter) {
                    break;
                }
            }
            if (back) {
                check_and_load(back, false);
                back = il.get(back, ImageList::Dir::Prev);
                --preload_counter;
            }
        }
    }

}

void Gallery::clear_thumbnails()
{
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

        // Also trim queued set so evicted items can be re-loaded if the user
        // scrolls back to them later.
        std::lock_guard lock(completed_mutex);
        std::erase_if(queued, [store_min, store_max](const ImageEntryPtr& e) {
            return e->index < store_min || e->index > store_max;
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
    const size_t thumb_size = layout.get_thumb_size();

    ThumbEntry thumb;
    thumb.entry = entry;

    if (pstore_enable) {
        thumb.pm = pstore_load(entry);
    }

    // Early stopping check before expensive image decode
    if (stopping) {
        std::lock_guard lock(completed_mutex);
        queued.erase(entry);
        return;
    }

    if (!thumb.pm) {
        // create thumbnail from original image
        ImagePtr image = ImageLoader::load(entry);
        if (!image) {
            Application::self().add_event(AppEvent::FileRemove { entry->path });
        } else {
            const Pixmap& origin = image->frames[0].pm;
            if (origin.width() < thumb_size && origin.height() < thumb_size) {
                // original image is too small, use it as thumbnail
                thumb.pm = origin;
            } else {
                // zoom out original image
                const double scale =
                    std::min(static_cast<double>(thumb_size) / origin.width(),
                             static_cast<double>(thumb_size) / origin.height());
                thumb.pm.create(origin.format(), scale * origin.width(),
                                scale * origin.height());
                Render::self().draw(thumb.pm, origin, { 0, 0 }, scale);

                // save thumbnail to persistent storage
                if (pstore_enable) {
                    pstore_save(entry, thumb.pm);
                }
            }
        }
    }

    if (stopping) {
        std::lock_guard lock(completed_mutex);
        queued.erase(entry);
        return;
    }

    {
        std::lock_guard lock(completed_mutex);
        if (thumb.pm) {
            completed.emplace_back(std::move(thumb));
            // Keep entry in queued as "already loaded" marker to prevent
            // re-queueing items that get evicted from cache later.
            // queued is cleaned in clear_thumbnails() and deactivate().
        } else {
            // Remove on failure so it won't block future retries
            queued.erase(entry);
        }
    }

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
    ImageEntryPtr thumb_entry = std::make_shared<ImageEntry>();
    thumb_entry->path = thumb_path;
    ImagePtr thumb_image = ImageLoader::load(thumb_entry);
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
    thumb.save(thumb_path);
}

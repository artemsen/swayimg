// SPDX-License-Identifier: MIT
// Image view mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.hpp"

#include "application.hpp"
#include "imageloader.hpp"
#include "log.hpp"
#include "render.hpp"
#include "resources.hpp"
#include "text.hpp"

#include <cmath>
#include <format>

/** Max scale factor. */
constexpr double MAX_SCALE = 100.0;

Viewer& Viewer::self()
{
    static Viewer singleton;
    return singleton;
}

Viewer::Viewer()
{
    // default settings

    free_move = false;
    imagelist_loop = true;
    scale = 1.0;

    default_scale = Scale::Optimal;
    default_pos = Position::Center;

    window_bkg = argb_t(argb_t::max, 0, 0, 0);

    tr_grid = true;
    tr_grsize = 20;
    tr_grcolor[0] = { argb_t::max, 0x33, 0x33, 0x33 };
    tr_grcolor[1] = { argb_t::max, 0x4c, 0x4c, 0x4c };
    tr_bgcolor = { argb_t::max, 0, 0, 0 };

    animation_enable = true;

    text_scheme[static_cast<size_t>(Text::TopLeft)] = {
        "File: {name}",
        "Format: {format}",
        "File size: {sizehr}",
        "File time: {time}",
        "EXIF date: {meta.Exif.Photo.DateTimeOriginal}",
        "EXIF camera: {meta.Exif.Image.Model}"
    };
    text_scheme[static_cast<size_t>(Text::TopRight)] = {
        "Image: {list.index} of {list.total} {list.scanning}",
        "Frame: {frame.index} of {frame.total}",
        "Size: {frame.width}x{frame.height}"
    };
    text_scheme[static_cast<size_t>(Text::BottomLeft)] = { "Scale: {scale}" };

    // default key bindings: general management
    bind_input(InputKeyboard { XKB_KEY_Escape, KEYMOD_NONE }, []() {
        Application::self().exit(0);
    });
    bind_input(InputKeyboard { XKB_KEY_Return, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Gallery);
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
    // image transform
    bind_input(InputKeyboard { XKB_KEY_bracketleft, KEYMOD_NONE }, [this]() {
        rotate(270);
    });
    bind_input(InputKeyboard { XKB_KEY_bracketright, KEYMOD_NONE }, [this]() {
        rotate(90);
    });
    bind_input(InputKeyboard { XKB_KEY_m, KEYMOD_NONE }, [this]() {
        flip_vertical();
    });
    bind_input(InputKeyboard { XKB_KEY_m, KEYMOD_SHIFT }, [this]() {
        flip_horizontal();
    });
    // text layer
    bind_input(InputKeyboard { XKB_KEY_i, KEYMOD_NONE }, []() {
        Text::self().show();
    });
    bind_input(InputKeyboard { XKB_KEY_i, KEYMOD_SHIFT }, []() {
        Text::self().hide();
    });
    // next/prev image
    bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_NONE }, [this]() {
        open_file(ImageList::Dir::Next);
    });
    bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_NONE }, [this]() {
        open_file(ImageList::Dir::Prev);
    });
    // next/prev frame
    bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_SHIFT }, [this]() {
        animation_stop();
        next_frame();
    });
    bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_SHIFT }, [this]() {
        animation_stop();
        prev_frame();
    });
    // scale
    bind_input(InputKeyboard { XKB_KEY_equal, KEYMOD_NONE }, [this]() {
        set_scale(scale + scale / 10.0);
    });
    bind_input(InputKeyboard { XKB_KEY_plus, KEYMOD_SHIFT }, [this]() {
        set_scale(scale + scale / 10.0);
    });
    bind_input(InputKeyboard { XKB_KEY_minus, KEYMOD_NONE }, [this]() {
        set_scale(scale - scale / 10.0);
    });
    bind_input(InputKeyboard { XKB_KEY_BackSpace, KEYMOD_NONE }, [this]() {
        reset_scale();
    });
    // image position
    bind_input(InputKeyboard { XKB_KEY_Left, KEYMOD_NONE }, [this]() {
        set_position(
            get_position() +
            Point { -static_cast<ssize_t>(window_size.width / 10), 0 });
    });
    bind_input(InputKeyboard { XKB_KEY_Right, KEYMOD_NONE }, [this]() {
        set_position(get_position() +
                     Point { static_cast<ssize_t>(window_size.width / 10), 0 });
    });
    bind_input(InputKeyboard { XKB_KEY_Up, KEYMOD_NONE }, [this]() {
        set_position(
            get_position() +
            Point { 0, -static_cast<ssize_t>(window_size.height / 10) });
    });
    bind_input(InputKeyboard { XKB_KEY_Down, KEYMOD_NONE }, [this]() {
        set_position(
            get_position() +
            Point { 0, static_cast<ssize_t>(window_size.height / 10) });
    });
    // mouse
    bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_NONE }, [this]() {
        set_position(get_position() + Point { 0, -20 });
    });
    bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_NONE }, [this]() {
        set_position(get_position() + Point { 0, 20 });
    });
    bind_input(InputMouse { InputMouse::SCROLL_LEFT, KEYMOD_NONE }, [this]() {
        set_position(get_position() + Point { -20, 0 });
    });
    bind_input(InputMouse { InputMouse::SCROLL_RIGHT, KEYMOD_NONE }, [this]() {
        set_position(get_position() + Point { 20, 0 });
    });
    bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_CTRL }, [this]() {
        set_scale(scale + scale / 10.0,
                  Application::self().get_ui()->get_mouse());
    });
    bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_CTRL }, [this]() {
        set_scale(scale - scale / 10.0,
                  Application::self().get_ui()->get_mouse());
    });

    bind_image_drag(InputMouse { InputMouse::BUTTON_LEFT, KEYMOD_NONE });
}

bool Viewer::open_file(const ImageList::Dir pos, const ImageEntryPtr& from)
{
    ImageList& il = ImageList::self();

    // When async load is pending, compute next from logical nav target
    // (not the still-displayed image) so rapid presses advance properly
    ImageEntryPtr base_entry;
    {
        std::lock_guard lock(image_pool.mutex);
        if (image_pool.pending && !from) {
            base_entry = image_pool.nav_target;
        }
    }
    ImageEntryPtr next_entry =
        from ? from
             : il.get(base_entry ? base_entry : image->entry, pos);

    const bool forward =
        pos != ImageList::Dir::Prev && pos != ImageList::Dir::PrevParent;

    if (!next_entry && !imagelist_loop) {
        return false; // end of list, no looping
    }

    if (!next_entry && imagelist_loop) {
        // reshuffle random on new loop
        if (il.get_order() == ImageList::Order::Random) {
            il.set_order(ImageList::Order::Random);
        }
        next_entry = il.get(
            nullptr, forward ? ImageList::Dir::First : ImageList::Dir::Last);
        if (image && next_entry == image->entry) {
            next_entry = il.get(next_entry, pos);
        }
        if (!next_entry) {
            return false;
        }
    }

    ImagePtr next_image = nullptr;

    {
        // get file from history/preload cache
        std::lock_guard lock(image_pool.mutex);
        next_image = image_pool.preload.get(next_entry);
        if (!next_image) {
            next_image = image_pool.history.get(next_entry);
        }
    }

    if (next_image) {
        // cache hit — switch immediately, cancel any pending async load
        Log::verbose("Get image {} from cache",
                     next_entry->path.filename().string());
        {
            std::lock_guard lock(image_pool.mutex);
            if (image_pool.pending) {
                image_pool.pending = nullptr;
                image_pool.nav_target = nullptr;
            }
            if (image) {
                image_pool.history.put(image);
            }
        }
        image_pool.loading = false;
        image = next_image;
        on_open();
        return true;
    }

    // cache miss
    if (!image) {
        // no current image (startup) — must load synchronously
        while (next_entry && !next_image) {
            next_image = ImageLoader::load(next_entry);
            if (!next_image) {
                next_entry = il.remove(next_entry, forward);
            }
            if (!next_entry && imagelist_loop) {
                next_entry = il.get(nullptr, ImageList::Dir::First);
            }
        }
        if (next_image) {
            image = next_image;
            on_open();
        }
        return !!next_image;
    }

    // load asynchronously — insert at front of queue so this image
    // loads before any pending preload tasks (which remain queued)
    if (image_pool.loading) {
        // re-navigating while a load is pending: cancel queued preload tasks
        // so the new target gets a thread immediately
        image_pool.tpool.cancel();
    }
    {
        std::lock_guard lock(image_pool.mutex);
        image_pool.pending = next_entry;
        image_pool.nav_target = next_entry;
    }
    image_pool.loading = true;

    image_pool.tpool.push_front([this, next_entry, forward]() {
        // check if this task is still wanted (not superseded by rapid nav)
        {
            std::lock_guard lock(image_pool.mutex);
            if (image_pool.pending != next_entry) {
                return; // superseded by newer navigation
            }
        }

        ImageList& il = ImageList::self();
        ImageEntryPtr entry = next_entry;
        ImagePtr loaded = nullptr;

        while (entry && !loaded) {
            if (image_pool.stop) {
                return; // mode switch / shutdown
            }
            // check if still the target before expensive I/O
            {
                std::lock_guard lock(image_pool.mutex);
                if (image_pool.pending != next_entry) {
                    return; // superseded
                }
            }
            loaded = ImageLoader::load(entry);
            if (!loaded) {
                entry = il.remove(entry, forward);
            }
            if (!entry && imagelist_loop) {
                entry = il.get(nullptr, ImageList::Dir::First);
            }
        }

        if (loaded && !image_pool.stop) {
            // deliver result to main thread via event
            Application::self().add_event(
                AppEvent::ImageReady { loaded, entry });
        }
    });

    return true; // async load dispatched, previous image stays displayed
}

size_t Viewer::next_frame()
{
    size_t index = frame_index + 1;
    if (index >= image->frames.size()) {
        index = 0;
    }
    set_frame(index);
    return index;
}

size_t Viewer::prev_frame()
{
    const size_t index =
        frame_index ? frame_index - 1 : image->frames.size() - 1;
    set_frame(index);
    return index;
}

void Viewer::export_frame(const std::filesystem::path& path) const
{
    const Pixmap& pm = image->frames[frame_index].pm;
    pm.save(path);
}

void Viewer::set_scale(const Scale sc)
{
    const Pixmap& pm = image->frames[frame_index].pm;
    const double ratio_w = static_cast<double>(window_size.width) / pm.width();
    const double ratio_h =
        static_cast<double>(window_size.height) / pm.height();

    double abs_sc = 1.0;
    switch (sc) {
        case Scale::Keep:
        case Scale::Optimal:
            abs_sc = std::min(ratio_w, ratio_h);
            if (abs_sc > 1.0) {
                abs_sc = 1.0;
            }
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

void Viewer::reset_scale()
{
    if (std::holds_alternative<Scale>(default_scale)) {
        set_scale(std::get<Scale>(default_scale));
    } else {
        set_scale(std::get<double>(default_scale));
    }
}

void Viewer::set_position(const Position pos)
{
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
    position = pos;
    fixup_position();
}

void Viewer::flip_vertical()
{
    image->flip_vertical();
    Application::redraw();
}

void Viewer::flip_horizontal()
{
    image->flip_horizontal();
    Application::redraw();
}

void Viewer::rotate(const size_t angle)
{
    assert(angle == 90 || angle == 180 || angle == 270);

    image->rotate(angle);

    const Pixmap& pm = image->frames[frame_index].pm;
    const ssize_t diff =
        static_cast<ssize_t>(pm.width()) - static_cast<ssize_t>(pm.height());
    const ssize_t shift = (scale * diff) / 2;
    position.x -= shift;
    position.y += shift;

    update_text(TextUpdate::Frame);

    fixup_position();
}

void Viewer::animation_resume()
{
    animation_enable = true;
    const Image::Frame& frame = image->frames[frame_index];
    if (image->frames.size() > 1 && frame.duration) {
        animation_timer.reset(frame.duration, 0);
    }
}

void Viewer::animation_stop()
{
    animation_enable = false;
    animation_timer.reset(0, 0);
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
    tr_grid = false;
    tr_bgcolor = color;
    if (is_active()) {
        Application::redraw();
    }
}

void Viewer::set_image_grid(const size_t size, const argb_t& color1,
                            const argb_t& color2)
{
    tr_grid = true;
    tr_grsize = size;
    tr_grcolor[0] = color1;
    tr_grcolor[1] = color2;
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
        animation_resume();
    });
}

void Viewer::activate(const ImageEntryPtr& entry, const Size& wnd)
{
    AppMode::activate(entry, wnd);

    window_size = wnd;
    image_pool.stop = false;

    if (!image || image->entry != entry) {
        if (!open_file(ImageList::Dir::Next, entry)) {
            Log::info("No more images to view, exit");
            Application::self().exit(0); // no images
            return;
        }
    } else {
        on_open();
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

void Viewer::set_frame(const size_t index)
{
    assert(index < image->frames.size());

    frame_index = index;
    update_text(TextUpdate::Frame);
    Application::redraw();
}

void Viewer::update_text(const TextUpdate what) const
{
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

void Viewer::on_open()
{
    const bool is_animation =
        image->frames.size() > 1 && image->frames[0].duration;
    frame_index = 0;

    // set content type
    Ui* ui = Application::self().get_ui();
    ui->set_ctype(is_animation ? Ui::ContentType::Animation
                               : Ui::ContentType::Static);

    if (std::holds_alternative<Scale>(default_scale) &&
        std::get<Scale>(default_scale) == Scale::Keep && previmg) {
        // handle "keep scale" mode
        const Pixmap& pm = image->frames[frame_index].pm;
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

    previmg = image->frames[frame_index].pm;

    // start animation
    if (animation_enable && is_animation) {
        animation_resume();
    } else {
        animation_stop();
    }

    preloader_start();

    switch_current();
    update_text(TextUpdate::All);
}

ImageEntryPtr Viewer::current_entry()
{
    return image ? image->entry : nullptr;
}

void Viewer::window_resize(const Size& wnd)
{
    window_size = wnd;
    fixup_position();
    AppMode::window_resize(wnd);
}

void Viewer::window_redraw(Pixmap& wnd)
{
    if (!image) {
        return; // async load in progress, nothing to render yet
    }
    const Pixmap& pm = image->frames[frame_index].pm;
    const Rectangle imgr = { position, static_cast<Size>(pm) * scale };

    // clear image background
    if (pm.format() == Pixmap::ARGB) {
        if (tr_grid) {
            wnd.grid(imgr, tr_grsize, tr_grcolor[0], tr_grcolor[1]);
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

#ifdef HAVE_VULKAN
#include "vulkan_aa.hpp"
#include "vulkan_blur.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_texture.hpp"

void Viewer::pre_render_vk(VkCommandBuffer cmd, TextureCache& texcache)
{
    if (!image || image->frames.empty()) {
        return;
    }

    const Pixmap& pm = image->frames[frame_index].pm;
    GpuTexture* tex = texcache.get_or_upload(
        pm, reinterpret_cast<size_t>(image->entry.get()), frame_index);
    if (!tex) {
        return;
    }

    // Blur background (before render pass)
    if (!std::holds_alternative<argb_t>(window_bkg)) {
        auto& blur = VulkanBlur::self();
        if (blur.is_valid()) {
            blur.prepare(cmd, *tex);
            blur.apply(cmd);
        }
    }

    // MKS13 anti-aliased scaling (before render pass)
    vk_aa_result = {};
    if (Render::self().antialiasing) {
        auto& aa = VulkanAA::self();
        if (aa.is_valid()) {
            const Size wnd =
                Application::self().get_ui()->get_window_size();
            vk_aa_result = aa.apply(cmd, *tex, static_cast<float>(scale),
                                    static_cast<float>(position.x),
                                    static_cast<float>(position.y),
                                    wnd.width, wnd.height);
        }
    }
}

void Viewer::window_redraw_vk(VkCommandBuffer cmd, TextureCache& texcache)
{
    if (!image || image->frames.empty()) {
        return;
    }

    auto& pipe = VulkanPipeline::self();
    const Pixmap& pm = image->frames[frame_index].pm;
    const float img_x = static_cast<float>(position.x);
    const float img_y = static_cast<float>(position.y);
    const float img_w = static_cast<float>(pm.width() * scale);
    const float img_h = static_cast<float>(pm.height() * scale);

    // Draw blur background (must be first — behind everything)
    if (!std::holds_alternative<argb_t>(window_bkg)) {
        auto& blur = VulkanBlur::self();
        if (blur.is_valid()) {
            const auto& result = blur.get_result();
            pipe.draw_image(cmd, result, 0, 0,
                            static_cast<float>(result.width),
                            static_cast<float>(result.height));
        }
    }

    // Clear image background for transparent images
    if (pm.format() == Pixmap::ARGB) {
        if (tr_grid) {
            pipe.draw_grid(cmd, img_x, img_y, img_w, img_h,
                           static_cast<float>(tr_grsize), tr_grcolor[0],
                           tr_grcolor[1]);
        } else {
            pipe.draw_fill(cmd, img_x, img_y, img_w, img_h, tr_bgcolor);
        }
    }

    // Draw image — use AA result if available, otherwise nearest-neighbor
    if (vk_aa_result.texture) {
        pipe.draw_image(cmd, *vk_aa_result.texture, vk_aa_result.x,
                        vk_aa_result.y, vk_aa_result.w, vk_aa_result.h);
    } else {
        GpuTexture* tex = texcache.get_or_upload(
            pm, reinterpret_cast<size_t>(image->entry.get()), frame_index);
        if (tex) {
            pipe.draw_image(cmd, *tex, img_x, img_y, img_w, img_h);
        } else {
            Log::verbose("VRAM exhausted, CPU fallback for current frame");
        }
    }

    // Fill window background (solid color mode only)
    if (std::holds_alternative<argb_t>(window_bkg)) {
        pipe.draw_fill_inverse(cmd, img_x, img_y, img_w, img_h,
                               std::get<argb_t>(window_bkg));
    }
}
#endif // HAVE_VULKAN

void Viewer::handle_mmove(const InputMouse& input, const Point&,
                          const Point& delta)
{
    if (drag && drag == input) {
        set_position(position + delta);
    }
}

void Viewer::handle_pinch(const double scale_delta)
{
    set_scale(scale + scale_delta);
}

void Viewer::handle_imagelist(const ImageListEvent event,
                              const ImageEntryPtr& entry)
{
    if (event == ImageListEvent::Modify || event == ImageListEvent::Remove) {
        // remove entry from cache
        std::lock_guard lock(image_pool.mutex);
        image_pool.history.get(entry);
        image_pool.preload.get(entry);
    }

    switch (event) {
        case ImageListEvent::Create:
            preloader_start();
            break;
        case ImageListEvent::Modify:
            if (entry == image->entry &&
                !open_file(ImageList::Dir::Next, image->entry)) {
                Log::info("No more images to view, exit");
                Application::self().exit(0);
            }
            break;
        case ImageListEvent::Remove:
            if (entry == image->entry) {
                Text::self().set_status("File deleted");
                if (!open_file(ImageList::Dir::Next, nullptr)) {
                    Log::info("No more images to view, exit");
                    Application::self().exit(0);
                }
            }
            break;
    }
}

void Viewer::on_image_ready(const ImagePtr& loaded, const ImageEntryPtr& entry)
{
    // verify this is still the image we want (not superseded by rapid navigation)
    {
        std::lock_guard lock(image_pool.mutex);
        if (image_pool.pending != entry) {
            return; // superseded, discard
        }
        image_pool.pending = nullptr;
        image_pool.nav_target = nullptr;
    }
    image_pool.loading = false;

    // put current image to history
    if (image) {
        std::lock_guard lock(image_pool.mutex);
        image_pool.history.put(image);
    }

    // switch to new image
    image = loaded;
    on_open();
}

void Viewer::preloader_start()
{
    image_pool.tpool.cancel();
    image_pool.stop = false;

    ImageList& il = ImageList::self();
    ImageEntryPtr last_entry = image->entry;

    // collect entries to preload
    std::vector<ImageEntryPtr> to_preload;
    for (size_t i = 0; i < image_pool.preload.capacity; ++i) {
        ImageEntryPtr next_entry = il.get(last_entry, ImageList::Dir::Next);
        if (!next_entry && imagelist_loop) {
            next_entry = il.get(nullptr, ImageList::Dir::First);
        }
        if (!next_entry || next_entry == last_entry) {
            break;
        }

        // skip if already cached
        bool cached = false;
        {
            std::lock_guard lock(image_pool.mutex);
            cached = image_pool.preload.get(next_entry) != nullptr ||
                     image_pool.history.get(next_entry) != nullptr;
        }

        if (!cached) {
            to_preload.push_back(next_entry);
        }
        last_entry = next_entry;
    }

    // dispatch parallel decode tasks
    for (auto& entry : to_preload) {
        image_pool.tpool.add([this, entry]() {
            if (image_pool.stop) {
                return;
            }

            ImagePtr loaded = ImageLoader::load(entry);
            if (image_pool.stop) {
                return;
            }

            if (loaded) {
                Log::verbose("Put image {} to cache",
                             entry->path.filename().string());
                std::lock_guard lock(image_pool.mutex);
                image_pool.preload.put(loaded);
            } else {
                ImageList::self().remove(entry);
            }

            Application::redraw();
        });
    }
}

void Viewer::fixup_position()
{
    const Pixmap& pm = image->frames[frame_index].pm;
    const Size scaled = static_cast<Size>(pm) * scale;

    if (free_move) {
        // don't let canvas to be far out of window
        if (position.x + static_cast<ssize_t>(scaled.width) < 0) {
            position.x = -static_cast<ssize_t>(scaled.width);
        }
        if (position.x > static_cast<ssize_t>(window_size.width)) {
            position.x = window_size.width;
        }
        if (position.y + static_cast<ssize_t>(scaled.height) < 0) {
            position.y = -static_cast<ssize_t>(scaled.height);
        }
        if (position.y > static_cast<ssize_t>(window_size.height)) {
            position.y = window_size.height;
        }
    } else {
        if (scaled.width <= window_size.width) {
            // entire image fits horizontally, center it
            position.x = window_size.width / 2 - scaled.width / 2;
        } else {
            // prevent going outside the window
            const ssize_t right = position.x + scaled.width;
            if (position.x > 0 &&
                right > static_cast<ssize_t>(window_size.width)) {
                position.x = 0;
            }
            if (position.x < 0 &&
                right < static_cast<ssize_t>(window_size.width)) {
                position.x = window_size.width - scaled.width;
            }
        }

        if (scaled.height <= window_size.height) {
            // entire image fits vertically, center it
            position.y = window_size.height / 2 - scaled.height / 2;
        } else {
            // prevent going outside the window
            const ssize_t bottom = position.y + scaled.height;
            if (position.y > 0 &&
                bottom > static_cast<ssize_t>(window_size.height)) {
                position.y = 0;
            }
            if (position.y < 0 &&
                bottom < static_cast<ssize_t>(window_size.height)) {
                position.y = window_size.height - scaled.height;
            }
        }
    }

    Application::redraw();
}

void Viewer::preloader_stop()
{
    image_pool.stop = true;
    image_pool.tpool.cancel();
    // don't wait — running tasks check stop flag and return early
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

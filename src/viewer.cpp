// SPDX-License-Identifier: MIT
// Image view mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.hpp"

#include "application.hpp"
#include "imageloader.hpp"
#include "log.hpp"
#include "render.hpp"

#include <format>

Viewer::Viewer()
{
    // default key bindings
    bind_input(InputKeyboard { XKB_KEY_Escape, KEYMOD_NONE }, []() {
        Application::self().exit(0);
    });
    bind_input(InputKeyboard { XKB_KEY_q, KEYMOD_NONE }, []() {
        Application::self().exit(0);
    });
    bind_input(InputKeyboard { XKB_KEY_equal, KEYMOD_NONE }, [this]() {
        set_scale(cur_scale + cur_scale / 10.0);
    });
    bind_input(InputKeyboard { XKB_KEY_plus, KEYMOD_SHIFT }, [this]() {
        set_scale(cur_scale + cur_scale / 10.0);
    });
    bind_input(InputKeyboard { XKB_KEY_minus, KEYMOD_NONE }, [this]() {
        set_scale(cur_scale - cur_scale / 10.0);
    });
    bind_input(InputKeyboard { XKB_KEY_Left, KEYMOD_NONE }, [this]() {
        move(-static_cast<ssize_t>(wnd_width / 10), 0);
    });
    bind_input(InputKeyboard { XKB_KEY_Right, KEYMOD_NONE }, [this]() {
        move(wnd_width / 10, 0);
    });
    bind_input(InputKeyboard { XKB_KEY_Up, KEYMOD_NONE }, [this]() {
        move(0, -static_cast<ssize_t>(wnd_width / 10));
    });
    bind_input(InputKeyboard { XKB_KEY_Down, KEYMOD_NONE }, [this]() {
        move(0, wnd_width / 10);
    });

    // key bindings: switch file
    bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_NONE }, [this]() {
        open_file(ImageList::Pos::Next);
    });
    bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_NONE }, [this]() {
        open_file(ImageList::Pos::Prev);
    });

    bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_NONE, 0, 0 },
               [this]() {
                   set_scale(cur_scale + cur_scale / 10.0, mouse_x, mouse_y);
               });
    bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_NONE, 0, 0 },
               [this]() {
                   set_scale(cur_scale - cur_scale / 10.0, mouse_x, mouse_y);
               });
    bind_input(InputMouse { InputMouse::BUTTON_LEFT, KEYMOD_NONE, 0, 0 }, []() {
        Application::get_ui()->set_cursor(Ui::CursorShape::Drag);
    });
}

bool Viewer::open_file(const ImageList::Pos pos,
                       const ImageList::EntryPtr& from)
{
    ImageList& imglist = Application::get_imagelist();

    ImageList::EntryPtr next_entry =
        from ? from : imglist.get(image->entry, pos);

    bool forward = true;

    if (!next_entry && imagelist_loop) {
        switch (pos) {
            case ImageList::Pos::Next:
            case ImageList::Pos::NextParent:
                if (imagelist_loop) {
                    next_entry = imglist.get(nullptr, ImageList::Pos::First);
                }
                break;
            case ImageList::Pos::Prev:
            case ImageList::Pos::PrevParent:
                forward = false;
                if (imagelist_loop) {
                    next_entry = imglist.get(nullptr, ImageList::Pos::Last);
                }
                break;
            default:
                break;
        }
        if (!next_entry) {
            return false;
        }
    }

    ImagePtr next_image = nullptr;

    {
        // get file form history/preload cache
        std::lock_guard lock(imp.mutex);
        next_image = imp.preload.get(next_entry);
        if (!next_image) {
            next_image = imp.history.get(next_entry);
        }
    }

    while (next_entry && !next_image) {
        next_image = ImageLoader::load(next_entry);
        if (!next_image) {
            next_entry = imglist.remove(next_entry, forward);
        }
    }
    if (next_image) {
        // put to history
        if (image) {
            std::lock_guard lock(imp.mutex);
            imp.history.put(image);
        }

        // switch to new image
        image = next_image;
        reset();
    }

    return !!next_image;
}

void Viewer::set_frame(const size_t index)
{
    frame_index = index;

    Text& text = Application::get_text();
    text.set_field(Text::FIELD_FRAME_INDEX, std::to_string(frame_index + 1));

    const Pixmap& pm = image->frames[frame_index].pm;
    text.set_field(Text::FIELD_FRAME_WIDTH, std::to_string(pm.width()));
    text.set_field(Text::FIELD_FRAME_HEIGHT, std::to_string(pm.height()));

    text.update();

    Application::self().redraw();
}

void Viewer::set_scale(const Scale scale)
{
    const Pixmap& pm = image->frames[frame_index].pm;
    const double ratio_w = static_cast<double>(wnd_width) / pm.width();
    const double ratio_h = static_cast<double>(wnd_height) / pm.height();

    double factor = 1.0;
    switch (scale) {
        case Scale::Keep:
        case Scale::Optimal:
            factor = std::min(ratio_w, ratio_h);
            if (factor > 1.0) {
                factor = 1.0;
            }
            break;
        case Scale::FitWindow:
            factor = std::min(ratio_w, ratio_h);
            break;
        case Scale::FitWidth:
            factor = ratio_w;
            break;
        case Scale::FitHeight:
            factor = ratio_h;
            break;
        case Scale::FillWindow:
            factor = std::max(ratio_w, ratio_h);
            break;
        case Scale::RealSize:
            factor = 1.0; // 100 %
            break;
    }

    set_scale(factor);
}

void Viewer::set_scale(const double scale, const size_t preserve_x,
                       const size_t preserve_y)
{
    assert(scale > 0);

    const Pixmap& pm = image->frames[frame_index].pm;
    double new_scale = scale;

    // check scale limits
    const size_t img_w = static_cast<size_t>(scale * pm.width());
    const size_t img_h = static_cast<size_t>(scale * pm.height());
    if (scale > 100 && img_w > wnd_width && img_h > wnd_height) {
        new_scale = 100; // too big
    } else if (scale < 1 && (img_w < 1 || img_h < 1)) {
        return; // too small
    }

    // save base point of the image
    const size_t bp_x =
        (preserve_x != std::numeric_limits<size_t>::max() ? preserve_x
                                                          : wnd_width / 2);
    const size_t bp_y =
        (preserve_y != std::numeric_limits<size_t>::max() ? preserve_y
                                                          : wnd_height / 2);
    const double img_bp_x = static_cast<double>(bp_x - img.x) / cur_scale;
    const double img_bp_y = static_cast<double>(bp_y - img.y) / cur_scale;

    // set new scale
    cur_scale = new_scale;

    // restore center
    img.x = bp_x - img_bp_x * cur_scale;
    img.y = bp_y - img_bp_y * cur_scale;

    // update text layer
    Text& text = Application::get_text();
    std::string tscale;
    if (cur_scale >= 0.1) {
        tscale = std::format("{}%", static_cast<size_t>(cur_scale * 100));
    } else {
        tscale = std::format("{:.02}%", cur_scale * 100);
    }
    text.set_field(Text::FIELD_SCALE, tscale);
    text.update();

    fixup_position();
}

void Viewer::move(const Position pos)
{
    const Pixmap& pm = image->frames[frame_index].pm;
    const size_t img_width = cur_scale * pm.width();
    const size_t img_height = cur_scale * pm.height();

    switch (pos) {
        case Position::Center:
            img.x = wnd_width / 2 - img_width / 2;
            img.y = wnd_height / 2 - img_height / 2;
            break;
        case Position::TopCenter:
            img.x = wnd_width / 2 - img_width / 2;
            img.y = 0;
            break;
        case Position::BottomCenter:
            img.x = wnd_width / 2 - img_width / 2;
            img.y = wnd_height - img_height;
            break;
        case Position::LeftCenter:
            img.x = 0;
            img.y = wnd_height / 2 - img_height / 2;
            break;
        case Position::RightCenter:
            img.x = wnd_width - img_width;
            img.y = wnd_height / 2 - img_height / 2;
            break;
        case Position::TopLeft:
            img.x = 0;
            img.y = 0;
            break;
        case Position::TopRight:
            img.x = wnd_width - img_width;
            img.y = 0;
            break;
        case Position::BottomLeft:
            img.x = 0;
            img.y = wnd_height - img_height;
            break;
        case Position::BottomRight:
            img.x = wnd_width - img_width;
            img.y = wnd_height - img_height;
            break;
    }

    Application::self().redraw();
}

void Viewer::move(const ssize_t dx, const ssize_t dy)
{
    img.x += dx;
    img.y += dy;
    fixup_position();
}

void Viewer::flip_vertical()
{
    image->flip_vertical();
    Application::self().redraw();
}

void Viewer::flip_horizontal()
{
    image->flip_horizontal();
    Application::self().redraw();
}

void Viewer::rotate(const size_t angle)
{
    assert(angle == 90 || angle == 180 || angle == 270);

    image->rotate(angle);

    const Pixmap& pm = image->frames[frame_index].pm;
    const ssize_t diff =
        static_cast<ssize_t>(pm.width()) - static_cast<ssize_t>(pm.height());
    const ssize_t shift = (cur_scale * diff) / 2;
    img.x -= shift;
    img.y += shift;

    set_frame(0); // force update frame params in text layer

    fixup_position();
}

void Viewer::animation_resume()
{
    animation_enable = true;
    const Image::Frame& frame = image->frames[frame_index];
    if (image->frames.size() > 1 && frame.duration) {
        animation.reset(frame.duration, 0);
    }
}

void Viewer::animation_stop()
{
    animation_enable = false;
    animation.reset(0, 0);
}

void Viewer::subscribe(const ImageSwitchHadler& handler)
{
    imsw_handlers.push_back(handler);
}

void Viewer::initialize()
{
    Application::self().add_fdpoll(animation, [this]() {
        size_t frame = frame_index + 1;
        if (frame >= image->frames.size()) {
            frame = 0;
        }
        set_frame(frame);
        animation_resume();
    });
    imp.preload.capacity = preload_limit;
    imp.history.capacity = history_limit;
}

void Viewer::activate(ImageList::EntryPtr entry)
{
    Ui* ui = Application::get_ui();
    wnd_width = ui->get_width();
    wnd_height = ui->get_height();

    if (!image || image->entry != entry) {
        if (!open_file(ImageList::Pos::Next, entry)) {
            Log::info("No more images to view, exit");
            Application::self().exit(0); // no images
            return;
        }
    } else {
        reset();
    }
}

void Viewer::deactivate()
{
    preloader_stop();
}

void Viewer::reset()
{
    const bool is_animation =
        image->frames.size() > 1 && image->frames[0].duration;

    // set window title and content type
    Ui* ui = Application::self().get_ui();
    const std::string title =
        std::format("Swayimg: {}", image->entry->path.filename().string());
    ui->set_title(title.c_str());
    ui->set_ctype(is_animation ? Ui::ContentType::Animation
                               : Ui::ContentType::Static);

    // update text layer
    Application::get_text().reset(image);

    set_frame(0);
    set_scale(default_scale);
    move(default_pos);

    // start animation
    if (animation_enable && is_animation) {
        animation_resume();
    } else {
        animation_stop();
    }

    preloader_start();

    // call handlers
    for (auto& it : imsw_handlers) {
        it(image);
    }

    Application::self().redraw();
}

ImageList::EntryPtr Viewer::current_image()
{
    return image->entry;
}

void Viewer::window_resize()
{
    Ui* ui = Application::get_ui();
    wnd_width = ui->get_width();
    wnd_height = ui->get_height();
    fixup_position();
}

void Viewer::window_redraw(Pixmap& wnd)
{
    const Pixmap& pm = image->frames[frame_index].pm;
    const Rectangle imgr = { img, static_cast<Size>(pm) * cur_scale };

    // clear image background
    if (pm.format() == Pixmap::ARGB) {
        if (grid.use) {
            wnd.grid(imgr, grid.size, grid.color[0], grid.color[1]);
        } else {
            wnd.fill(imgr, bkg_transp);
        }
    }

    // put image on window surface
    image->draw(frame_index, wnd, cur_scale, img.x, img.y);

    // fill window background
    switch (bkg_mode) {
        case Background::Solid:
            Render::self().fill_inverse(wnd, imgr, bkg_window);
            break;
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

void Viewer::handle_mmove(const InputMouse& input)
{
    if (input.buttons == InputMouse::BUTTON_LEFT) {
        const ssize_t dx = input.x - static_cast<ssize_t>(mouse_x);
        const ssize_t dy = input.y - static_cast<ssize_t>(mouse_y);
        move(dx, dy);
    }
    mouse_x = input.x;
    mouse_y = input.y;
}

void Viewer::handle_imagelist(const FsMonitor::Event event,
                              const ImageList::EntryPtr& entry)
{
    switch (event) {
        case FsMonitor::Event::Create:
            break;
        case FsMonitor::Event::Modify:
            if (entry == image->entry) {
                open_file(ImageList::Pos::Next, image->entry);
            } else {
                std::lock_guard lock(imp.mutex);
                imp.history.get(entry);
                imp.preload.get(entry);
            }
            break;
        case FsMonitor::Event::Remove:
            if (entry == image->entry) {
                open_file(ImageList::Pos::Next, nullptr);
            } else {
                std::lock_guard lock(imp.mutex);
                imp.history.get(entry);
                imp.preload.get(entry);
            }
            break;
    }
}

void Viewer::preloader_start()
{
    if (imp.thread.joinable()) {
        if (imp.stop) {
            imp.thread.join();
        } else {
            return; // already in progress
        }
    }

    imp.thread = std::thread([this] {
        ImageList& imglist = Application::get_imagelist();
        ImagePtr current_image = image;
        size_t counter = 0;

        while (!imp.stop && counter < preload_limit) {
            if (current_image != image) {
                current_image = image;
                counter = 0; // restart from new position
            }

            ImageList::EntryPtr next_entry =
                imglist.get(image->entry, counter + 1);
            if (!next_entry) {
                std::lock_guard lock(imp.mutex);
                imp.preload.trim(counter);
                break; // last image in the list
            }

            // get existing image form history/preload cache
            {
                std::lock_guard lock(imp.mutex);
                ImagePtr next_image = imp.preload.get(next_entry);
                if (!next_image) {
                    next_image = imp.history.get(next_entry);
                }
                if (next_image) {
                    imp.preload.put(next_image);
                    ++counter;
                    continue;
                }
            }

            // load image
            ImagePtr next_image = ImageLoader::load(next_entry);
            if (next_image) {
                std::lock_guard lock(imp.mutex);
                imp.preload.put(next_image);
                ++counter;
            } else {
                next_entry = imglist.remove(next_entry, true);
            }
        }

        imp.stop = true;
    });
}

void Viewer::fixup_position()
{
    const Pixmap& pm = image->frames[frame_index].pm;
    const size_t img_width = cur_scale * pm.width();
    const size_t img_height = cur_scale * pm.height();

    if (free_move) {
        // don't let canvas to be far out of window
        if (img.x + static_cast<ssize_t>(img_width) < 0) {
            img.x = -img_width;
        }
        if (img.x > static_cast<ssize_t>(wnd_width)) {
            img.x = wnd_width;
        }
        if (img.y + static_cast<ssize_t>(img_height) < 0) {
            img.y = -img_height;
        }
        if (img.y > static_cast<ssize_t>(wnd_height)) {
            img.y = wnd_height;
        }
    } else {
        if (img_width <= wnd_width) {
            // entire image fits horizontally, center it
            img.x = wnd_width / 2 - img_width / 2;
        } else {
            // prevent going outside the window
            const ssize_t right = img.x + img_width;
            if (img.x > 0 && right > static_cast<ssize_t>(wnd_width)) {
                img.x = 0;
            }
            if (img.x < 0 && right < static_cast<ssize_t>(wnd_width)) {
                img.x = wnd_width - img_width;
            }
        }

        if (img_height <= wnd_height) {
            // entire image fits vertically, center it
            img.y = wnd_height / 2 - img_height / 2;
        } else {
            // prevent going outside the window
            const ssize_t bottom = img.y + img_height;
            if (img.y > 0 && bottom > static_cast<ssize_t>(wnd_height)) {
                img.y = 0;
            }
            if (img.y < 0 && bottom < static_cast<ssize_t>(wnd_height)) {
                img.y = wnd_height - img_height;
            }
        }
    }

    Application::self().redraw();
}

void Viewer::preloader_stop()
{
    if (imp.thread.joinable()) {
        imp.stop = true;
        imp.thread.join();
    }
}

void Viewer::Cache::trim(const size_t size)
{
    cache.resize(size);
}

void Viewer::Cache::put(const ImagePtr& image)
{
    if (cache.size() == capacity) {
        cache.pop_back();
    }
    cache.push_front(image);
}

ImagePtr Viewer::Cache::get(const ImageList::EntryPtr& entry)
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

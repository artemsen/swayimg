// SPDX-License-Identifier: MIT
// Default configuration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "defaults.hpp"

#include "application.hpp"
#include "render.hpp"

#include <cstdlib>
#include <cstring>

void Defaults::viewer::bind_inputs(Viewer* mode)
{
    // general management
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Escape, .mods = KEYMOD_NONE }, []() {
            Application::self().exit(0);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Return, .mods = KEYMOD_NONE }, []() {
            Application::self().set_mode(AppMode::Gallery);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Enter, .mods = KEYMOD_NONE }, []() {
            Application::self().set_mode(AppMode::Gallery);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_s, .mods = KEYMOD_NONE },
                     []() {
                         Application::self().set_mode(AppMode::Slideshow);
                         Text::self().set_status("Slide show started");
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Insert, .mods = KEYMOD_NONE }, [mode]() {
            mode->mark_current(std::nullopt);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Delete, .mods = KEYMOD_NONE }, [mode]() {
            const ImageEntryPtr entry = mode->get_current();
            if (entry) {
                Application::self().remove_images({ entry->path });
            }
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_f, .mods = KEYMOD_NONE },
                     []() {
                         Ui* ui = Application::get_ui();
                         ui->set_fullscreen(!ui->get_fullscreen());
                     });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_a, .mods = KEYMOD_NONE },
                     []() {
                         bool& antialiasing = Render::self().antialiasing;
                         antialiasing = !antialiasing;
                         Application::redraw();
                     });

    // image transform
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_bracketleft, .mods = KEYMOD_NONE },
        [mode]() {
            mode->rotate(270);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_bracketright, .mods = KEYMOD_NONE },
        [mode]() {
            mode->rotate(90);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_m, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->flip_vertical();
                     });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_m, .mods = KEYMOD_SHIFT },
                     [mode]() {
                         mode->flip_horizontal();
                     });

    // text layer
    mode->bind_input(InputKeyboard { .key = XKB_KEY_t, .mods = KEYMOD_NONE },
                     []() {
                         Text& text = Text::self();
                         if (text.is_visible()) {
                             text.hide();
                         } else {
                             text.show();
                         }
                     });

    // next/prev image
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Next, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->open(ImageList::Dir::Next);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Next, .mods = KEYMOD_NONE },
        [mode]() {
            mode->open(ImageList::Dir::Next);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Prior, .mods = KEYMOD_NONE }, [mode]() {
            mode->open(ImageList::Dir::Prev);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Prior, .mods = KEYMOD_NONE },
        [mode]() {
            mode->open(ImageList::Dir::Prev);
        });
    // next/prev frame
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Next, .mods = KEYMOD_SHIFT }, [mode]() {
            mode->enable_animation(false);
            mode->next_frame();
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Next, .mods = KEYMOD_SHIFT },
        [mode]() {
            mode->enable_animation(false);
            mode->next_frame();
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Prior, .mods = KEYMOD_SHIFT }, [mode]() {
            mode->enable_animation(false);
            mode->prev_frame();
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Prior, .mods = KEYMOD_SHIFT },
        [mode]() {
            mode->enable_animation(false);
            mode->prev_frame();
        });

    // scale
    const auto zoom_fn = [mode](const double factor,
                                const Point& pos = Point()) {
        const double scale = mode->get_scale();
        mode->set_scale(scale + scale / factor, pos);
    };
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_equal, .mods = KEYMOD_NONE },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_plus, .mods = KEYMOD_SHIFT },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Add, .mods = KEYMOD_SHIFT },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_minus, .mods = KEYMOD_NONE },
        [zoom_fn]() {
            zoom_fn(-10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Subtract, .mods = KEYMOD_NONE },
        [zoom_fn]() {
            zoom_fn(-10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_BackSpace, .mods = KEYMOD_NONE },
        [mode]() {
            mode->reset();
        });

    // image position
    const auto move_fn = [mode](const Point& dir) {
        const Size wnd = Application::get_ui()->get_window_size();
        Point pos = mode->get_position();
        if (dir.x) {
            pos.x += static_cast<ssize_t>(wnd.width) / dir.x;
        }
        if (dir.y) {
            pos.y += static_cast<ssize_t>(wnd.height) / dir.y;
        }
        mode->set_position(pos);
    };
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Left, .mods = KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ .x = 10, .y = 0 });
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Left, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = 10, .y = 0 });
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Right, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = -10, .y = 0 });
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Right, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = -10, .y = 0 });
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Up, .mods = KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ .x = 0, .y = 10 });
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Up, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = 0, .y = 10 });
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Down, .mods = KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ .x = 0, .y = -10 });
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Down, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = 0, .y = -10 });
        });

    // mouse
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_UP, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = 0, .y = -10 });
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_DOWN, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = 0, .y = 10 });
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_LEFT, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = -10, .y = 0 });
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_RIGHT, .mods = KEYMOD_NONE },
        [move_fn]() {
            move_fn({ .x = 10, .y = 0 });
        });

    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_UP, .mods = KEYMOD_CTRL },
        [zoom_fn]() {
            zoom_fn(10, Application::get_ui()->get_mouse());
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_DOWN, .mods = KEYMOD_CTRL },
        [zoom_fn]() {
            zoom_fn(-10, Application::get_ui()->get_mouse());
        });

    mode->bind_image_drag(
        InputMouse { .buttons = InputMouse::BUTTON_LEFT, .mods = KEYMOD_NONE });
}

void Defaults::slideshow::bind_inputs(Slideshow* mode)
{
    Defaults::viewer::bind_inputs(mode);

    mode->bind_input(InputKeyboard { .key = XKB_KEY_s, .mods = KEYMOD_NONE },
                     []() {
                         Application::self().set_mode(AppMode::Viewer);
                         Text::self().set_status("Slide show stopped");
                     });
}

void Defaults::gallery::bind_inputs(Gallery* mode)
{
    // general management
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Escape, .mods = KEYMOD_NONE }, []() {
            Application::self().exit(0);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Return, .mods = KEYMOD_NONE }, []() {
            Application::self().set_mode(AppMode::Viewer);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Enter, .mods = KEYMOD_NONE }, []() {
            Application::self().set_mode(AppMode::Viewer);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_s, .mods = KEYMOD_NONE },
                     []() {
                         Application::self().set_mode(AppMode::Slideshow);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Insert, .mods = KEYMOD_NONE }, [mode]() {
            mode->mark_current(std::nullopt);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Delete, .mods = KEYMOD_NONE }, [mode]() {
            const ImageEntryPtr entry = mode->get_current();
            if (entry) {
                Application::self().remove_images({ entry->path });
            }
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_f, .mods = KEYMOD_NONE },
                     []() {
                         Ui* ui = Application::get_ui();
                         ui->set_fullscreen(!ui->get_fullscreen());
                     });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_a, .mods = KEYMOD_NONE },
                     []() {
                         bool& antialiasing = Render::self().antialiasing;
                         antialiasing = !antialiasing;
                         Application::redraw();
                     });

    // scale
    const auto zoom_fn = [mode](const ssize_t factor) {
        const size_t size = mode->get_thumb_size();
        mode->set_thumb_size(size + static_cast<ssize_t>(size) / factor);
    };
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_equal, .mods = KEYMOD_NONE },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_plus, .mods = KEYMOD_SHIFT },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Add, .mods = KEYMOD_SHIFT },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_minus, .mods = KEYMOD_NONE },
        [zoom_fn]() {
            zoom_fn(-10);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Subtract, .mods = KEYMOD_NONE },
        [zoom_fn]() {
            zoom_fn(-10);
        });

    // image selection
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Home, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::First);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Home, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::First);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_End, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Last);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_End, .mods = KEYMOD_NONE }, [mode]() {
            mode->select(Layout::Last);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Left, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Left);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Left, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Left);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Right, .mods = KEYMOD_NONE }, [mode]() {
            mode->select(Layout::Right);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Right, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Right);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Up, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Up);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Up, .mods = KEYMOD_NONE }, [mode]() {
            mode->select(Layout::Up);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Down, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Down);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Down, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Down);
        });
    mode->bind_input(InputKeyboard { .key = XKB_KEY_Next, .mods = KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::PgDown);
                     });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Next, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::PgDown);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_Prior, .mods = KEYMOD_NONE }, [mode]() {
            mode->select(Layout::PgUp);
        });
    mode->bind_input(
        InputKeyboard { .key = XKB_KEY_KP_Prior, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::PgUp);
        });

    // text layer
    mode->bind_input(InputKeyboard { .key = XKB_KEY_t, .mods = KEYMOD_NONE },
                     []() {
                         Text& text = Text::self();
                         if (text.is_visible()) {
                             text.hide();
                         } else {
                             text.show();
                         }
                     });

    // mouse
    mode->bind_input(
        InputMouse { .buttons = InputMouse::BUTTON_LEFT, .mods = KEYMOD_NONE },
        [mode]() {
            const Point pos = Application::get_ui()->get_mouse();
            mode->select(pos);
            Application::self().set_mode(AppMode::Viewer);
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_UP, .mods = KEYMOD_CTRL },
        [zoom_fn]() {
            zoom_fn(10);
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_DOWN, .mods = KEYMOD_CTRL },
        [zoom_fn]() {
            zoom_fn(-10);
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_UP, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Up);
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_DOWN, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Down);
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_LEFT, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Left);
        });
    mode->bind_input(
        InputMouse { .buttons = InputMouse::SCROLL_RIGHT, .mods = KEYMOD_NONE },
        [mode]() {
            mode->select(Layout::Right);
        });
}

std::filesystem::path Defaults::gallery::pstore_path()
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
            path = std::string(env, delim);
        }

        path /= postfix;

        return std::filesystem::absolute(path).lexically_normal();
    }

    return {};
}

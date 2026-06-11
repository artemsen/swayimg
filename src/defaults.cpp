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
    mode->bind_input(InputKeyboard { XKB_KEY_Escape, KEYMOD_NONE }, []() {
        Application::self().exit(0);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Return, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Gallery);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_s, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Slideshow);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Insert, KEYMOD_NONE }, [mode]() {
        mode->mark_current(std::nullopt);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_f, KEYMOD_NONE }, []() {
        Ui* ui = Application::get_ui();
        ui->set_fullscreen(!ui->get_fullscreen());
    });
    mode->bind_input(InputKeyboard { XKB_KEY_a, KEYMOD_NONE }, []() {
        bool& antialiasing = Render::self().antialiasing;
        antialiasing = !antialiasing;
        Application::redraw();
    });

    // image transform
    mode->bind_input(InputKeyboard { XKB_KEY_bracketleft, KEYMOD_NONE },
                     [mode]() {
                         mode->rotate(270);
                     });
    mode->bind_input(InputKeyboard { XKB_KEY_bracketright, KEYMOD_NONE },
                     [mode]() {
                         mode->rotate(90);
                     });
    mode->bind_input(InputKeyboard { XKB_KEY_m, KEYMOD_NONE }, [mode]() {
        mode->flip_vertical();
    });
    mode->bind_input(InputKeyboard { XKB_KEY_m, KEYMOD_SHIFT }, [mode]() {
        mode->flip_horizontal();
    });

    // text layer
    mode->bind_input(InputKeyboard { XKB_KEY_t, KEYMOD_NONE }, []() {
        Text& text = Text::self();
        if (text.is_visible()) {
            text.hide();
        } else {
            text.show();
        }
    });

    // next/prev image
    mode->bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_NONE }, [mode]() {
        mode->open(ImageList::Dir::Next);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_NONE }, [mode]() {
        mode->open(ImageList::Dir::Prev);
    });
    // next/prev frame
    mode->bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_SHIFT }, [mode]() {
        mode->enable_animation(false);
        mode->next_frame();
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_SHIFT }, [mode]() {
        mode->enable_animation(false);
        mode->prev_frame();
    });

    // scale
    const auto zoom_fn = [mode](const double factor,
                                const Point& pos = Point()) {
        const double scale = mode->get_scale();
        mode->set_scale(scale + scale / factor, pos);
    };
    mode->bind_input(InputKeyboard { XKB_KEY_equal, KEYMOD_NONE }, [zoom_fn]() {
        zoom_fn(10);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_plus, KEYMOD_SHIFT }, [zoom_fn]() {
        zoom_fn(10);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_minus, KEYMOD_NONE }, [zoom_fn]() {
        zoom_fn(-10);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_BackSpace, KEYMOD_NONE },
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
    mode->bind_input(InputKeyboard { XKB_KEY_Left, KEYMOD_NONE }, [move_fn]() {
        move_fn({ 10, 0 });
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Right, KEYMOD_NONE }, [move_fn]() {
        move_fn({ -10, 0 });
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Up, KEYMOD_NONE }, [move_fn]() {
        move_fn({ 0, 10 });
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Down, KEYMOD_NONE }, [move_fn]() {
        move_fn({ 0, -10 });
    });

    // mouse
    mode->bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ 0, -10 });
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ 0, 10 });
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_LEFT, KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ -10, 0 });
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_RIGHT, KEYMOD_NONE },
                     [move_fn]() {
                         move_fn({ 10, 0 });
                     });

    mode->bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_CTRL },
                     [zoom_fn]() {
                         zoom_fn(10, Application::self().get_ui()->get_mouse());
                     });
    mode->bind_input(
        InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_CTRL }, [zoom_fn]() {
            zoom_fn(-10, Application::self().get_ui()->get_mouse());
        });

    mode->bind_image_drag(InputMouse { InputMouse::BUTTON_LEFT, KEYMOD_NONE });
}

void Defaults::slideshow::bind_inputs(Slideshow* mode)
{
    Defaults::viewer::bind_inputs(mode);

    mode->bind_input(InputKeyboard { XKB_KEY_s, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Viewer);
    });
}

void Defaults::gallery::bind_inputs(Gallery* mode)
{
    // general management
    mode->bind_input(InputKeyboard { XKB_KEY_Escape, KEYMOD_NONE }, []() {
        Application::self().exit(0);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Return, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Viewer);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_s, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Slideshow);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Insert, KEYMOD_NONE }, [mode]() {
        mode->mark_current(std::nullopt);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_f, KEYMOD_NONE }, []() {
        Ui* ui = Application::get_ui();
        ui->set_fullscreen(!ui->get_fullscreen());
    });
    mode->bind_input(InputKeyboard { XKB_KEY_a, KEYMOD_NONE }, []() {
        bool& antialiasing = Render::self().antialiasing;
        antialiasing = !antialiasing;
        Application::redraw();
    });

    // scale
    const auto zoom_fn = [mode](const ssize_t factor) {
        const size_t size = mode->get_thumb_size();
        mode->set_thumb_size(size + static_cast<ssize_t>(size) / factor);
    };
    mode->bind_input(InputKeyboard { XKB_KEY_equal, KEYMOD_NONE }, [zoom_fn]() {
        zoom_fn(10);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_plus, KEYMOD_SHIFT }, [zoom_fn]() {
        zoom_fn(10);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_minus, KEYMOD_NONE }, [zoom_fn]() {
        zoom_fn(-10);
    });

    // image selection
    mode->bind_input(InputKeyboard { XKB_KEY_Home, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::First);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_End, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::Last);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Left, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::Left);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Right, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::Right);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Up, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::Up);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Down, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::Down);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Next, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::PgDown);
    });
    mode->bind_input(InputKeyboard { XKB_KEY_Prior, KEYMOD_NONE }, [mode]() {
        mode->select(Layout::PgUp);
    });

    // text layer
    mode->bind_input(InputKeyboard { XKB_KEY_t, KEYMOD_NONE }, []() {
        Text& text = Text::self();
        if (text.is_visible()) {
            text.hide();
        } else {
            text.show();
        }
    });

    // mouse
    mode->bind_input(InputMouse { InputMouse::BUTTON_LEFT, KEYMOD_NONE }, []() {
        Application::self().set_mode(Application::Mode::Viewer);
    });
    mode->bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_CTRL },
                     [zoom_fn]() {
                         zoom_fn(10);
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_CTRL },
                     [zoom_fn]() {
                         zoom_fn(-10);
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_UP, KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Up);
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_DOWN, KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Down);
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_LEFT, KEYMOD_NONE },
                     [mode]() {
                         mode->select(Layout::Left);
                     });
    mode->bind_input(InputMouse { InputMouse::SCROLL_RIGHT, KEYMOD_NONE },
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
            path = std::string(env, delim - 1);
        }

        path /= postfix;

        return std::filesystem::absolute(path).lexically_normal();
    }

    return {};
}

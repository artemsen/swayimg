// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luaengine.hpp"

#include "application.hpp"
#include "gallery.hpp"
#include "imageformat.hpp"
#include "imagelist.hpp"
#include "log.hpp"
#include "render.hpp"
#include "slideshow.hpp"
#include "text.hpp"
#include "viewer.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>

// Namespaces
constexpr const char* NS_SWAYIMG = "swayimg";
constexpr const char* NS_TEXT = "text";
constexpr const char* NS_IMAGELIST = "imagelist";
constexpr const char* NS_VIEWER = "viewer";
constexpr const char* NS_SLIDESHOW = "slideshow";
constexpr const char* NS_GALLERY = "gallery";

// app modes table: type to name
static constexpr std::array appmodes =
    std::to_array<std::pair<Application::Mode, const char*>>({
        { Application::Mode::Viewer,    "viewer"    },
        { Application::Mode::Slideshow, "slideshow" },
        { Application::Mode::Gallery,   "gallery"   },
});

// ilage list orders: type to name
static constexpr std::array ilorders =
    std::to_array<std::pair<ImageList::Order, const char*>>({
        { ImageList::Order::None,    "none"    },
        { ImageList::Order::Alpha,   "alpha"   },
        { ImageList::Order::Numeric, "numeric" },
        { ImageList::Order::Mtime,   "mtime"   },
        { ImageList::Order::Size,    "size"    },
        { ImageList::Order::Random,  "random"  },
});

// image list directions: type to name
static constexpr std::array ildirs =
    std::to_array<std::pair<ImageList::Dir, const char*>>({
        { ImageList::Dir::First,      "first"    },
        { ImageList::Dir::Last,       "last"     },
        { ImageList::Dir::Next,       "next"     },
        { ImageList::Dir::Prev,       "prev"     },
        { ImageList::Dir::NextParent, "next_dir" },
        { ImageList::Dir::PrevParent, "prev_dir" },
        { ImageList::Dir::Random,     "random"   },
});

// text block position: type to name
static constexpr std::array tbpositions =
    std::to_array<std::pair<Text::Position, const char*>>({
        { Text::Position::TopLeft,     "topleft"     },
        { Text::Position::TopRight,    "topright"    },
        { Text::Position::BottomLeft,  "bottomleft"  },
        { Text::Position::BottomRight, "bottomright" },
});

// fixed scale: type to name
static constexpr std::array scales =
    std::to_array<std::pair<Viewer::Scale, const char*>>({
        { Viewer::Scale::Optimal,    "optimal" },
        { Viewer::Scale::FitWindow,  "fit"     },
        { Viewer::Scale::FitWidth,   "width"   },
        { Viewer::Scale::FitHeight,  "height"  },
        { Viewer::Scale::FillWindow, "fill"    },
        { Viewer::Scale::RealSize,   "real"    },
        { Viewer::Scale::Keep,       "keep"    },
});

// fixed position: type to name
static constexpr std::array imgpositions =
    std::to_array<std::pair<Viewer::Position, const char*>>({
        { Viewer::Position::Center,       "center"       },
        { Viewer::Position::TopCenter,    "topcenter"    },
        { Viewer::Position::BottomCenter, "bottomcenter" },
        { Viewer::Position::LeftCenter,   "leftcenter"   },
        { Viewer::Position::RightCenter,  "rightcenter"  },
        { Viewer::Position::TopLeft,      "topleft"      },
        { Viewer::Position::TopRight,     "topright"     },
        { Viewer::Position::BottomLeft,   "bottomleft"   },
        { Viewer::Position::BottomRight,  "bottomright"  },
});

// window background mode: type to name
static constexpr std::array wndbkgs =
    std::to_array<std::pair<Viewer::Background, const char*>>({
        { Viewer::Background::Mirror, "mirror" },
        { Viewer::Background::Extend, "extend" },
        { Viewer::Background::Auto,   "auto"   },
});

// set current selection in gallery: type to name
static constexpr std::array gldirs =
    std::to_array<std::pair<Layout::Direction, const char*>>({
        { Layout::Direction::First,  "first"  },
        { Layout::Direction::Last,   "last"   },
        { Layout::Direction::Up,     "up"     },
        { Layout::Direction::Down,   "down"   },
        { Layout::Direction::Left,   "left"   },
        { Layout::Direction::Right,  "right"  },
        { Layout::Direction::PgUp,   "pgup"   },
        { Layout::Direction::PgDown, "pgdown" },
});

// thumnail aspect ratio: type to name
static constexpr std::array aspects =
    std::to_array<std::pair<Gallery::Aspect, const char*>>({
        { Gallery::Aspect::Fit,  "fit"  },
        { Gallery::Aspect::Fill, "fill" },
        { Gallery::Aspect::Keep, "keep" },
});

/**
 * Get type from name.
 * @param arr table with all possible pairs type->name
 * @param name name to convert
 * @return type or nullopt if name not found
 */
template <typename T, size_t N>
static std::optional<T>
name_to_type(const std::array<std::pair<T, const char*>, N>& arr,
             const char* name)
{
    for (const auto& it : arr) {
        if (strcmp(it.second, name) == 0) {
            return it.first;
        }
    }
    return std::nullopt;
}

/**
 * Get name from type.
 * @param arr table with all possible pairs type->name
 * @param type type to convert
 * @return name
 */
template <typename T, size_t N>
static const char*
type_to_name(const std::array<std::pair<T, const char*>, N>& arr, const T& type)
{
    for (const auto& it : arr) {
        if (it.first == type) {
            return it.second;
        }
    }
    assert(false && "unreachable");
    return nullptr;
}

/**
 * Get path to config file (init.lua).
 * @return path to initial config file
 */
static std::filesystem::path get_config_file()
{
    static constexpr std::array env_paths =
        std::to_array<std::pair<const char*, const char*>>({
            { "XDG_CONFIG_HOME", "swayimg"          },
            { "XDG_CONFIG_DIRS", "swayimg"          },
            { "HOME",            ".config/swayimg"  },
            { nullptr,           "/etc/xdg/swayimg" }
    });

    for (auto [env_name, postfix] : env_paths) {
        std::filesystem::path path;

        if (env_name) {
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
        }
        path /= postfix;
        path /= "init.lua";

        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).lexically_normal();
        }
    }

    return {};
}

LuaEngine& LuaEngine::self()
{
    static LuaEngine singleton;
    return singleton;
}

LuaEngine::~LuaEngine()
{
    if (lua_state) {
        for (auto& it : refs) {
            delete it;
        }
        lua_close(lua_state);
    }
}

void LuaEngine::initialize(const std::filesystem::path& config)
{
    // initialize lua
    lua_state = luaL_newstate();
    if (!lua_state) {
        Log::error("Unable to initialize Lua state");
        return;
    }
    luaL_openlibs(lua_state);

    const std::filesystem::path config_file =
        config.empty() ? get_config_file() : config;
    if (config_file.empty()) {
        Log::verbose("User config not found, use default settings");
    } else {
        Log::verbose("Load user config from {}", config_file.c_str());
    }

    // add config dir to lua runtime path
    if (!config_file.empty()) {
        lua_getglobal(lua_state, "package");
        lua_getfield(lua_state, -1, "path");
        std::string pack_path = lua_tostring(lua_state, -1);
        lua_pop(lua_state, 2);
        pack_path += ";" + config_file.parent_path().string() + "/?.lua";
        lua_getglobal(lua_state, "package");
        lua_pushstring(lua_state, pack_path.c_str());
        lua_setfield(lua_state, -2, "path");
        lua_pop(lua_state, 1); // Pop package table
    }

    // register lua bindings
    bind_root_api();
    bind_imagelist_api();
    bind_text_api();
    bind_viewer_api(NS_VIEWER);
    bind_slideshow_api();
    bind_gallery_api();

    // load config file
    if (!config_file.empty()) {
        if (luaL_loadfile(lua_state, config_file.c_str()) != LUA_OK) {
            print_error("Failed to load config file: {}",
                        lua_tostring(lua_state, -1));
        } else if (lua_pcall(lua_state, 0, 0, 0) != LUA_OK) {
            print_error("Failed to execute config file: {}",
                        lua_tostring(lua_state, -1));
        }
    }
}

void LuaEngine::execute(const std::string& script)
{
    assert(lua_state);

    if (luaL_loadstring(lua_state, script.c_str()) != LUA_OK) {
        print_error("Failed to load script line: {}",
                    lua_tostring(lua_state, -1));
    } else if (lua_pcall(lua_state, 0, 0, 0) != LUA_OK) {
        print_error("Failed to execute script line: {}",
                    lua_tostring(lua_state, -1));
    }
}

void LuaEngine::bind_root_api()
{
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .addFunction("exit",
                     [](const std::optional<int>& code) {
                         Application::self().exit(code ? *code : 0);
                     })
        .addFunction("set_mode",
                     [this](const std::string& name) {
                         const auto mode = name_to_type(appmodes, name.c_str());
                         if (mode.has_value()) {
                             Application::self().set_mode(mode.value());
                         } else {
                             print_error("Invalid mode: {}", name);
                         }
                     })
        .addFunction("get_mode",
                     []() {
                         return type_to_name(appmodes,
                                             Application::self().get_mode());
                     })
        .addFunction("set_title",
                     [](const std::string& title) {
                         Ui* ui = Application::get_ui();
                         if (ui) {
                             ui->set_title(title.c_str());
                         }
                     })
        .addFunction("get_window_size",
                     []() {
                         Size wnd { 0, 0 };
                         Ui* ui = Application::get_ui();
                         if (ui) {
                             wnd = ui->get_window_size();
                         }
                         return std::unordered_map<std::string, size_t> {
                             { "width",  wnd.width  },
                             { "height", wnd.height },
                         };
                     })
        .addFunction(
            "set_window_size",
            [this](const size_t width, const size_t height) {
                if (!width || !height || width > 32767 || height > 32767) {
                    print_error(
                        "Invalid arguments ({}, {}) for {} .set_window_size ",
                        width, height, NS_SWAYIMG);
                    return;
                }
                Ui* ui = Application::get_ui();
                if (ui) {
                    ui->set_window_size({ width, height });
                } else {
                    Rectangle& wnd = Application::self().sparams.window;
                    if (!static_cast<Size>(wnd)) {
                        wnd.width = width;
                        wnd.height = height;
                    }
                }
            })
        .addFunction(
            "on_window_resize",
            [this](const luabridge::LuaRef& cb) {
                if (!cb.isFunction()) {
                    print_error("Invalid argument for {}.on_window_resize: "
                                "expected function, but got {}",
                                NS_SWAYIMG, cb.tostring().c_str());
                } else {
                    const luabridge::LuaRef* ref = add_ref(&cb);
                    Application::self().subscribe_window_resize([this, ref]() {
                        const luabridge::LuaResult result = (*ref)();
                        if (!result) {
                            print_error("{}", result.errorMessage());
                        }
                    });
                }
            })
        .addFunction("get_mouse_pos",
                     []() {
                         Point pos { 0, 0 };
                         Ui* ui = Application::get_ui();
                         if (ui) {
                             pos = ui->get_mouse();
                         }
                         return std::unordered_map<std::string, ssize_t> {
                             { "x", pos.x },
                             { "y", pos.y },
                         };
                     })
        .addFunction("set_fullscreen",
                     [](const std::optional<bool>& enable) {
                         Ui* ui = Application::get_ui();
                         if (ui) {
                             ui->set_fullscreen(
                                 enable.value_or(!ui->get_fullscreen()));
                         } else {
                             std::optional<bool>& fullscreen =
                                 Application::self().sparams.fullscreen;
                             if (!fullscreen.has_value()) {
                                 fullscreen = enable.value_or(true);
                             }
                         }
                     })
        .addFunction("get_fullscreen",
                     []() {
                         const Ui* ui = Application::get_ui();
                         if (ui) {
                             return ui->get_fullscreen();
                         } else {
                             const std::optional<bool>& fullscreen =
                                 Application::self().sparams.fullscreen;
                             return fullscreen.value_or(false);
                         }
                     })
        .addFunction("toggle_fullscreen",
                     [this]() {
                         warn_deprecated("toggle_fullscreen", "set_fullscreen");
                         Ui* ui = Application::get_ui();
                         if (ui) {
                             ui->set_fullscreen(!ui->get_fullscreen());
                             return ui->get_fullscreen();
                         } else {
                             std::optional<bool>& fullscreen =
                                 Application::self().sparams.fullscreen;
                             if (!fullscreen.has_value()) {
                                 fullscreen = true;
                             }
                             return fullscreen.value();
                         }
                     })
        .addFunction("on_initialized",
                     [this](const luabridge::LuaRef& cb) {
                         if (!cb.isFunction()) {
                             print_error(
                                 "Invalid argument for {}.on_initialized: "
                                 "expected function, but got {}",
                                 NS_SWAYIMG, cb.tostring().c_str());
                             return;
                         }
                         const luabridge::LuaRef* ref = add_ref(&cb);
                         Application::self().on_init_complete = [this, ref]() {
                             const luabridge::LuaResult result = (*ref)();
                             if (!result) {
                                 print_error("{}", result.errorMessage());
                             }
                         };
                     })
        .addFunction("enable_antialiasing",
                     [](const bool enable) {
                         Render::self().antialiasing = enable;
                         Application::redraw();
                     })
        .addFunction("enable_exif_orientation",
                     [](const bool enable) {
                         FormatFactory::self().fix_orientation = enable;
                     })
        .addFunction("enable_decoration",
                     [](const bool enable) {
                         Application::self().sparams.decoration = enable;
                     })
        .addFunction("enable_overlay",
                     [](const bool enable) {
                         Application::self().sparams.use_overlay = enable;
                     })
        .addFunction(
            "set_dnd_button",
            [this](const std::string& button) {
                std::optional<InputMouse> input = InputMouse::load(button);
                if (!input) {
                    print_error("Invalid button for {}.set_drag_button: {}",
                                NS_SWAYIMG, button);
                    return;
                }
                Application::self().sparams.dnd = input.value();
            })
        .endNamespace();
}

void LuaEngine::bind_imagelist_api()
{
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_IMAGELIST)
        .addFunction("size",
                     []() {
                         return ImageList::self().size();
                     })
        .addFunction("get",
                     [this]() {
                         luabridge::LuaRef table =
                             luabridge::newTable(lua_state);
                         size_t index = 0;
                         for (const auto& it : ImageList::self().get_all()) {
                             table[++index] = entry_to_table(it);
                         }
                         return table;
                     })
        .addFunction("add",
                     [](const std::string& path) {
                         Application::self().add_image_entry(path);
                     })
        .addFunction("remove",
                     [](const std::string& path) {
                         Application::self().remove_image_entry(path);
                     })
        .addFunction("set_order",
                     [this](const std::string& name) {
                         const auto order =
                             name_to_type(ilorders, name.c_str());
                         if (order.has_value()) {
                             ImageList::self().set_order(order.value());
                         } else {
                             print_error("Invalid image list order: {}", name);
                         }
                     })
        .addFunction("enable_reverse",
                     [](const bool enable) {
                         ImageList::self().set_reverse(enable);
                     })
        .addFunction("enable_recursive",
                     [](const bool enable) {
                         ImageList::self().recursive = enable;
                     })
        .addFunction("enable_adjacent",
                     [](const bool enable) {
                         ImageList::self().adjacent = enable;
                     })
        .addFunction("enable_fsmon",
                     [](const bool enable) {
                         ImageList::self().fsmon = enable;
                     })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_text_api()
{
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_TEXT)
        .addFunction("show",
                     []() {
                         Text::self().show();
                     })
        .addFunction("hide",
                     []() {
                         Text::self().hide();
                     })
        .addFunction("visible",
                     []() {
                         return Text::self().is_visible();
                     })
        .addFunction("set_font",
                     [](const std::string& name) {
                         Text::self().set_font(name);
                     })
        .addFunction("set_size",
                     [](const size_t sz) {
                         Text::self().set_size(sz);
                     })
        .addFunction("set_spacing",
                     [](const ssize_t sz) {
                         Text::self().set_spacing(sz);
                     })
        .addFunction("set_padding",
                     [](const size_t sz) {
                         Text::self().set_padding(sz);
                     })
        .addFunction("set_foreground",
                     [](const uint32_t clr) {
                         Text::self().set_foreground(clr);
                     })
        .addFunction("set_background",
                     [](const uint32_t clr) {
                         Text::self().set_background(clr);
                     })
        .addFunction("set_shadow",
                     [](const uint32_t clr) {
                         Text::self().set_shadow(clr);
                     })
        .addFunction("set_timeout",
                     [](const double timeout) {
                         Text::self().set_overall_timer(timeout * 1000);
                     })
        .addFunction("set_status_timeout",
                     [](const double timeout) {
                         Text::self().set_status_timer(timeout * 1000);
                     })
        .addFunction("set_status",
                     [](const std::string& status) {
                         Text::self().set_status(status);
                     })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_viewer_api(const char* name)
{
    assert(strcmp(name, NS_VIEWER) == 0 || strcmp(name, NS_SLIDESHOW) == 0);

    Viewer* mode =
        strcmp(name, NS_VIEWER) == 0 ? &Viewer::self() : &Slideshow::self();

    bind_appmode_api(name);

    // check if required mode is active
    auto check_active = [this, mode, name](const char* fname) {
        if (Application::self().current_mode() == mode) {
            return true;
        }
        print_error("Unable to execute {}.{}.{}: {} mode is not active",
                    NS_SWAYIMG, name, fname, name);
        return false;
    };

    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(name)
        .addFunction(
            "switch_image",
            [this, check_active, mode, name](const std::string& dname) {
                if (!check_active("switch_image")) {
                    return;
                }
                const auto dir = name_to_type(ildirs, dname.c_str());
                if (dir.has_value()) {
                    mode->open(dir.value());
                } else {
                    print_error(
                        "Invalid argument \"{}\" for {}.{}.switch_image", dname,
                        NS_SWAYIMG, name);
                }
            })
        .addFunction("open",
                     [check_active, mode](const std::string& path) {
                         if (!check_active("open")) {
                             return;
                         }
                         ImageEntryPtr entry =
                             Application::self().add_image_entry(path);
                         if (!entry) {
                             // try to get existing entry
                             entry = ImageList::self().find(path);
                         }
                         if (entry) {
                             mode->open(entry);
                         }
                     })
        .addFunction("get_image",
                     [this, check_active, mode]() {
                         if (!check_active("get_image")) {
                             return luabridge::newTable(lua_state);
                         }
                         const ImagePtr image = mode->current_image();
                         luabridge::LuaRef tbl = entry_to_table(*image->entry);
                         tbl["format"] = image->format;
                         tbl["frames"] = image->frames.size();
                         tbl["width"] = image->frames[0].pm.width();
                         tbl["height"] = image->frames[0].pm.height();
                         const luabridge::LuaRef meta =
                             luabridge::newTable(lua_state);
                         for (const auto& [key, value] : image->meta) {
                             meta[key] = value;
                         }
                         tbl["meta"] = meta;
                         return tbl;
                     })
        .addFunction("reload",
                     [check_active, mode]() {
                         if (check_active("reload")) {
                             mode->reload();
                         }
                     })
        .addFunction("reset",
                     [check_active, mode]() {
                         if (check_active("reset")) {
                             mode->reset();
                         }
                     })
        .addFunction("get_scale",
                     [mode]() {
                         return mode->get_scale();
                     })
        .addFunction("set_abs_scale",
                     [check_active, mode](const double scale,
                                          const std::optional<ssize_t>& x,
                                          const std::optional<ssize_t>& y) {
                         if (!check_active("set_abs_scale")) {
                             return;
                         }
                         Point preserve;
                         if (x.has_value() && y.has_value()) {
                             preserve.x = x.value();
                             preserve.y = y.value();
                         }
                         mode->set_scale(scale, preserve);
                     })
        .addFunction(
            "set_fix_scale",
            [this, check_active, mode, name](const std::string& scname) {
                if (!check_active("set_fix_scale")) {
                    return;
                }
                const auto scale = name_to_type(scales, scname.c_str());
                if (scale.has_value()) {
                    mode->set_scale(scale.value());
                } else {
                    print_error(
                        "Invalid argument \"{}\" for {}.{}.set_fix_scale",
                        scname, NS_SWAYIMG, name);
                }
            })
        .addFunction("set_default_scale",
                     [this, mode, name](const luabridge::LuaRef& val) {
                         if (val.isString()) {
                             const std::string str = val;
                             const auto scale =
                                 name_to_type(scales, str.c_str());
                             if (scale.has_value()) {
                                 mode->default_scale = scale.value();
                             } else {
                                 print_error("Invalid argument \"{}\" for "
                                             "{}.{}.set_default_scale",
                                             str, NS_SWAYIMG, name);
                             }
                         } else if (val.isNumber()) {
                             mode->default_scale = static_cast<double>(val);
                         }
                     })
        .addFunction("get_position",
                     [check_active, mode]() {
                         Point pos;
                         if (check_active("get_position")) {
                             pos = mode->get_position();
                         }
                         return std::unordered_map<std::string, ssize_t> {
                             { "x", pos.x },
                             { "y", pos.y },
                         };
                     })
        .addFunction("set_abs_position",
                     [check_active, mode](const ssize_t x, const ssize_t y) {
                         if (check_active("set_abs_position")) {
                             mode->set_position({ x, y });
                         }
                     })
        .addFunction("set_fix_position",
                     [this, check_active, mode, name](const std::string& fpos) {
                         if (check_active("set_fix_position")) {
                             const auto pos =
                                 name_to_type(imgpositions, fpos.c_str());
                             if (pos.has_value()) {
                                 mode->set_position(pos.value());
                             } else {
                                 print_error("Invalid argument \"{}\" for "
                                             "{}.{}.set_fix_position",
                                             fpos, NS_SWAYIMG, name);
                             }
                         }
                     })
        .addFunction("set_default_position",
                     [this, mode, name](const std::string& fpos) {
                         const auto pos =
                             name_to_type(imgpositions, fpos.c_str());
                         if (pos.has_value()) {
                             mode->default_pos = pos.value();
                         } else {
                             print_error("Invalid argument \"{}\" for "
                                         "{}.{}.set_default_position",
                                         fpos, NS_SWAYIMG, name);
                         }
                     })
        .addFunction("next_frame",
                     [check_active, mode]() {
                         if (check_active("next_frame")) {
                             mode->enable_animation(false);
                             return mode->next_frame();
                         }
                         return static_cast<size_t>(0);
                     })
        .addFunction("prev_frame",
                     [check_active, mode]() {
                         if (check_active("prev_frame")) {
                             mode->enable_animation(false);
                             return mode->prev_frame();
                         }
                         return static_cast<size_t>(0);
                     })
        .addFunction("set_animation",
                     [check_active, mode](const std::optional<bool>& enable) {
                         if (check_active("set_animation")) {
                             mode->enable_animation(
                                 enable.value_or(!mode->animation_enabled()));
                         }
                     })
        .addFunction("get_animation",
                     [check_active, mode]() {
                         return check_active("get_animation") &&
                             mode->animation_enabled();
                     })
        .addFunction("animation_stop",
                     [this, check_active, mode]() {
                         warn_deprecated("animation_stop", "set_animation");
                         if (check_active("animation_stop")) {
                             mode->enable_animation(false);
                         }
                     })
        .addFunction("animation_resume",
                     [this, check_active, mode]() {
                         warn_deprecated("animation_resume", "set_animation");
                         if (check_active("animation_resume")) {
                             mode->enable_animation(true);
                         }
                     })
        .addFunction("flip_vertical",
                     [check_active, mode]() {
                         if (check_active("flip_vertical")) {
                             mode->flip_vertical();
                         }
                     })
        .addFunction("flip_horizontal",
                     [check_active, mode]() {
                         if (check_active("flip_horizontal")) {
                             mode->flip_horizontal();
                         }
                     })
        .addFunction("rotate",
                     [this, check_active, mode, name](const size_t angle) {
                         if (!check_active("rotate")) {
                             return;
                         }
                         if (angle == 90 || angle == 180 || angle == 270) {
                             mode->rotate(angle);
                         } else {
                             print_error(
                                 "Invalid argument \"{}\" for {}.{}.rotate",
                                 angle, NS_SWAYIMG, name);
                         }
                     })
        .addFunction("export",
                     [check_active, mode](const std::string& path) {
                         if (check_active("export")) {
                             mode->export_frame(path);
                         }
                     })
        .addFunction("set_meta",
                     [this, check_active, mode, name](const std::string& key,
                                                      const std::string& val) {
                         if (!check_active("set_meta")) {
                             return;
                         }
                         // remove "meta." from key
                         std::string meta_key;
                         const std::string meta_prefix =
                             std::string(Text::FIELD_META) + ".";
                         if (key.starts_with(meta_prefix)) {
                             meta_key = key.substr(meta_prefix.length());
                         } else {
                             meta_key = key;
                         }
                         if (meta_key.empty()) {
                             print_error("Empty key for {}.{}.set_meta",
                                         NS_SWAYIMG, name);
                             return;
                         }
                         // update meta in image
                         const ImagePtr image = mode->current_image();
                         if (val.empty()) {
                             image->meta.erase(meta_key);
                         } else {
                             image->meta.insert_or_assign(meta_key, val);
                         }
                         // update text layer
                         Text& text = Text::self();
                         text.set_field(std::string(Text::FIELD_META) + "." +
                                            meta_key,
                                        val);
                         text.update();
                         Application::redraw();
                     })
        .addFunction(
            "set_drag_button",
            [this, mode, name](const std::string& state) {
                std::optional<InputMouse> input = InputMouse::load(state);
                if (input) {
                    mode->bind_image_drag(input.value());
                } else {
                    print_error("Invalid button for {}.{}.set_drag_button: {}",
                                NS_SWAYIMG, name, state);
                }
            })
        .addFunction(
            "set_window_background",
            [this, mode, name](const luabridge::LuaRef& val) {
                if (val.isString()) {
                    const std::string str = val;
                    const auto bgmode = name_to_type(wndbkgs, str.c_str());
                    if (bgmode.has_value()) {
                        mode->set_window_background(bgmode.value());
                    } else {
                        print_error("Invalid argument \"{}\" for "
                                    "{}.{}.set_window_background",
                                    str, NS_SWAYIMG, name);
                    }
                } else if (val.isNumber()) {
                    mode->set_window_background(static_cast<uint32_t>(val));
                }
            })
        .addFunction("set_image_background",
                     [mode](const uint32_t& val) {
                         mode->set_image_background(val);
                     })
        .addFunction(
            "set_image_chessboard",
            [mode](const size_t sz, const uint32_t clr0, const uint32_t clr1) {
                mode->set_image_chessboard(sz, clr0, clr1);
            })
        .addFunction("enable_centering",
                     [mode](const bool enable) {
                         mode->auto_center = enable;
                     })
        .addFunction("enable_loop",
                     [mode](const bool enable) {
                         mode->imagelist_loop = enable;
                     })
        .addFunction("limit_preload",
                     [mode](const size_t size) {
                         mode->set_preload_limit(size);
                     })
        .addFunction("limit_history",
                     [mode](const size_t size) {
                         mode->set_history_limit(size);
                     })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_slideshow_api()
{
    bind_viewer_api(NS_SLIDESHOW);

    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_SLIDESHOW)
        .addFunction("set_timeout",
                     [](const double timeout) {
                         Slideshow::self().duration = timeout * 1000;
                     })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_gallery_api()
{
    // check if required mode is active
    auto check_active = [this](const char* name) {
        if (Gallery::self().is_active()) {
            return true;
        }
        print_error("Unable to execute {}.{}.{}: gallery mode is not active",
                    NS_SWAYIMG, NS_GALLERY, name);
        return false;
    };

    bind_appmode_api(NS_GALLERY);

    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_GALLERY)
        .addFunction(
            "switch_image",
            [this, check_active](const std::string& name) {
                if (!check_active("select")) {
                    return;
                }
                const auto dir = name_to_type(gldirs, name.c_str());
                if (dir.has_value()) {
                    Gallery::self().select(dir.value());
                } else {
                    print_error(
                        "Invalid argument \"{}\" for {}.{}.switch_image", name,
                        NS_SWAYIMG, NS_GALLERY);
                }
            })
        .addFunction(
            "get_image",
            [this, check_active]() {
                if (!check_active("get_image")) {
                    return luabridge::newTable(lua_state);
                }
                luabridge::LuaRef table = entry_to_table(
                    *Application::self().current_mode()->current_entry());
                return table;
            })
        .addFunction(
            "set_aspect",
            [this](const std::string& name) {
                const auto aspect = name_to_type(aspects, name.c_str());
                if (aspect.has_value()) {
                    Gallery::self().set_thumb_aspect(aspect.value());
                } else {
                    print_error("Invalid argument \"{}\" for {}.{}.set_aspect",
                                name, NS_SWAYIMG, NS_GALLERY);
                }
            })
        .addFunction("get_thumb_size",
                     []() {
                         return Gallery::self().get_thumb_size();
                     })
        .addFunction("set_thumb_size",
                     [](const size_t size) {
                         Gallery::self().set_thumb_size(size);
                     })
        .addFunction("set_columns",
                     [](const size_t count) {
                         Gallery::self().set_columns(count);
                     })
        .addFunction("set_padding_size",
                     [](const size_t size) {
                         Gallery::self().set_padding_size(size);
                     })
        .addFunction("set_rows",
                     [](const size_t count) {
                         Gallery::self().set_rows(count);
                     })
        .addFunction("set_border_size",
                     [](const size_t size) {
                         Gallery::self().set_border_size(size);
                     })
        .addFunction("set_border_color",
                     [](const uint32_t& color) {
                         Gallery::self().set_border_color(color);
                     })
        .addFunction("set_selected_scale",
                     [](const double scale) {
                         Gallery::self().set_selected_scale(scale);
                     })
        .addFunction("set_selected_color",
                     [](const uint32_t& color) {
                         Gallery::self().set_selected_color(color);
                     })
        .addFunction("set_unselected_color",
                     [](const uint32_t& color) {
                         Gallery::self().set_background_color(color);
                     })
        .addFunction("set_window_color",
                     [](const uint32_t& color) {
                         Gallery::self().set_window_color(color);
                     })
        .addFunction("limit_cache",
                     [](const size_t size) {
                         Gallery::self().set_cache_size(size);
                     })
        .addFunction("enable_preload",
                     [](const bool enable) {
                         Gallery::self().enable_preload(enable);
                     })
        .addFunction("enable_embedded_thumb",
                     [](const bool enable) {
                         FormatFactory::self().embedded_thumb = enable;
                     })
        .addFunction("enable_pstore",
                     [](const bool enable) {
                         Gallery::self().enable_pstore(enable);
                     })
        .addFunction("set_pstore_path",
                     [](const std::filesystem::path& path) {
                         Gallery::self().set_pstore_path(path);
                     })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_appmode_api(const char* name)
{
    AppMode* appmode = nullptr;
    if (strcmp(name, NS_VIEWER) == 0) {
        appmode = &Viewer::self();
    } else if (strcmp(name, NS_SLIDESHOW) == 0) {
        appmode = &Slideshow::self();
    } else if (strcmp(name, NS_GALLERY) == 0) {
        appmode = &Gallery::self();
    }
    assert(appmode);

    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(name)
        .addFunction(
            "mark_image",
            [](const std::optional<bool>& state) {
                const ImageEntryPtr entry =
                    Application::self().current_mode()->current_entry();
                if (state.has_value()) {
                    entry->mark = state.value();
                } else {
                    entry->mark = !entry->mark;
                }
                Application::redraw();
            })
        .addFunction("set_mark_color",
                     [appmode](const uint32_t color) {
                         appmode->set_mark_color(color);
                     })
        .addFunction("set_pinch_factor",
                     [appmode](const double factor) {
                         appmode->set_pinch_factor(factor);
                     })
        .addFunction("bind_reset",
                     [appmode]() {
                         appmode->bind_reset();
                     })
        .addFunction(
            "on_key",
            [this, appmode, name](const std::string& key,
                                  const luabridge::LuaRef& cb) {
                std::optional<InputKeyboard> input = InputKeyboard::load(key);
                if (!input) {
                    print_error("Invalid key for {}.{}.on_key: {}", NS_SWAYIMG,
                                name, key);
                    return;
                }
                if (!cb.isFunction()) {
                    print_error("Invalid argument for {}.{}.on_key: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring().c_str());
                    return;
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->bind_input(*input, [this, ref]() {
                    const luabridge::LuaResult result = (*ref)();
                    if (!result) {
                        print_error("{}", result.errorMessage());
                    }
                });
            })
        .addFunction(
            "on_mouse",
            [this, appmode, name](const std::string& key,
                                  const luabridge::LuaRef& cb) {
                std::optional<InputMouse> input = InputMouse::load(key);
                if (!input) {
                    print_error("Invalid button for {}.{}.on_mouse: {}",
                                NS_SWAYIMG, name, key);
                    return;
                }
                if (!cb.isFunction()) {
                    print_error("Invalid argument for {}.{}.on_mouse: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring().c_str());
                    return;
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->bind_input(*input, [this, ref]() {
                    const luabridge::LuaResult result = (*ref)();
                    if (!result) {
                        print_error("{}", result.errorMessage());
                    }
                });
            })
        .addFunction(
            "on_signal",
            [this, appmode, name](const std::string& key,
                                  const luabridge::LuaRef& cb) {
                std::optional<InputSignal> input = InputSignal::load(key);
                if (!input) {
                    print_error("Invalid signal for {}.{}.on_signal: {}",
                                NS_SWAYIMG, name, key);
                    return;
                }
                if (!cb.isFunction()) {
                    print_error("Invalid argument for {}.{}.on_signal: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring().c_str());
                    return;
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->bind_input(*input, [this, ref]() {
                    const luabridge::LuaResult result = (*ref)();
                    if (!result) {
                        print_error("{}", result.errorMessage());
                    }
                });
            })
        .addFunction("on_image_change",
                     [this, appmode, name](const luabridge::LuaRef& cb) {
                         if (!cb.isFunction()) {
                             print_error(
                                 "Invalid argument for {}.{}.on_image_change: "
                                 "expected function, but got {}",
                                 NS_SWAYIMG, name, cb.tostring().c_str());
                         } else {
                             const luabridge::LuaRef* ref = add_ref(&cb);
                             appmode->subscribe_image_switch([this, ref]() {
                                 const luabridge::LuaResult result = (*ref)();
                                 if (!result) {
                                     print_error("{}", result.errorMessage());
                                 }
                             });
                         }
                     })
        .addFunction(
            "set_text",
            [this, appmode, name](const std::string& pos,
                                  const luabridge::LuaRef& table) {
                const auto bp = name_to_type(tbpositions, pos.c_str());
                if (bp.has_value()) {
                    appmode->set_text_scheme(
                        bp.value(), table.cast<Text::Scheme>().value());
                } else {
                    print_error("Invalid argument \"{}\" for {}.{}.set_text",
                                pos, NS_SWAYIMG, name);
                }
            })
        .endNamespace()
        .endNamespace();
}

luabridge::LuaRef LuaEngine::entry_to_table(const ImageEntry& entry) const
{
    luabridge::LuaRef table = luabridge::newTable(lua_state);
    table["path"] = entry.path.string();
    table["index"] = entry.index;
    table["size"] = entry.size;
    table["mtime"] = entry.mtime;
    table["mark"] = entry.mark;
    return table;
}

luabridge::LuaRef* LuaEngine::add_ref(const luabridge::LuaRef* obj)
{
    luabridge::LuaRef* ref = new luabridge::LuaRef(*obj);
    refs.push_back(ref);
    return ref;
}

void LuaEngine::warn_deprecated(const char* name, const char* replacement) const
{
    Log::warning(
        "Function \"{}\" is deprecated and will be removed in a future release,"
        " use \"{}\" instead",
        name, replacement);
}

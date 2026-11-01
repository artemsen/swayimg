// SPDX-License-Identifier: MIT
// Lua integration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luaengine.hpp"

#include "application.hpp"
#include "formatfactory.hpp"
#include "gallery.hpp"
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

// Using 0xff in the alpha channel leads to runtime errors:
// "The native integer can't fit inside a lua integer"
// Use double to avoid such errors.
#if defined(__ILP32__) || defined(_ILP32)
using luacolor_t = double;
#else
using luacolor_t = uint32_t;
#endif

// app modes table: type to name
static constexpr std::array appmodes =
    std::to_array<std::pair<AppMode::Type, const char*>>({
        { AppMode::Viewer,    "viewer"    },
        { AppMode::Slideshow, "slideshow" },
        { AppMode::Gallery,   "gallery"   },
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

// automatic positioning: type to name
static constexpr std::array imgautopos =
    std::to_array<std::pair<Viewer::AutoPos, const char*>>({
        { Viewer::AutoPos::Free,   "free"   },
        { Viewer::AutoPos::Center, "center" },
        { Viewer::AutoPos::Follow, "follow" },
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

// mouse buttons removed in favor of on_scroll()
static constexpr std::array removed_scroll_buttons = {
    "ScrollUp",
    "ScrollDown",
    "ScrollLeft",
    "ScrollRight",
};

namespace {

/**
 * Get type from name.
 * @param arr table with all possible pairs type->name
 * @param name name to convert
 * @return type or nullopt if name not found
 */
template <typename T, size_t N>
std::optional<T>
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
const char* type_to_name(const std::array<std::pair<T, const char*>, N>& arr,
                         const T& type)
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
std::filesystem::path get_config_file()
{
    static constexpr std::array env_paths =
        std::to_array<std::pair<const char*, const char*>>({
            { "XDG_CONFIG_HOME", "swayimg"          },
            { "XDG_CONFIG_DIRS", "swayimg"          },
            { "HOME",            ".config/swayimg"  },
            { nullptr,           "/etc/xdg/swayimg" },
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
                path = std::string(env, delim);
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

} // anonymous namespace

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
        delete defer_fn;
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
        const std::filesystem::path config_dir = config_file.parent_path();
        std::string pack_path = config_dir / "?.lua;";
        pack_path += config_dir / "?.so;";
        lua_getglobal(lua_state, "package");
        lua_getfield(lua_state, -1, "path");
        pack_path += lua_tostring(lua_state, -1);
        lua_pop(lua_state, 2);
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

    // register timer for deferred procedure call
    Application::self().add_fdpoll(defer_timer, [this] {
        defer_timer.reset(0, 0);
        call(*defer_fn);
    });

    // load config file
    if (!config_file.empty()) {
        if (luaL_loadfile(lua_state, config_file.c_str()) != LUA_OK) {
            const char* msg = lua_tostring(lua_state, -1);
            print_error("Failed to load config file: {}", msg ? msg : "<?>");
        } else {
            const luabridge::LuaRef chunk =
                luabridge::LuaRef::fromStack(lua_state, -1);
            lua_pop(lua_state, 1);
            call(chunk);
        }
    }
}

void LuaEngine::execute(const std::string& script)
{
    assert(lua_state);

    if (luaL_loadstring(lua_state, script.c_str()) != LUA_OK) {
        const char* msg = lua_tostring(lua_state, -1);
        print_error("Failed to load script line: {}", msg ? msg : "<?>");
    } else {
        const luabridge::LuaRef chunk =
            luabridge::LuaRef::fromStack(lua_state, -1);
        lua_pop(lua_state, 1);
        call(chunk);
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
void LuaEngine::bind_root_api()
{
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .addProperty(
            "appid",
            [] {
                return Application::self().get_appid();
            },
            [this](const std::string& value) {
                if (Application::self().initialized()) {
                    raise_error("Application Id can be set only at startup");
                }
                if (value.empty()) {
                    raise_error("Application Id can not be empty");
                }
                Application::self().sparams->app_id.set(value);
            })
        .addProperty(
            "mode",
            [] {
                return type_to_name(appmodes, Application::self().get_mode());
            },
            [this](const std::string& value) {
                const auto mode = name_to_type(appmodes, value.c_str());
                if (!mode.has_value()) {
                    raise_error("Invalid mode: {}", value);
                }
                Application::self().set_mode(mode.value());
            })
        .addProperty(
            "fullscreen",
            [] {
                const Ui* ui = Application::get_ui();
                if (ui) {
                    return ui->get_fullscreen();
                }
                return Application::self().sparams->fullscreen.get();
            },
            [](const bool value) {
                if (Application::self().initialized()) {
                    Application::get_ui()->set_fullscreen(value);
                } else {
                    Application::self().sparams->fullscreen.set(value);
                }
            })
        .addProperty(
            "dnd_button",
            [] {
                return nullptr;
            },
            [this](const std::string& value) {
                if (Application::self().initialized()) {
                    raise_error("DND can be set only at startup");
                }
                std::optional<InputMouse> input = InputMouse::load(value);
                if (!input.has_value()) {
                    raise_error("Invalid button for {}.drag_button: {}",
                                NS_SWAYIMG, value);
                }
                Application::self().sparams->dnd = input.value();
            })
        .addFunction(
            "on_dnd",
            [this](const luabridge::LuaRef& cb) {
                Application& app = Application::self();
                if (cb.isNil()) {
                    app.on_drag_and_drop = nullptr;
                } else if (!cb.isFunction()) {
                    raise_error("Invalid argument for {}.on_dnd: "
                                "expected function, but got {}",
                                NS_SWAYIMG, cb.tostring());
                } else {
                    const luabridge::LuaRef* ref = add_ref(&cb);
                    app.on_drag_and_drop =
                        [this,
                         ref](const std::vector<std::filesystem::path>& paths) {
                            std::vector<std::string> sp;
                            sp.reserve(paths.size());
                            for (const auto& it : paths) {
                                sp.emplace_back(it);
                            }
                            call(*ref, sp);
                        };
                }
            })
        .addProperty(
            "overlay",
            [] {
                return nullptr;
            },
            [this](const bool value) {
                if (Application::self().initialized()) {
                    raise_error("Overlay can be set only at startup");
                }
                Application::self().sparams->use_overlay = value;
            })
        .addProperty(
            "decoration",
            [] {
                return nullptr;
            },
            [this](const bool value) {
                if (Application::self().initialized()) {
                    raise_error("Decoration can be set only at startup");
                }
                Application::self().sparams->decoration = value;
            })
        .addProperty(
            "antialiasing",
            [] {
                return Render::self().antialiasing;
            },
            [](const bool value) {
                Render::self().antialiasing = value;
                Application::redraw();
            })
        .addProperty(
            "exif_orientation",
            [] {
                return FormatFactory::self().fix_orientation;
            },
            [](const bool value) {
                FormatFactory::self().fix_orientation = value;
            })
        .addProperty(
            "title",
            [] {
                return nullptr;
            },
            [](const std::string& value) {
                Ui* ui = Application::get_ui();
                if (ui) {
                    ui->set_title(value.c_str());
                }
            })
        .addFunction("exit",
                     [](const std::optional<int>& code) {
                         Application::self().exit(code ? *code : 0);
                     })
        .addFunction("get_window_size",
                     [] {
                         Size wnd;
                         if (Application::self().initialized()) {
                             wnd = Application::get_ui()->get_window_size();
                         } else {
                             wnd = Application::self().sparams->wnd_size;
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
                    raise_error(
                        "Invalid arguments ({}, {}) for {}.set_window_size",
                        width, height, NS_SWAYIMG);
                }
                if (Application::self().initialized()) {
                    Application::get_ui()->set_window_size(
                        { .width = width, .height = height });
                } else {
                    Application::self().sparams->wnd_size = Size(width, height);
                }
            })
        .addFunction("on_window_resize",
                     [this](const luabridge::LuaRef& cb) {
                         Application& app = Application::self();
                         if (cb.isNil()) {
                             app.on_wnd_resize = nullptr;
                         } else if (!cb.isFunction()) {
                             raise_error(
                                 "Invalid argument for {}.on_window_resize: "
                                 "expected function, but got {}",
                                 NS_SWAYIMG, cb.tostring());
                         } else {
                             const luabridge::LuaRef* ref = add_ref(&cb);
                             app.on_wnd_resize = [this, ref] {
                                 call(*ref);
                             };
                         }
                     })
        .addFunction("get_mouse_pos",
                     [] {
                         Point pos { .x = 0, .y = 0 };
                         Ui* ui = Application::get_ui();
                         if (ui) {
                             pos = ui->get_mouse();
                         }
                         return std::unordered_map<std::string, ssize_t> {
                             { "x", pos.x },
                             { "y", pos.y },
                         };
                     })
        .addFunction("on_initialized",
                     [this](const luabridge::LuaRef& cb) {
                         if (!cb.isFunction()) {
                             raise_error(
                                 "Invalid argument for {}.on_initialized: "
                                 "expected function, but got {}",
                                 NS_SWAYIMG, cb.tostring());
                         }
                         const luabridge::LuaRef* ref = add_ref(&cb);
                         Application::self().on_init_complete = [this, ref] {
                             call(*ref);
                         };
                     })
        .addFunction("on_redrawn",
                     [this](const luabridge::LuaRef& cb) {
                         Application& app = Application::self();
                         if (cb.isNil()) {
                             app.on_redraw_complete = nullptr;
                         } else if (!cb.isFunction()) {
                             raise_error("Invalid argument for {}.on_redrawn: "
                                         "expected function, but got {}",
                                         NS_SWAYIMG, cb.tostring());
                         } else {
                             const luabridge::LuaRef* ref = add_ref(&cb);
                             app.on_redraw_complete = [this, ref] {
                                 call(*ref);
                             };
                         }
                     })
        .addFunction("defer",
                     [this](const double delay, const luabridge::LuaRef& cb) {
                         delete defer_fn;
                         defer_fn = nullptr;
                         if (delay == 0 || cb.isNil()) {
                             defer_timer.reset(0, 0);
                             return;
                         }
                         if (!cb.isFunction()) {
                             raise_error("Invalid argument for {}.defer: "
                                         "expected function, but got {}",
                                         NS_SWAYIMG, cb.tostring());
                         }
                         const size_t ms = delay * 1000;
                         defer_fn = new luabridge::LuaRef(cb);
                         defer_timer.reset(ms > 0 ? ms : 1, 0);
                     })
        .addProperty(
            "format_conf",
            [] {
                return nullptr;
            },
            [this](const std::unordered_map<
                   std::string,
                   std::unordered_map<std::string, luabridge::LuaRef>>& conf) {
                for (const auto& [format, params] : conf) {
                    ImageFormat* fmt =
                        FormatFactory::self().get(format.c_str());
                    if (!fmt) {
                        raise_error("Unsupported image format {}", format);
                    }
                    ImageFormat::Config fmtconf;
                    for (const auto& [name, value] : params) {
                        switch (value.type()) {
                            case LUA_TBOOLEAN:
                                fmtconf.set(name, static_cast<bool>(value));
                                break;
                            case LUA_TNUMBER:
                                fmtconf.set(
                                    name,
                                    static_cast<size_t>(
                                        static_cast<luacolor_t>(value)));
                                break;
                            case LUA_TSTRING:
                                fmtconf.set(name,
                                            static_cast<std::string>(value));
                                break;
                            default:
                                raise_error("Invalid type in parameter '{}' "
                                            "for format {}",
                                            name, format);
                        }
                    }
                    fmt->set_config(fmtconf);
                    for (const auto& it :
                         fmtconf.get(ImageFormat::Config::Invalid)) {
                        raise_error(
                            "Invalid value in parameter '{}' for format {}", it,
                            format);
                    }
                    for (const auto& it :
                         fmtconf.get(ImageFormat::Config::Unhandled)) {
                        raise_error("Unknown parameter '{}' for format {}", it,
                                    format);
                    }
                }
            })
        .endNamespace();
}

void LuaEngine::bind_imagelist_api()
{
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_IMAGELIST)
        .addProperty(
            "order",
            [] {
                return type_to_name(ilorders, ImageList::self().get_order());
            },
            [this](const std::string& value) {
                const auto order = name_to_type(ilorders, value.c_str());
                if (!order.has_value()) {
                    raise_error("Invalid image list order: {}", value);
                }
                ImageList::self().set_order(order.value());
            })
        .addProperty(
            "reverse",
            [] {
                return ImageList::self().get_reverse();
            },
            [](const bool value) {
                ImageList::self().set_reverse(value);
            })
        .addProperty(
            "recursive",
            [] {
                return ImageList::self().recursive;
            },
            [](const bool value) {
                ImageList::self().recursive = value;
            })
        .addProperty(
            "adjacent",
            [] {
                return ImageList::self().adjacent;
            },
            [](const bool value) {
                ImageList::self().adjacent = value;
            })
        .addProperty(
            "fsmon",
            [] {
                return ImageList::self().fsmon;
            },
            [](const bool value) {
                ImageList::self().fsmon = value;
            })
        .addProperty("size",
                     [] {
                         return ImageList::self().size();
                     })
        .addFunction("add",
                     [this](const luabridge::LuaRef& val) {
                         std::vector<std::filesystem::path> paths;
                         if (val.isString()) {
                             paths.emplace_back(val.tostring());
                         } else if (val.isTable()) {
                             const size_t arr_sz = val.length();
                             paths.reserve(arr_sz);
                             for (size_t i = 1; i <= arr_sz; ++i) {
                                 paths.emplace_back(val[i].tostring());
                             }
                         } else {
                             raise_error("Invalid argument type");
                         }
                         Application::self().add_images(paths);
                     })
        .addFunction("remove",
                     [this](const luabridge::LuaRef& val) {
                         std::vector<std::filesystem::path> paths;
                         if (val.isString()) {
                             paths.emplace_back(val.tostring());
                         } else if (val.isTable()) {
                             const size_t arr_sz = val.length();
                             paths.reserve(arr_sz);
                             for (size_t i = 1; i <= arr_sz; ++i) {
                                 paths.emplace_back(val[i].tostring());
                             }
                         } else {
                             raise_error("Invalid argument type");
                         }
                         Application::self().remove_images(paths);
                     })
        .addFunction("clear",
                     [] {
                         Application::self().remove_all_images();
                     })
        .addFunction("get",
                     [this] {
                         luabridge::LuaRef table =
                             luabridge::newTable(lua_state);
                         size_t index = 0;
                         for (const auto& it : ImageList::self().get_all()) {
                             table[++index] = entry_to_table(*it);
                         }
                         return table;
                     })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_text_api()
{
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_TEXT)
        .addProperty(
            "visible",
            [] {
                return Text::self().is_visible();
            },
            [](const bool value) {
                if (value) {
                    Text::self().show();
                } else {
                    Text::self().hide();
                }
            })
        .addProperty(
            "timeout",
            [] {
                return nullptr;
            },
            [](const double value) {
                Text::self().set_overall_timer(value * 1000);
            })
        .addProperty(
            "status_timeout",
            [] {
                return nullptr;
            },
            [](const double value) {
                Text::self().set_status_timer(value * 1000);
            })
        .addProperty(
            "font",
            [] {
                return nullptr;
            },
            [](const std::string& value) {
                Text::self().set_font(value);
            })
        .addProperty(
            "size",
            [] {
                return nullptr;
            },
            [](const size_t value) {
                Text::self().set_size(value);
            })
        .addProperty(
            "spacing",
            [] {
                return nullptr;
            },
            [](const ssize_t value) {
                Text::self().set_spacing(value);
            })
        .addProperty(
            "padding",
            [] {
                return nullptr;
            },
            [](const size_t value) {
                Text::self().set_padding(value);
            })
        .addProperty(
            "color",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Text::self().set_foreground(value);
            })
        .addProperty(
            "background",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Text::self().set_background(value);
            })
        .addProperty(
            "shadow",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Text::self().set_shadow(value);
            })
        .addProperty(
            "status",
            [] {
                return nullptr;
            },
            [](const std::string& value) {
                Text::self().set_status(value);
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
    const auto ensure_active = [this, mode, name](const char* fname) {
        if (Application::self().current_mode() != mode) {
            raise_error("Unable to execute {}.{}.{}: mode not active",
                        NS_SWAYIMG, name, fname);
        }
    };

    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(name)
        .addProperty(
            "autopos",
            [] {
                return nullptr;
            },
            [this, mode, name](const char* value) {
                const auto pos = name_to_type(imgautopos, value);
                if (!pos.has_value()) {
                    raise_error("Invalid argument \"{}\" for "
                                "{}.{}.autopos",
                                value, NS_SWAYIMG, name);
                }
                mode->auto_pos = pos.value();
            })
        .addProperty(
            "loop",
            [] {
                return nullptr;
            },
            [mode](const bool value) {
                mode->imagelist_loop = value;
            })
        .addProperty(
            "default_scale",
            [] {
                return nullptr;
            },
            [this, mode, name](const luabridge::LuaRef& value) {
                if (value.isString()) {
                    const std::string str = value;
                    const auto scale = name_to_type(scales, str.c_str());
                    if (!scale.has_value()) {
                        raise_error("Invalid argument \"{}\" for "
                                    "{}.{}.set_default_scale",
                                    str, NS_SWAYIMG, name);
                    }
                    mode->default_scale = scale.value();
                } else if (value.isNumber()) {
                    mode->default_scale = static_cast<double>(value);
                } else {
                    raise_error(
                        "Invalid argument \"{}\" for {}.{}.default_scale",
                        value.tostring(), NS_SWAYIMG, name);
                }
            })
        .addProperty(
            "default_position",
            [] {
                return nullptr;
            },
            [this, mode, name](const std::string& value) {
                const auto pos = name_to_type(imgpositions, value.c_str());
                if (!pos.has_value()) {
                    raise_error(
                        "Invalid argument \"{}\" for {}.{}.default_position",
                        value, NS_SWAYIMG, name);
                }
                mode->default_pos = pos.value();
            })
        .addProperty(
            "scale",
            [mode] {
                return mode->get_scale();
            },
            [mode](const double value) {
                Point pos { .x = 0, .y = 0 };
                Ui* ui = Application::get_ui();
                if (ui) {
                    pos = ui->get_mouse();
                }
                mode->set_scale(value, pos);
            })
        .addProperty(
            "animation",
            [ensure_active, mode] {
                ensure_active("animation");
                return mode->animation_enabled();
            },
            [ensure_active, mode](const bool value) {
                ensure_active("next_frame");
                mode->enable_animation(value);
            })
        .addProperty(
            "frame",
            [ensure_active, mode] {
                ensure_active("frame");
                return mode->get_frame();
            },
            [ensure_active, mode](const size_t value) {
                ensure_active("frame");
                mode->enable_animation(false);
                mode->set_frame(value);
            })
        .addProperty(
            "focus_radius",
            [] {
                return nullptr;
            },
            [mode](const size_t value) {
                mode->set_focus_radius(value);
            })
        .addProperty(
            "focus_factor",
            [] {
                return nullptr;
            },
            [mode](const double value) {
                mode->set_focus_factor(value);
            })
        .addProperty(
            "drag_button",
            [] {
                return nullptr;
            },
            [this, mode, name](const std::string& value) {
                std::optional<InputMouse> input = InputMouse::load(value);
                if (!input.has_value()) {
                    raise_error("Invalid button for {}.{}.drag_button: {}",
                                NS_SWAYIMG, name, value);
                }
                mode->bind_image_drag(input.value());
            })
        .addProperty(
            "preload",
            [] {
                return nullptr;
            },
            [mode](const size_t value) {
                mode->set_preload_limit(value);
            })
        .addProperty(
            "history",
            [] {
                return nullptr;
            },
            [mode](const size_t value) {
                mode->set_history_limit(value);
            })
        .addFunction(
            "open",
            [this, ensure_active, mode, name](const std::string& dname) {
                ensure_active("open");
                const auto dir = name_to_type(ildirs, dname.c_str());
                if (!dir.has_value()) {
                    raise_error("Invalid argument \"{}\" for {}.{}.open", dname,
                                NS_SWAYIMG, name);
                }
                return mode->open(dir.value());
            })
        .addFunction("open_path",
                     [ensure_active, mode](const std::string& path) {
                         ensure_active("open_path");
                         ImageEntryPtr entry = ImageList::self().find(path);
                         if (!entry) {
                             entry = Application::self().add_images({ path });
                         }
                         return entry && mode->set_current(entry);
                     })
        .addFunction(
            "get_image",
            [this, ensure_active, mode] {
                ensure_active("get_image");
                const ImagePtr image = mode->current_image();
                if (!image) {
                    return luabridge::LuaRef(lua_state, luabridge::LuaNil {});
                }
                luabridge::LuaRef tbl = entry_to_table(*image->entry);
                tbl["format"] = image->format;
                tbl["frames"] = image->frames.size();
                tbl["width"] = image->frames[0].pm.width();
                tbl["height"] = image->frames[0].pm.height();
                const luabridge::LuaRef meta = luabridge::newTable(lua_state);
                for (const auto& [key, value] : image->meta) {
                    meta[key] = value;
                }
                tbl["meta"] = meta;
                return tbl;
            })
        .addFunction("reload",
                     [ensure_active, mode] {
                         ensure_active("reload");
                         mode->reload();
                     })
        .addFunction("set_abs_scale",
                     [ensure_active, mode](const double scale,
                                           const std::optional<ssize_t>& x,
                                           const std::optional<ssize_t>& y) {
                         ensure_active("set_abs_scale");
                         Point preserve;
                         if (x.has_value() && y.has_value()) {
                             preserve.x = x.value();
                             preserve.y = y.value();
                         }
                         mode->set_scale(scale, preserve);
                     })
        .addFunction(
            "set_fix_scale",
            [this, ensure_active, mode, name](const std::string& scname) {
                ensure_active("set_fix_scale");
                const auto scale = name_to_type(scales, scname.c_str());
                if (!scale.has_value()) {
                    raise_error(
                        "Invalid argument \"{}\" for {}.{}.set_fix_scale",
                        scname, NS_SWAYIMG, name);
                }
                mode->set_scale(scale.value());
            })
        .addFunction("reset",
                     [ensure_active, mode] {
                         ensure_active("reset");
                         mode->reset();
                     })
        .addFunction("get_position",
                     [ensure_active, mode] {
                         ensure_active("get_position");
                         Point pos = mode->get_position();
                         return std::unordered_map<std::string, ssize_t> {
                             { "x", pos.x },
                             { "y", pos.y },
                         };
                     })
        .addFunction("set_abs_position",
                     [ensure_active, mode](const ssize_t x, const ssize_t y) {
                         ensure_active("set_abs_position");
                         mode->set_position({ .x = x, .y = y });
                     })
        .addFunction(
            "set_fix_position",
            [this, ensure_active, mode, name](const std::string& fpos) {
                ensure_active("set_fix_position");
                const auto pos = name_to_type(imgpositions, fpos.c_str());
                if (!pos.has_value()) {
                    raise_error("Invalid argument \"{}\" for "
                                "{}.{}.set_fix_position",
                                fpos, NS_SWAYIMG, name);
                }
                mode->set_position(pos.value());
            })
        .addFunction("flip_vertical",
                     [ensure_active, mode] {
                         ensure_active("flip_vertical");
                         mode->flip_vertical();
                     })
        .addFunction("flip_horizontal",
                     [ensure_active, mode] {
                         ensure_active("flip_horizontal");
                         mode->flip_horizontal();
                     })
        .addFunction("rotate",
                     [this, ensure_active, mode, name](const size_t angle) {
                         ensure_active("rotate");
                         if (angle != 90 && angle != 180 && angle != 270) {
                             raise_error(
                                 "Invalid argument \"{}\" for {}.{}.rotate",
                                 angle, NS_SWAYIMG, name);
                         }
                         mode->rotate(angle);
                     })
        .addFunction("export",
                     [ensure_active, mode](const std::string& path) {
                         ensure_active("export");
                         mode->export_frame(path);
                     })
        .addFunction("set_meta",
                     [this, ensure_active, mode, name](const std::string& key,
                                                       const std::string& val) {
                         ensure_active("set_meta");
                         const ImagePtr image = mode->current_image();
                         if (!image) {
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
                             raise_error("Empty key for {}.{}.set_meta",
                                         NS_SWAYIMG, name);
                         }
                         // update meta in image
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
            "set_window_background",
            [this, mode, name](const luabridge::LuaRef& value) {
                if (value.isString()) {
                    const std::string str = value;
                    const auto bgmode = name_to_type(wndbkgs, str.c_str());
                    if (!bgmode.has_value()) {
                        raise_error("Invalid argument \"{}\" for "
                                    "{}.{}.set_window_background()",
                                    str, NS_SWAYIMG, name);
                    }
                    mode->set_window_background(bgmode.value());
                } else if (value.isNumber()) {
                    mode->set_window_background(static_cast<luacolor_t>(value));
                } else {
                    raise_error("Invalid argument \"{}\" for "
                                "{}.{}.set_window_background()",
                                value.tostring(), NS_SWAYIMG, name);
                }
            })
        .addFunction("set_image_background",
                     [mode](const luacolor_t val) {
                         mode->set_image_background(val);
                     })
        .addFunction("set_image_chessboard",
                     [mode](const size_t sz, const luacolor_t clr0,
                            const luacolor_t clr1) {
                         mode->set_image_chessboard(sz, clr0, clr1);
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
        .addProperty(
            "timeout",
            [] {
                return Slideshow::self().duration / 1000;
            },
            [](const double value) {
                Slideshow::self().duration = value * 1000;
            })
        .endNamespace()
        .endNamespace();
}

void LuaEngine::bind_gallery_api()
{
    // check if required mode is active
    const auto ensure_active = [this](const char* fname) {
        if (!Gallery::self().is_active()) {
            raise_error("Unable to execute {}.{}.{}: mode not active",
                        NS_SWAYIMG, NS_GALLERY, fname);
        }
    };

    bind_appmode_api(NS_GALLERY);

    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace(NS_SWAYIMG)
        .beginNamespace(NS_GALLERY)
        .addProperty(
            "aspect",
            [] {
                return nullptr;
            },
            [this](const std::string& value) {
                const auto aspect = name_to_type(aspects, value.c_str());
                if (!aspect.has_value()) {
                    raise_error("Invalid argument \"{}\" for {}.{}.set_aspect",
                                value, NS_SWAYIMG, NS_GALLERY);
                }
                Gallery::self().set_thumb_aspect(aspect.value());
            })
        .addProperty(
            "thumb_size",
            [] {
                return Gallery::self().get_thumb_size();
            },
            [](const size_t size) {
                Gallery::self().set_thumb_size(size);
            })
        .addProperty(
            "padding_size",
            [] {
                return nullptr;
            },
            [](const size_t value) {
                Gallery::self().set_padding_size(value);
            })
        .addProperty(
            "border_size",
            [] {
                return nullptr;
            },
            [](const size_t value) {
                Gallery::self().set_border_size(value);
            })
        .addProperty(
            "selected_scale",
            [] {
                return nullptr;
            },
            [](const double value) {
                Gallery::self().set_selected_scale(value);
            })
        .addProperty(
            "window_color",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Gallery::self().set_window_color(value);
            })
        .addProperty(
            "unselected_color",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Gallery::self().set_background_color(value);
            })
        .addProperty(
            "selected_color",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Gallery::self().set_selected_color(value);
            })
        .addProperty(
            "border_color",
            [] {
                return nullptr;
            },
            [](const luacolor_t value) {
                Gallery::self().set_border_color(value);
            })
        .addProperty(
            "hover",
            [] {
                return nullptr;
            },
            [](const bool value) {
                Gallery::self().enable_hover(value);
            })
        .addProperty(
            "pstore",
            [] {
                return nullptr;
            },
            [](const bool value) {
                Gallery::self().enable_pstore(value);
            })
        .addProperty(
            "pstore_path",
            [] {
                return nullptr;
            },
            [](const std::string& value) {
                Gallery::self().set_pstore_path(value);
            })
        .addProperty(
            "preload",
            [] {
                return nullptr;
            },
            [](const bool value) {
                Gallery::self().enable_preload(value);
            })
        .addProperty(
            "cache",
            [] {
                return nullptr;
            },
            [](const size_t value) {
                Gallery::self().set_cache_size(value);
            })
        .addProperty(
            "embedded_thumb",
            [] {
                return FormatFactory::self().embedded_thumb;
            },
            [](const bool value) {
                FormatFactory::self().embedded_thumb = value;
            })
        .addFunction("select",
                     [this, ensure_active](const std::string& name) {
                         ensure_active("select");
                         const auto dir = name_to_type(gldirs, name.c_str());
                         if (!dir.has_value()) {
                             raise_error(
                                 "Invalid argument \"{}\" for {}.{}.select",
                                 name, NS_SWAYIMG, NS_GALLERY);
                         }
                         return Gallery::self().select(dir.value());
                     })
        .addFunction("select_at",
                     [ensure_active](const size_t x, const size_t y) {
                         ensure_active("select_at");
                         return Gallery::self().select(Point(x, y));
                     })
        .addFunction("select_path",
                     [ensure_active](const std::string& path) {
                         ensure_active("select_path");
                         ImageEntryPtr entry = ImageList::self().find(path);
                         if (!entry) {
                             entry = Application::self().add_images({ path });
                         }
                         return entry && Gallery::self().set_current(entry);
                     })
        .addFunction("reload",
                     [ensure_active] {
                         ensure_active("reload");
                         Gallery::self().reload();
                     })
        .addFunction("get_image",
                     [this, ensure_active] {
                         ensure_active("get_image");
                         const ImageEntryPtr entry =
                             Application::self().current_mode()->get_current();
                         if (entry) {
                             return entry_to_table(*entry);
                         }
                         return luabridge::LuaRef(lua_state,
                                                  luabridge::LuaNil {});
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
        .addProperty(
            "mark_color",
            [] {
                return nullptr;
            },
            [appmode](const luacolor_t value) {
                appmode->set_mark_color(value);
            })
        .addProperty(
            "pinch_factor",
            [] {
                return nullptr;
            },
            [appmode](const double value) {
                appmode->set_pinch_factor(value);
            })
        .addProperty(
            "text",
            [] {
                return nullptr;
            },
            [this, appmode](
                const std::unordered_map<std::string, Text::Scheme>& params) {
                for (const auto& [pos, content] : params) {
                    const auto bp = name_to_type(tbpositions, pos.c_str());
                    if (!bp.has_value()) {
                        raise_error("Invalid position name \"{}\"", pos);
                    }
                    appmode->set_text_scheme(bp.value(), content);
                }
            })
        .addFunction("mark_image",
                     [appmode](const std::optional<bool>& state) {
                         appmode->mark_current(state);
                     })
        .addFunction("bind_reset",
                     [appmode] {
                         appmode->bind_reset();
                     })
        .addFunction("on_key",
                     [this, appmode, name](const luabridge::LuaRef& val,
                                           const luabridge::LuaRef& cb) {
                         if (!cb.isFunction()) {
                             raise_error("Invalid argument for {}.{}.on_key: "
                                         "expected function, but got {}",
                                         NS_SWAYIMG, name, cb.tostring());
                         }

                         std::vector<std::string> kdesc;
                         if (val.isString()) {
                             kdesc.emplace_back(val.tostring());
                         } else if (val.isTable()) {
                             const size_t arr_sz = val.length();
                             kdesc.reserve(arr_sz);
                             for (size_t i = 1; i <= arr_sz; ++i) {
                                 kdesc.emplace_back(val[i].tostring());
                             }
                         } else {
                             raise_error("Invalid argument type");
                         }

                         for (const auto& key : kdesc) {
                             std::optional<InputKeyboard> input =
                                 InputKeyboard::load(key);
                             if (!input.has_value()) {
                                 raise_error("Invalid key for {}.{}.on_key: {}",
                                             NS_SWAYIMG, name, key);
                             }
                             const luabridge::LuaRef* ref = add_ref(&cb);
                             appmode->bind_input(*input, [this, ref] {
                                 call(*ref);
                             });
                         }
                     })
        .addFunction(
            "on_unassigned_key",
            [this, appmode, name](const luabridge::LuaRef& cb) {
                if (!cb.isFunction()) {
                    raise_error("Invalid argument for {}.{}.on_unassigned_key: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring());
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->on_unassigned_key = [this,
                                              ref](const InputKeyboard& input) {
                    call(*ref, input.to_string().c_str());
                };
            })
        .addFunction(
            "on_scroll",
            [this, appmode, name](const luabridge::LuaRef& cb) {
                if (!cb.isFunction()) {
                    raise_error("Invalid argument for {}.{}.on_scroll: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring());
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->on_scroll = [this, ref](const keymod_t kmods,
                                                 const double delta_h,
                                                 const double delta_v) {
                    call(*ref, InputKeyboard::mods_to_string(kmods), delta_h,
                         delta_v);
                };
            })
        .addFunction(
            "on_mouse",
            [this, appmode, name](const std::string& key,
                                  const luabridge::LuaRef& cb) {
                std::optional<InputMouse> input = InputMouse::load(key);
                if (!input.has_value()) {
                    for (const char* button : removed_scroll_buttons) {
                        if (key.find(button) != std::string::npos) {
                            raise_error("Invalid button for {}.{}.on_mouse: "
                                        "{} was removed, use {}.{}.on_scroll() "
                                        "instead",
                                        NS_SWAYIMG, name, button, NS_SWAYIMG,
                                        name);
                        }
                    }
                    raise_error("Invalid button for {}.{}.on_mouse: {}",
                                NS_SWAYIMG, name, key);
                }
                if (!cb.isFunction()) {
                    raise_error("Invalid argument for {}.{}.on_mouse: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring());
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->bind_input(*input, [this, ref] {
                    call(*ref);
                });
            })
        .addFunction(
            "on_signal",
            [this, appmode, name](const std::string& key,
                                  const luabridge::LuaRef& cb) {
                std::optional<InputSignal> input = InputSignal::load(key);
                if (!input.has_value()) {
                    raise_error("Invalid signal for {}.{}.on_signal: {}",
                                NS_SWAYIMG, name, key);
                }
                if (!cb.isFunction()) {
                    raise_error("Invalid argument for {}.{}.on_signal: "
                                "expected function, but got {}",
                                NS_SWAYIMG, name, cb.tostring());
                }
                const luabridge::LuaRef* ref = add_ref(&cb);
                appmode->bind_input(*input, [this, ref] {
                    call(*ref);
                });
            })
        .addFunction("on_image_change",
                     [this, appmode, name](const luabridge::LuaRef& cb) {
                         if (cb.isNil()) {
                             appmode->on_image_change = nullptr;
                         } else if (!cb.isFunction()) {
                             raise_error(
                                 "Invalid argument for {}.{}.on_image_change: "
                                 "expected function, but got {}",
                                 NS_SWAYIMG, name, cb.tostring());
                         } else {
                             const luabridge::LuaRef* ref = add_ref(&cb);
                             appmode->on_image_change = [this, ref] {
                                 call(*ref);
                             };
                         }
                     })
        .endNamespace()
        .endNamespace();
}
// NOLINTEND(readability-function-cognitive-complexity)

luabridge::LuaRef LuaEngine::entry_to_table(const ImageEntry& entry) const
{
    luabridge::LuaRef table = luabridge::newTable(lua_state);
    table["path"] = entry.path.string();
    table["index"] = entry.index + 1;
    table["size"] = entry.size;
    table["mtime"] = entry.mtime;
    table["mark"] = entry.mark;
    return table;
}

const luabridge::LuaRef* LuaEngine::add_ref(const luabridge::LuaRef* obj)
{
    const luabridge::LuaRef* ref = new luabridge::LuaRef(*obj);
    refs.push_back(ref);
    return ref;
}

int LuaEngine::traceback_lua(lua_State* lua)
{
    std::string& last_traceback = LuaEngine::self().last_traceback;
    last_traceback.clear();

    const char* msg = lua_tostring(lua, 1);

    // retrieve Lua's global 'debug.traceback' function
    lua_getglobal(lua, "debug");
    lua_getfield(lua, -1, "traceback");
    lua_remove(lua, -2);

    // push the original message and call level 2 (skipping this handler frame)
    lua_pushstring(lua, msg);
    lua_pushinteger(lua, 2);

    // call debug.traceback(message, 2)
    lua_call(lua, 2, 1);

    // traceback string is now at the top of the stack; save it for call_lua
    if (lua_isstring(lua, -1)) {
        size_t len = 0;
        const char* str = lua_tolstring(lua, -1, &len);
        last_traceback.assign(str, len);
    }

    return 1;
}

void LuaEngine::warn_deprecated(const char* name, const char* replacement)
{
    Log::warning("Function `{}` is deprecated and will be removed in a "
                 "future release, use `{}` instead",
                 name, replacement);
    Text::self().set_status(std::format("Warning: {} is deprecated", name));
}

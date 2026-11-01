// SPDX-License-Identifier: MIT
// Lua binding.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "luabinding.hpp"

#include "application.hpp"
#include "formatfactory.hpp"
#include "render.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <utility>

namespace {

// app modes table: type to name
constexpr std::array appmodes =
    std::to_array<std::pair<AppMode::Type, const char*>>({
        { AppMode::Viewer,    "viewer"    },
        { AppMode::Slideshow, "slideshow" },
        { AppMode::Gallery,   "gallery"   },
});

// image list orders: type to name
constexpr std::array ilorders =
    std::to_array<std::pair<ImageList::Order, const char*>>({
        { ImageList::Order::None,    "none"    },
        { ImageList::Order::Alpha,   "alpha"   },
        { ImageList::Order::Numeric, "numeric" },
        { ImageList::Order::Mtime,   "mtime"   },
        { ImageList::Order::Size,    "size"    },
        { ImageList::Order::Random,  "random"  },
});

// text block position: type to name
constexpr std::array tbpositions =
    std::to_array<std::pair<Text::Position, const char*>>({
        { Text::Position::TopLeft,     "topleft"     },
        { Text::Position::TopRight,    "topright"    },
        { Text::Position::BottomLeft,  "bottomleft"  },
        { Text::Position::BottomRight, "bottomright" },
});

// automatic positioning: type to name
constexpr std::array imgautopos =
    std::to_array<std::pair<Viewer::AutoPos, const char*>>({
        { Viewer::AutoPos::Free,   "free"   },
        { Viewer::AutoPos::Center, "center" },
        { Viewer::AutoPos::Follow, "follow" },
});

// fixed scale: type to name
constexpr std::array scales =
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
constexpr std::array imgpositions =
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

// image list directions: type to name
constexpr std::array ildirs =
    std::to_array<std::pair<ImageList::Dir, const char*>>({
        { ImageList::Dir::First,      "first"    },
        { ImageList::Dir::Last,       "last"     },
        { ImageList::Dir::Next,       "next"     },
        { ImageList::Dir::Prev,       "prev"     },
        { ImageList::Dir::NextParent, "next_dir" },
        { ImageList::Dir::PrevParent, "prev_dir" },
        { ImageList::Dir::Random,     "random"   },
});

// window background mode: type to name
constexpr std::array wndbkgs =
    std::to_array<std::pair<Viewer::Background, const char*>>({
        { Viewer::Background::Mirror, "mirror" },
        { Viewer::Background::Extend, "extend" },
        { Viewer::Background::Auto,   "auto"   },
});

// thumbnail aspect ratio: type to name
constexpr std::array aspects =
    std::to_array<std::pair<Gallery::Aspect, const char*>>({
        { Gallery::Aspect::Fit,  "fit"  },
        { Gallery::Aspect::Fill, "fill" },
        { Gallery::Aspect::Keep, "keep" },
});

// set current selection in gallery: type to name
constexpr std::array gldirs =
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

/**
 * Get type from name.
 * @param arr table with all possible pairs type->name
 * @param name name to convert
 * @return type or nullopt if name not found
 */
template <typename T, size_t N>
std::optional<T>
name_to_type(const std::array<std::pair<T, const char*>, N>& arr,
             const std::string& name)
{
    for (const auto& it : arr) {
        if (name == it.second) {
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
 * Construct Lua table from image entry.
 * @param lua Lua engine instance
 * @param entry image entry
 * @return Lua table reference
 */
luabridge::LuaRef entry_to_table(const LuaEngine& lua, const ImageEntry& entry)
{
    luabridge::LuaRef table = luabridge::newTable(lua);
    table["path"] = entry.path.string();
    table["index"] = entry.index + 1;
    table["size"] = entry.size;
    table["mtime"] = entry.mtime;
    table["mark"] = entry.mark;
    return table;
}

/**
 * Convert Lua reference to array of strings.
 * @param lua Lua engine instance
 * @param value origin Lua reference
 * @return array of strings
 */
std::vector<std::string> ref_to_array(const LuaEngine& lua,
                                      const luabridge::LuaRef& value)
{
    std::vector<std::string> arr;
    if (value.isString()) {
        arr.emplace_back(value.tostring());
    } else if (value.isTable()) {
        const size_t arr_sz = value.length();
        arr.reserve(arr_sz);
        for (size_t i = 1; i <= arr_sz; ++i) {
            arr.emplace_back(value[i].tostring());
        }
    } else {
        lua.raise_error("Invalid argument type");
    }
    return arr;
}

} // anonymous namespace

void LuaRootApi::bind()
{
    // register timer for deferred procedure call
    Application::self().add_fdpoll(defer_timer, [this] {
        assert(defer_fn);
        defer_timer.reset(0, 0);
        lua.call(*defer_fn);
        lua.remove_ref(defer_fn);
        defer_fn = nullptr;
    });

    add_property("appid", &LuaRootApi::get_appid, &LuaRootApi::set_appid);
    add_property("mode", &LuaRootApi::get_mode, &LuaRootApi::set_mode);
    add_property("fullscreen", &LuaRootApi::get_fullscreen,
                 &LuaRootApi::set_fullscreen);
    add_property_wo("overlay", &LuaRootApi::set_overlay);
    add_property_wo("decoration", &LuaRootApi::set_decoration);
    add_property("antialiasing", &LuaRootApi::get_antialiasing,
                 &LuaRootApi::set_antialiasing);
    add_property("exif_orientation", &LuaRootApi::get_exif_orientation,
                 &LuaRootApi::set_exif_orientation);
    add_property_wo("title", &LuaRootApi::set_title);
    add_property_wo("dnd_button", &LuaRootApi::set_dnd_button);
    add_function("on_dnd", &LuaRootApi::on_dnd);
    add_function("exit", &LuaRootApi::exit);
    add_function("get_window_size", &LuaRootApi::get_window_size);
    add_function("set_window_size", &LuaRootApi::set_window_size);
    add_function("on_window_resize", &LuaRootApi::on_window_resize);
    add_function("get_mouse_pos", &LuaRootApi::get_mouse_pos);
    add_function("on_initialized", &LuaRootApi::on_initialized);
    add_function("on_redrawn", &LuaRootApi::on_redrawn);
    add_function("defer", &LuaRootApi::defer);
    add_property_wo("format_conf", &LuaRootApi::set_format_conf);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::string LuaRootApi::get_appid()
{
    return Application::self().get_appid();
}

void LuaRootApi::set_appid(const std::string& value)
{
    if (Application::self().initialized()) {
        lua.raise_error("Application Id can be set only at startup");
    }
    if (value.empty()) {
        lua.raise_error("Application Id can not be empty");
    }
    Application::self().sparams->app_id.set(value);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::string LuaRootApi::get_mode()
{
    return type_to_name(appmodes, Application::self().get_mode());
}

void LuaRootApi::set_mode(const std::string& value)
{
    const auto mode = name_to_type(appmodes, value);
    if (!mode.has_value()) {
        lua.raise_error(R"(Invalid mode "{}")", value);
    }
    Application::self().set_mode(mode.value());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool LuaRootApi::get_fullscreen()
{
    const Ui* ui = Application::get_ui();
    if (ui) {
        return ui->get_fullscreen();
    }
    return Application::self().sparams->fullscreen.get();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaRootApi::set_fullscreen(const bool value)
{
    if (Application::self().initialized()) {
        Application::get_ui()->set_fullscreen(value);
    } else {
        Application::self().sparams->fullscreen.set(value);
    }
}

void LuaRootApi::set_overlay(const bool value)
{
    if (Application::self().initialized()) {
        lua.raise_error("Overlay can be set only at startup");
    }
    Application::self().sparams->use_overlay = value;
}

void LuaRootApi::set_decoration(const bool value)
{
    if (Application::self().initialized()) {
        lua.raise_error("Decoration can be set only at startup");
    }
    Application::self().sparams->decoration = value;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool LuaRootApi::get_antialiasing()
{
    return Render::self().antialiasing;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaRootApi::set_antialiasing(const bool value)
{
    Render::self().antialiasing = value;
    Application::redraw();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool LuaRootApi::get_exif_orientation()
{
    return FormatFactory::self().fix_orientation;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaRootApi::set_exif_orientation(const bool value)
{
    FormatFactory::self().fix_orientation = value;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaRootApi::set_title(const std::string& value)
{
    Ui* ui = Application::get_ui();
    if (ui) {
        ui->set_title(value.c_str());
    }
}

void LuaRootApi::set_dnd_button(const std::string& value)
{
    if (Application::self().initialized()) {
        lua.raise_error("DND can be set only at startup");
    }
    std::optional<InputMouse> input = InputMouse::load(value);
    if (!input.has_value()) {
        lua.raise_error(R"(Invalid button "{}")", value);
    }
    Application::self().sparams->dnd = input.value();
}

void LuaRootApi::on_dnd(const luabridge::LuaRef& value)
{
    Application& app = Application::self();
    if (value.isNil()) {
        app.on_drag_and_drop = nullptr;
    } else {
        const luabridge::LuaRef* fn = lua.add_ref(&value);
        app.on_drag_and_drop =
            [this, fn](const std::vector<std::filesystem::path>& paths) {
                std::vector<std::string> sp;
                sp.reserve(paths.size());
                for (const auto& it : paths) {
                    sp.emplace_back(it);
                }
                lua.call(*fn, sp);
            };
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaRootApi::exit(const std::optional<int>& value)
{
    Application::self().exit(value ? *value : 0);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
LuaRootApi::NamedTuple LuaRootApi::get_window_size()
{
    Size wnd;
    if (Application::self().initialized()) {
        wnd = Application::get_ui()->get_window_size();
    } else {
        wnd = Application::self().sparams->wnd_size;
    }
    return NamedTuple {
        { "width",  wnd.width  },
        { "height", wnd.height },
    };
}

void LuaRootApi::set_window_size(const size_t width, const size_t height)
{
    if (!width || !height || width > 32767 || height > 32767) {
        lua.raise_error("Invalid window size {}x{}", width, height);
    }
    if (Application::self().initialized()) {
        Application::get_ui()->set_window_size(
            { .width = width, .height = height });
    } else {
        Application::self().sparams->wnd_size = Size(width, height);
    }
}

void LuaRootApi::on_window_resize(const luabridge::LuaRef& value)
{
    Application& app = Application::self();
    if (value.isNil()) {
        app.on_wnd_resize = nullptr;
    } else {
        const luabridge::LuaRef* fn = lua.add_ref(&value);
        app.on_wnd_resize = [this, fn] {
            lua.call(*fn);
        };
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
LuaRootApi::NamedTuple LuaRootApi::get_mouse_pos()
{
    Point pos { .x = 0, .y = 0 };
    Ui* ui = Application::get_ui();
    if (ui) {
        pos = ui->get_mouse();
    }
    return NamedTuple {
        { "x", pos.x },
        { "y", pos.y },
    };
}

void LuaRootApi::on_initialized(const luabridge::LuaRef& value)
{
    const luabridge::LuaRef* fn = lua.add_ref(&value);
    Application::self().on_init_complete = [this, fn] {
        lua.call(*fn);
    };
}

void LuaRootApi::on_redrawn(const luabridge::LuaRef& value)
{
    Application& app = Application::self();
    if (value.isNil()) {
        app.on_redraw_complete = nullptr;
    } else {
        const luabridge::LuaRef* fn = lua.add_ref(&value);
        app.on_redraw_complete = [this, fn] {
            lua.call(*fn);
        };
    }
}

void LuaRootApi::defer(const double delay, const luabridge::LuaRef& cb)
{
    if (defer_fn) {
        lua.remove_ref(defer_fn);
        defer_fn = nullptr;
    }
    if (delay == 0 || cb.isNil()) {
        defer_timer.reset(0, 0);
        return;
    }
    const size_t ms = delay * 1000;
    defer_fn = lua.add_ref(&cb);
    defer_timer.reset(ms > 0 ? ms : 1, 0);
}

void LuaRootApi::set_format_conf(FormatParams& value)
{
    for (const auto& [format, params] : value) {
        ImageFormat* fmt = FormatFactory::self().get(format.c_str());
        if (!fmt) {
            return; // ignore errors
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
                        static_cast<size_t>(static_cast<luacolor_t>(value)));
                    break;
                case LUA_TSTRING:
                    fmtconf.set(name, static_cast<std::string>(value));
                    break;
                default:
                    lua.raise_error(
                        R"(Invalid type in parameter "{}" for format "{}")",
                        name, format);
            }
        }
        fmt->set_config(fmtconf);
        for (const auto& it : fmtconf.get(ImageFormat::Config::Invalid)) {
            lua.raise_error(
                R"(Invalid value in parameter "{}" for format "{}")", it,
                format);
        }
        for (const auto& it : fmtconf.get(ImageFormat::Config::Unhandled)) {
            lua.raise_error(R"(Unknown parameter "{}" for format "{}")", it,
                            format);
        }
    }
}

LuaImageListApi::LuaImageListApi(LuaEngine& lua, ImageList* imglist)
    : LuaBind(lua)
    , imglist(imglist)
{
}

void LuaImageListApi::bind()
{
    add_property("order", &LuaImageListApi::get_order,
                 &LuaImageListApi::set_order);
    add_property("reverse", &LuaImageListApi::get_reverse,
                 &LuaImageListApi::set_reverse);
    add_property("recursive", &LuaImageListApi::get_recursive,
                 &LuaImageListApi::set_recursive);
    add_property("adjacent", &LuaImageListApi::get_adjacent,
                 &LuaImageListApi::set_adjacent);
    add_property("fsmon", &LuaImageListApi::get_fsmon,
                 &LuaImageListApi::set_fsmon);
    add_property_ro("size", &LuaImageListApi::get_size);
    add_function("add", &LuaImageListApi::add);
    add_function("remove", &LuaImageListApi::remove);
    add_function("clear", &LuaImageListApi::clear);
    add_function("get", &LuaImageListApi::get);
}

std::vector<std::filesystem::path>
LuaImageListApi::ref_to_paths(const luabridge::LuaRef& value) const
{
    std::vector<std::filesystem::path> paths;
    if (value.isString()) {
        paths.emplace_back(value.tostring());
    } else if (value.isTable()) {
        const size_t arr_sz = value.length();
        paths.reserve(arr_sz);
        for (size_t i = 1; i <= arr_sz; ++i) {
            paths.emplace_back(value[i].tostring());
        }
    } else {
        lua.raise_error("Invalid argument type");
    }
    return paths;
}

std::string LuaImageListApi::get_order()
{
    return type_to_name(ilorders, imglist->get_order());
}

void LuaImageListApi::set_order(const std::string& value)
{
    const auto order = name_to_type(ilorders, value);
    if (!order.has_value()) {
        lua.raise_error(R"(Invalid image list order: "{}")", value);
    }
    imglist->set_order(order.value());
}

bool LuaImageListApi::get_reverse()
{
    return imglist->get_reverse();
}

void LuaImageListApi::set_reverse(const bool value)
{
    imglist->set_reverse(value);
}

bool LuaImageListApi::get_recursive()
{
    return imglist->recursive;
}

void LuaImageListApi::set_recursive(const bool value)
{
    imglist->recursive = value;
}

bool LuaImageListApi::get_adjacent()
{
    return imglist->adjacent;
}

void LuaImageListApi::set_adjacent(const bool value)
{
    imglist->adjacent = value;
}

bool LuaImageListApi::get_fsmon()
{
    return imglist->fsmon;
}

void LuaImageListApi::set_fsmon(const bool value)
{
    imglist->fsmon = value;
}

size_t LuaImageListApi::get_size()
{
    return imglist->size();
}

void LuaImageListApi::add(const luabridge::LuaRef& value)
{
    Application::self().add_images(ref_to_paths(value));
}

void LuaImageListApi::remove(const luabridge::LuaRef& value)
{
    Application::self().remove_images(ref_to_paths(value));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaImageListApi::clear()
{
    Application::self().remove_all_images();
}

luabridge::LuaRef LuaImageListApi::get()
{
    luabridge::LuaRef table = luabridge::newTable(lua);
    size_t index = 0;
    for (const auto& it : ImageList::self().get_all()) {
        table[++index] = entry_to_table(lua, *it);
    }
    return table;
}

LuaTextApi::LuaTextApi(LuaEngine& lua, Text* text)
    : LuaBind(lua)
    , text(text)
{
}

void LuaTextApi::bind()
{
    add_property("visible", &LuaTextApi::get_visible, &LuaTextApi::set_visible);
    add_property_wo("timeout", &LuaTextApi::set_timeout);
    add_property_wo("status_timeout", &LuaTextApi::set_status_timeout);
    add_property_wo("font", &LuaTextApi::set_font);
    add_property_wo("size", &LuaTextApi::set_size);
    add_property_wo("spacing", &LuaTextApi::set_spacing);
    add_property_wo("padding", &LuaTextApi::set_padding);
    add_property_wo("color", &LuaTextApi::set_color);
    add_property_wo("background", &LuaTextApi::set_background);
    add_property_wo("shadow", &LuaTextApi::set_shadow);
    add_property_wo("status", &LuaTextApi::set_status);
}

bool LuaTextApi::get_visible()
{
    return text->is_visible();
}

void LuaTextApi::set_visible(const bool value)
{
    if (value) {
        text->show();
    } else {
        text->hide();
    }
}

void LuaTextApi::set_timeout(const double value)
{
    text->set_overall_timer(value * 1000);
}

void LuaTextApi::set_status_timeout(const double value)
{
    text->set_status_timer(value * 1000);
}

void LuaTextApi::set_font(const std::string& value)
{
    text->set_font(value);
}

void LuaTextApi::set_size(const size_t value)
{
    text->set_size(value);
}

void LuaTextApi::set_spacing(const ssize_t value)
{
    text->set_spacing(value);
}

void LuaTextApi::set_padding(const size_t value)
{
    text->set_padding(value);
}

void LuaTextApi::set_color(const luacolor_t value)
{
    text->set_foreground(value);
}

void LuaTextApi::set_background(const luacolor_t value)
{
    text->set_background(value);
}

void LuaTextApi::set_shadow(const luacolor_t value)
{
    text->set_shadow(value);
}

void LuaTextApi::set_status(const std::string& value)
{
    text->set_status(value);
}

LuaAppModeApi::LuaAppModeApi(LuaEngine& lua, AppMode* appmode)
    : LuaBind(lua)
    , mode(appmode)
{
}

void LuaAppModeApi::bind()
{
    add_property_wo("mark_color", &LuaAppModeApi::set_mark_color);
    add_property_wo("pinch_factor", &LuaAppModeApi::set_pinch_factor);
    add_property_wo("text", &LuaAppModeApi::set_text);
    add_function("mark_image", &LuaAppModeApi::mark_image);
    add_function("bind_reset", &LuaAppModeApi::bind_reset);
    add_function("on_key", &LuaAppModeApi::on_key);
    add_function("on_unassigned_key", &LuaAppModeApi::on_unassigned_key);
    add_function("on_scroll", &LuaAppModeApi::on_scroll);
    add_function("on_mouse", &LuaAppModeApi::on_mouse);
    add_function("on_signal", &LuaAppModeApi::on_signal);
    add_function("on_image_change", &LuaAppModeApi::on_image_change);
}

void LuaAppModeApi::check_active()
{
    if (Application::self().current_mode() != mode) {
        lua.raise_error("Required mode is not active");
    }
}

void LuaAppModeApi::set_mark_color(const luacolor_t value)
{
    mode->set_mark_color(value);
}

void LuaAppModeApi::set_pinch_factor(const double value)
{
    mode->set_pinch_factor(value);
}

void LuaAppModeApi::set_text(
    const std::unordered_map<std::string, Text::Scheme>& value)
{
    for (const auto& [pos, content] : value) {
        const auto bp = name_to_type(tbpositions, pos);
        if (!bp.has_value()) {
            lua.raise_error(R"(Invalid position name "{}")", pos);
        }
        mode->set_text_scheme(bp.value(), content);
    }
}

void LuaAppModeApi::mark_image(const std::optional<bool>& value)
{
    mode->mark_current(value);
}

void LuaAppModeApi::bind_reset()
{
    mode->bind_reset();
}

void LuaAppModeApi::on_key(const luabridge::LuaRef& key,
                           const luabridge::LuaRef& cb)
{
    for (const auto& key : ref_to_array(lua, key)) {
        std::optional<InputKeyboard> input = InputKeyboard::load(key);
        if (!input.has_value()) {
            lua.raise_error(R"(Invalid key: "{}")", key);
        }
        const luabridge::LuaRef* ref = lua.add_ref(&cb);
        mode->bind_input(*input, [this, ref] {
            lua.call(*ref);
        });
    }
}

void LuaAppModeApi::on_unassigned_key(const luabridge::LuaRef& value)
{
    const luabridge::LuaRef* ref = lua.add_ref(&value);
    mode->on_unassigned_key = [this, ref](const InputKeyboard& input) {
        lua.call(*ref, input.to_string().c_str());
    };
}

void LuaAppModeApi::on_scroll(const luabridge::LuaRef& value)
{
    const luabridge::LuaRef* ref = lua.add_ref(&value);
    mode->on_scroll = [this, ref](const keymod_t kmods, const double delta_h,
                                  const double delta_v) {
        lua.call(*ref, InputKeyboard::mods_to_string(kmods), delta_h, delta_v);
    };
}

void LuaAppModeApi::on_mouse(const std::string& key,
                             const luabridge::LuaRef& cb)
{
    std::optional<InputMouse> input = InputMouse::load(key);
    if (!input.has_value()) {
        // mouse buttons removed in favor of on_scroll()
        constexpr std::array removed_scroll_buttons = {
            "ScrollUp",
            "ScrollDown",
            "ScrollLeft",
            "ScrollRight",
        };
        for (const char* button : removed_scroll_buttons) {
            if (key.find(button) != std::string::npos) {
                lua.raise_error(
                    R"(Invalid button: "{}" was removed, use on_scroll() instead)",
                    button);
            }
        }
        lua.raise_error(R"(Invalid button "{}")", key);
    }
    const luabridge::LuaRef* ref = lua.add_ref(&cb);
    mode->bind_input(*input, [this, ref] {
        lua.call(*ref);
    });
}

void LuaAppModeApi::on_signal(const std::string& sig,
                              const luabridge::LuaRef& cb)
{
    std::optional<InputSignal> input = InputSignal::load(sig);
    if (!input.has_value()) {
        lua.raise_error(R"(Invalid signal "{}")", sig);
    }
    const luabridge::LuaRef* ref = lua.add_ref(&cb);
    mode->bind_input(*input, [this, ref] {
        lua.call(*ref);
    });
}

void LuaAppModeApi::on_image_change(const luabridge::LuaRef& value)
{
    if (value.isNil()) {
        mode->on_image_change = nullptr;
    } else {
        const luabridge::LuaRef* ref = lua.add_ref(&value);
        mode->on_image_change = [this, ref] {
            lua.call(*ref);
        };
    }
}

LuaViewerApi::LuaViewerApi(LuaEngine& lua, Viewer* viewer)
    : LuaAppModeApi(lua, viewer)
{
}

void LuaViewerApi::bind()
{
    LuaAppModeApi::bind();
    add_property_wo("autopos", &LuaViewerApi::set_autopos);
    add_property_wo("loop", &LuaViewerApi::set_loop);
    add_property_wo("default_scale", &LuaViewerApi::set_default_scale);
    add_property_wo("default_position", &LuaViewerApi::set_default_position);
    add_property("scale", &LuaViewerApi::get_scale, &LuaViewerApi::set_scale);
    add_property("animation", &LuaViewerApi::get_animation,
                 &LuaViewerApi::set_animation);
    add_property("frame", &LuaViewerApi::get_frame, &LuaViewerApi::set_frame);
    add_property_wo("focus_radius", &LuaViewerApi::set_focus_radius);
    add_property_wo("focus_factor", &LuaViewerApi::set_focus_factor);
    add_property_wo("drag_button", &LuaViewerApi::set_drag_button);
    add_property_wo("preload", &LuaViewerApi::set_preload);
    add_property_wo("history", &LuaViewerApi::set_history);
    add_function("open", &LuaViewerApi::open);
    add_function("open_path", &LuaViewerApi::open_path);
    add_function("get_image", &LuaViewerApi::get_image);
    add_function("reload", &LuaViewerApi::reload);
    add_function("set_abs_scale", &LuaViewerApi::set_abs_scale);
    add_function("set_fix_scale", &LuaViewerApi::set_fix_scale);
    add_function("reset", &LuaViewerApi::reset);
    add_function("get_position", &LuaViewerApi::get_position);
    add_function("set_abs_position", &LuaViewerApi::set_abs_position);
    add_function("set_fix_position", &LuaViewerApi::set_fix_position);
    add_function("flip_vertical", &LuaViewerApi::flip_vertical);
    add_function("flip_horizontal", &LuaViewerApi::flip_horizontal);
    add_function("rotate", &LuaViewerApi::rotate);
    add_function("export_image", &LuaViewerApi::export_image);
    add_function("set_meta", &LuaViewerApi::set_meta);
    add_function("set_window_background", &LuaViewerApi::set_window_background);
    add_function("set_image_background", &LuaViewerApi::set_image_background);
    add_function("set_image_chessboard", &LuaViewerApi::set_image_chessboard);
}

void LuaViewerApi::set_autopos(const std::string& value)
{
    const auto pos = name_to_type(imgautopos, value);
    if (!pos.has_value()) {
        lua.raise_error(R"(Invalid argument "{}")", value);
    }
    static_cast<Viewer*>(mode)->auto_pos = pos.value();
}

void LuaViewerApi::set_loop(const bool value)
{
    static_cast<Viewer*>(mode)->imagelist_loop = value;
}

void LuaViewerApi::set_default_scale(const luabridge::LuaRef& value)
{
    if (value.isString()) {
        const std::string str = value;
        const auto scale = name_to_type(scales, str);
        if (!scale.has_value()) {
            lua.raise_error(R"(Invalid scale "{}")", str);
        }
        static_cast<Viewer*>(mode)->default_scale = scale.value();
    } else if (value.isNumber()) {
        static_cast<Viewer*>(mode)->default_scale = static_cast<double>(value);
    } else {
        lua.raise_error(R"(Invalid scale "{}")", value.tostring());
    }
}

void LuaViewerApi::set_default_position(const std::string& value)
{
    const auto pos = name_to_type(imgpositions, value);
    if (!pos.has_value()) {
        lua.raise_error(R"(Invalid position "{}")", value);
    }
    static_cast<Viewer*>(mode)->default_pos = pos.value();
}

double LuaViewerApi::get_scale()
{
    check_active();
    return static_cast<Viewer*>(mode)->get_scale();
}

void LuaViewerApi::set_scale(const double value)
{
    check_active();
    Point pos { .x = 0, .y = 0 };
    Ui* ui = Application::get_ui();
    if (ui) {
        pos = ui->get_mouse();
    }
    static_cast<Viewer*>(mode)->set_scale(value, pos);
}

bool LuaViewerApi::get_animation()
{
    check_active();
    return static_cast<Viewer*>(mode)->animation_enabled();
}

void LuaViewerApi::set_animation(const bool value)
{
    check_active();
    static_cast<Viewer*>(mode)->enable_animation(value);
}

size_t LuaViewerApi::get_frame()
{
    check_active();
    return static_cast<Viewer*>(mode)->get_frame();
}

void LuaViewerApi::set_frame(const size_t value)
{
    check_active();
    static_cast<Viewer*>(mode)->enable_animation(false);
    static_cast<Viewer*>(mode)->set_frame(value);
}

void LuaViewerApi::set_focus_radius(const size_t value)
{
    static_cast<Viewer*>(mode)->set_focus_radius(value);
}

void LuaViewerApi::set_focus_factor(const double value)
{
    static_cast<Viewer*>(mode)->set_focus_factor(value);
}

void LuaViewerApi::set_drag_button(const std::string& value)
{
    std::optional<InputMouse> input = InputMouse::load(value);
    if (!input.has_value()) {
        lua.raise_error(R"(Invalid mouse button "{}")", value);
    }
    static_cast<Viewer*>(mode)->bind_image_drag(input.value());
}

void LuaViewerApi::set_preload(const size_t value)
{
    static_cast<Viewer*>(mode)->set_preload_limit(value);
}

void LuaViewerApi::set_history(const size_t value)
{
    static_cast<Viewer*>(mode)->set_history_limit(value);
}

bool LuaViewerApi::open(const std::string& value)
{
    check_active();

    const auto dir = name_to_type(ildirs, value);
    if (!dir.has_value()) {
        lua.raise_error(R"(Invalid direction "{}")", value);
    }
    return static_cast<Viewer*>(mode)->open(dir.value());
}

bool LuaViewerApi::open_path(const std::string& value)
{
    check_active();

    ImageEntryPtr entry = ImageList::self().find(value);
    if (!entry) {
        entry = Application::self().add_images({ value });
    }
    return entry && mode->set_current(entry);
}

luabridge::LuaRef LuaViewerApi::get_image()
{
    check_active();

    const ImagePtr image = static_cast<Viewer*>(mode)->current_image();
    if (!image) {
        return luabridge::LuaRef(lua, luabridge::LuaNil {});
    }
    luabridge::LuaRef tbl = entry_to_table(lua, *image->entry);
    tbl["format"] = image->format;
    tbl["frames"] = image->frames.size();
    tbl["width"] = image->frames[0].pm.width();
    tbl["height"] = image->frames[0].pm.height();
    const luabridge::LuaRef meta = luabridge::newTable(lua);
    for (const auto& [key, value] : image->meta) {
        meta[key] = value;
    }
    tbl["meta"] = meta;
    return tbl;
}

void LuaViewerApi::reload()
{
    check_active();
    static_cast<Viewer*>(mode)->reload();
}

void LuaViewerApi::set_abs_scale(const double scale,
                                 const std::optional<ssize_t>& x,
                                 const std::optional<ssize_t>& y)
{
    check_active();

    Point preserve;
    if (x.has_value() && y.has_value()) {
        preserve.x = x.value();
        preserve.y = y.value();
    }
    static_cast<Viewer*>(mode)->set_scale(scale, preserve);
}

void LuaViewerApi::set_fix_scale(const std::string& value)
{
    check_active();

    const auto scale = name_to_type(scales, value);
    if (!scale.has_value()) {
        lua.raise_error(R"(Invalid scale "{}")", value);
    }
    static_cast<Viewer*>(mode)->set_scale(scale.value());
}

void LuaViewerApi::reset()
{
    check_active();
    static_cast<Viewer*>(mode)->reset();
}

std::unordered_map<std::string, ssize_t> LuaViewerApi::get_position()
{
    check_active();

    const Point pos = static_cast<Viewer*>(mode)->get_position();
    return std::unordered_map<std::string, ssize_t> {
        { "x", pos.x },
        { "y", pos.y },
    };
}

void LuaViewerApi::set_abs_position(const ssize_t x, const ssize_t y)
{
    check_active();
    static_cast<Viewer*>(mode)->set_position({ .x = x, .y = y });
}

void LuaViewerApi::set_fix_position(const std::string& value)
{
    check_active();

    const auto pos = name_to_type(imgpositions, value);
    if (!pos.has_value()) {
        lua.raise_error(R"(Invalid position "{}")", value);
    }
    static_cast<Viewer*>(mode)->set_position(pos.value());
}

void LuaViewerApi::flip_vertical()
{
    check_active();
    static_cast<Viewer*>(mode)->flip_vertical();
}

void LuaViewerApi::flip_horizontal()
{
    check_active();
    static_cast<Viewer*>(mode)->flip_horizontal();
}

void LuaViewerApi::rotate(const size_t value)
{
    check_active();

    if (value != 90 && value != 180 && value != 270) {
        lua.raise_error(R"(Invalid angle "{}")", value);
    }
    static_cast<Viewer*>(mode)->rotate(value);
}

void LuaViewerApi::export_image(const std::string& value)
{
    check_active();
    static_cast<Viewer*>(mode)->export_frame(value);
}

void LuaViewerApi::set_meta(const std::string& key, const std::string& val)
{
    check_active();

    const ImagePtr image = static_cast<Viewer*>(mode)->current_image();
    if (!image) {
        return;
    }

    // remove "meta." from key
    std::string meta_key;
    const std::string meta_prefix = std::string(Text::FIELD_META) + ".";
    if (key.starts_with(meta_prefix)) {
        meta_key = key.substr(meta_prefix.length());
    } else {
        meta_key = key;
    }
    if (meta_key.empty()) {
        lua.raise_error("Empty key for meta");
    }

    // update meta in image
    if (val.empty()) {
        image->meta.erase(meta_key);
    } else {
        image->meta.insert_or_assign(meta_key, val);
    }

    // update text layer
    Text& text = Text::self();
    text.set_field(std::string(Text::FIELD_META) + "." + meta_key, val);
    text.update();
    Application::redraw();
}

void LuaViewerApi::set_window_background(const luabridge::LuaRef& value)
{
    if (value.isString()) {
        const std::string str = value;
        const auto bgmode = name_to_type(wndbkgs, str);
        if (!bgmode.has_value()) {
            lua.raise_error(R"(Invalid background mode "{}")", str);
        }
        static_cast<Viewer*>(mode)->set_window_background(bgmode.value());
    } else if (value.isNumber()) {
        static_cast<Viewer*>(mode)->set_window_background(
            static_cast<luacolor_t>(value));
    } else {
        lua.raise_error(R"(Invalid background value "{}")", value.tostring());
    }
}

void LuaViewerApi::set_image_background(const luacolor_t value)
{
    static_cast<Viewer*>(mode)->set_image_background(value);
}

void LuaViewerApi::set_image_chessboard(const size_t sz, const luacolor_t clr0,
                                        const luacolor_t clr1)
{
    static_cast<Viewer*>(mode)->set_image_chessboard(sz, clr0, clr1);
}

LuaSlideshowApi::LuaSlideshowApi(LuaEngine& lua, Slideshow* slideshow)
    : LuaViewerApi(lua, slideshow)
{
}

void LuaSlideshowApi::bind()
{
    LuaViewerApi::bind();
    add_property("timeout", &LuaSlideshowApi::get_timeout,
                 &LuaSlideshowApi::set_timeout);
}

double LuaSlideshowApi::get_timeout()
{
    return static_cast<double>(static_cast<Slideshow*>(mode)->duration) / 1000;
}

void LuaSlideshowApi::set_timeout(const double value)
{
    static_cast<Slideshow*>(mode)->duration = value * 1000;
}

LuaGalleryApi::LuaGalleryApi(LuaEngine& lua, Gallery* gallery)
    : LuaAppModeApi(lua, gallery)
{
}

void LuaGalleryApi::bind()
{
    LuaAppModeApi::bind();
    add_property_wo("aspect", &LuaGalleryApi::set_aspect);
    add_property("thumb_size", &LuaGalleryApi::get_thumb_size,
                 &LuaGalleryApi::set_thumb_size);
    add_property_wo("padding_size", &LuaGalleryApi::set_padding_size);
    add_property_wo("border_size", &LuaGalleryApi::set_border_size);
    add_property_wo("selected_scale", &LuaGalleryApi::set_selected_scale);
    add_property_wo("window_color", &LuaGalleryApi::set_window_color);
    add_property_wo("border_color", &LuaGalleryApi::set_border_color);
    add_property_wo("selected_color", &LuaGalleryApi::set_selected_color);
    add_property_wo("unselected_color", &LuaGalleryApi::set_unselected_color);
    add_property_wo("hover", &LuaGalleryApi::set_hover);
    add_property_wo("pstore", &LuaGalleryApi::set_pstore);
    add_property_wo("pstore_path", &LuaGalleryApi::set_pstore_path);
    add_property_wo("preload", &LuaGalleryApi::set_preload);
    add_property_wo("cache", &LuaGalleryApi::set_cache);
    add_property("embedded_thumb", &LuaGalleryApi::get_embedded_thumb,
                 &LuaGalleryApi::set_embedded_thumb);
    add_function("select", &LuaGalleryApi::select);
    add_function("select_at", &LuaGalleryApi::select_at);
    add_function("select_path", &LuaGalleryApi::select_path);
    add_function("reload", &LuaGalleryApi::reload);
    add_function("get_image", &LuaGalleryApi::get_image);
}

void LuaGalleryApi::set_aspect(const std::string& value)
{
    const auto aspect = name_to_type(aspects, value);
    if (!aspect.has_value()) {
        lua.raise_error(R"(Invalid aspect "{}")", value);
    }
    static_cast<Gallery*>(mode)->set_thumb_aspect(aspect.value());
}

size_t LuaGalleryApi::get_thumb_size()
{
    return static_cast<Gallery*>(mode)->get_thumb_size();
}

void LuaGalleryApi::set_thumb_size(const size_t value)
{
    static_cast<Gallery*>(mode)->set_thumb_size(value);
}

void LuaGalleryApi::set_padding_size(const size_t value)
{
    static_cast<Gallery*>(mode)->set_padding_size(value);
}

void LuaGalleryApi::set_border_size(const size_t value)
{
    static_cast<Gallery*>(mode)->set_border_size(value);
}

void LuaGalleryApi::set_selected_scale(const double value)
{
    static_cast<Gallery*>(mode)->set_selected_scale(value);
}

void LuaGalleryApi::set_window_color(const luacolor_t value)
{
    static_cast<Gallery*>(mode)->set_window_color(value);
}

void LuaGalleryApi::set_border_color(const luacolor_t value)
{
    static_cast<Gallery*>(mode)->set_background_color(value);
}

void LuaGalleryApi::set_selected_color(const luacolor_t value)
{
    static_cast<Gallery*>(mode)->set_selected_color(value);
}

void LuaGalleryApi::set_unselected_color(const luacolor_t value)
{
    static_cast<Gallery*>(mode)->set_border_color(value);
}

void LuaGalleryApi::set_hover(const bool value)
{
    static_cast<Gallery*>(mode)->enable_hover(value);
}

void LuaGalleryApi::set_pstore(const bool value)
{
    static_cast<Gallery*>(mode)->enable_pstore(value);
}

void LuaGalleryApi::set_pstore_path(const std::string& value)
{
    static_cast<Gallery*>(mode)->set_pstore_path(value);
}

void LuaGalleryApi::set_preload(const bool value)
{
    static_cast<Gallery*>(mode)->enable_preload(value);
}

void LuaGalleryApi::set_cache(const size_t value)
{
    static_cast<Gallery*>(mode)->set_cache_size(value);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool LuaGalleryApi::get_embedded_thumb()
{
    return FormatFactory::self().embedded_thumb;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void LuaGalleryApi::set_embedded_thumb(const bool value)
{
    FormatFactory::self().embedded_thumb = value;
}

bool LuaGalleryApi::select(const std::string& value)
{
    check_active();

    const auto dir = name_to_type(gldirs, value);
    if (!dir.has_value()) {
        lua.raise_error(R"(Invalid direction "{}")", value);
    }
    return static_cast<Gallery*>(mode)->select(dir.value());
}

bool LuaGalleryApi::select_at(const size_t x, const size_t y)
{
    check_active();
    return static_cast<Gallery*>(mode)->select(Point(x, y));
}

bool LuaGalleryApi::select_path(const std::string& value)
{
    check_active();

    ImageEntryPtr entry = ImageList::self().find(value);
    if (!entry) {
        entry = Application::self().add_images({ value });
    }
    return entry && static_cast<Gallery*>(mode)->set_current(entry);
}

void LuaGalleryApi::reload()
{
    check_active();
    static_cast<Gallery*>(mode)->reload();
}

luabridge::LuaRef LuaGalleryApi::get_image()
{
    check_active();
    const ImageEntryPtr entry = mode->get_current();
    if (entry) {
        return entry_to_table(lua, *entry);
    }
    return luabridge::LuaRef(lua, luabridge::LuaNil {});
}

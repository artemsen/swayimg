// SPDX-License-Identifier: MIT
// Lua bindings.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"
#include "fdevent.hpp"
#include "gallery.hpp"
#include "imagelist.hpp"
#include "luaengine.hpp"
#include "slideshow.hpp"
#include "text.hpp"
#include "viewer.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/** Root API binding. */
class LuaRootApi : public LuaBind {
public:
    using LuaBind::LuaBind;

protected:
    void bind() override;

private:
    using NamedTuple = std::unordered_map<std::string, size_t>;
    using FormatParams = const std::unordered_map<
        std::string, std::unordered_map<std::string, luabridge::LuaRef>>;

    // binding handlers
    std::string get_appid();
    void set_appid(const std::string& value);
    std::string get_mode();
    void set_mode(const std::string& value);
    bool get_fullscreen();
    void set_fullscreen(const bool value);
    void set_overlay(const bool value);
    void set_decoration(const bool value);
    bool get_antialiasing();
    void set_antialiasing(const bool value);
    bool get_exif_orientation();
    void set_exif_orientation(const bool value);
    void set_title(const std::string& value);
    void set_dnd_button(const std::string& value);
    void on_dnd(const luabridge::LuaRef& value);
    void exit(const std::optional<int>& value);
    NamedTuple get_window_size();
    void set_window_size(const size_t width, const size_t height);
    void on_window_resize(const luabridge::LuaRef& value);
    NamedTuple get_mouse_pos();
    void on_initialized(const luabridge::LuaRef& value);
    void on_redrawn(const luabridge::LuaRef& value);
    void defer(const double delay, const luabridge::LuaRef& cb);
    void set_format_conf(FormatParams& value);

private:
    FdTimer defer_timer;                         ///< Timer for deferred call
    const luabridge::LuaRef* defer_fn = nullptr; ///< Deferred callback
};

/** Image list API binding. */
class LuaImageListApi : public LuaBind {
public:
    LuaImageListApi(LuaEngine& lua, ImageList* imglist);
    static constexpr const char* NS = "imagelist";

protected:
    void bind() override;

private:
    /**
     * Convert Lua reference to array of paths.
     * @param ref origin Lua reference
     * @return array of paths
     */
    [[nodiscard]] std::vector<std::filesystem::path>
    ref_to_paths(const luabridge::LuaRef& value) const;

    // binding handlers
    std::string get_order();
    void set_order(const std::string& value);
    bool get_reverse();
    void set_reverse(const bool value);
    bool get_recursive();
    void set_recursive(const bool value);
    bool get_adjacent();
    void set_adjacent(const bool value);
    bool get_fsmon();
    void set_fsmon(const bool value);
    size_t get_size();
    void add(const luabridge::LuaRef& value);
    void remove(const luabridge::LuaRef& value);
    void clear();
    luabridge::LuaRef get();

private:
    ImageList* imglist; ///< Image list instance
};

/** Text API binding. */
class LuaTextApi : public LuaBind {
public:
    LuaTextApi(LuaEngine& lua, Text* text);
    static constexpr const char* NS = "text";

protected:
    void bind() override;

private:
    // binding handlers
    bool get_visible();
    void set_visible(const bool value);
    void set_timeout(const double value);
    void set_status_timeout(const double value);
    void set_font(const std::string& value);
    void set_size(const size_t value);
    void set_spacing(const ssize_t value);
    void set_padding(const size_t value);
    void set_color(const luacolor_t value);
    void set_background(const luacolor_t value);
    void set_shadow(const luacolor_t value);
    void set_status(const std::string& value);

private:
    Text* text; ///< Text layer instance
};

/** App mode API binding. */
class LuaAppModeApi : public LuaBind {
protected:
    LuaAppModeApi(LuaEngine& lua, AppMode* appmode);
    void bind() override;

    /**
     * Check if current mode is active.
     * Raises Lua error if current mode is not active.
     */
    void check_active();

private:
    // binding handlers
    void set_mark_color(const luacolor_t value);
    void set_pinch_factor(const double value);
    void set_text(const std::unordered_map<std::string, Text::Scheme>& value);
    void mark_image(const std::optional<bool>& value);
    void bind_reset();
    void on_key(const luabridge::LuaRef& key, const luabridge::LuaRef& cb);
    void on_unassigned_key(const luabridge::LuaRef& value);
    void on_scroll(const luabridge::LuaRef& value);
    void on_mouse(const std::string& key, const luabridge::LuaRef& cb);
    void on_signal(const std::string& sig, const luabridge::LuaRef& cb);
    void on_image_change(const luabridge::LuaRef& value);

protected:
    AppMode* mode; ///< Application mode instance
};

/** Viewer API binding. */
class LuaViewerApi : public LuaAppModeApi {
public:
    LuaViewerApi(LuaEngine& lua, Viewer* viewer);
    static constexpr const char* NS = "viewer";

protected:
    void bind() override;

private:
    // binding handlers
    void set_autopos(const std::string& value);
    void set_loop(const bool value);
    void set_default_scale(const luabridge::LuaRef& value);
    void set_default_position(const std::string& value);
    double get_scale();
    void set_scale(const double value);
    bool get_animation();
    void set_animation(const bool value);
    size_t get_frame();
    void set_frame(const size_t value);
    void set_focus_radius(const size_t value);
    void set_focus_factor(const double value);
    void set_drag_button(const std::string& value);
    void set_preload(const size_t value);
    void set_history(const size_t value);
    bool open(const std::string& value);
    bool open_path(const std::string& value);
    luabridge::LuaRef get_image();
    void reload();
    void set_abs_scale(const double scale, const std::optional<ssize_t>& x,
                       const std::optional<ssize_t>& y);
    void set_fix_scale(const std::string& value);
    void reset();
    std::unordered_map<std::string, ssize_t> get_position();
    void set_abs_position(const ssize_t x, const ssize_t y);
    void set_fix_position(const std::string& value);
    void flip_vertical();
    void flip_horizontal();
    void rotate(const size_t value);
    void export_image(const std::string& value);
    void set_meta(const std::string& key, const std::string& val);
    void set_window_background(const luabridge::LuaRef& value);
    void set_image_background(const luacolor_t value);
    void set_image_chessboard(const size_t sz, const luacolor_t clr0,
                              const luacolor_t clr1);
};

/** Slideshow API binding. */
class LuaSlideshowApi : public LuaViewerApi {
public:
    LuaSlideshowApi(LuaEngine& lua, Slideshow* slideshow);
    static constexpr const char* NS = "slideshow";

protected:
    void bind() override;

private:
    // binding handlers
    double get_timeout();
    void set_timeout(const double value);
};

/** Gallery API binding. */
class LuaGalleryApi : public LuaAppModeApi {
public:
    LuaGalleryApi(LuaEngine& lua, Gallery* gallery);
    static constexpr const char* NS = "gallery";

protected:
    void bind() override;

private:
    // binding handlers
    void set_aspect(const std::string& value);
    size_t get_thumb_size();
    void set_thumb_size(const size_t value);
    void set_padding_size(const size_t value);
    void set_border_size(const size_t value);
    void set_selected_scale(const double value);
    void set_window_color(const luacolor_t value);
    void set_border_color(const luacolor_t value);
    void set_selected_color(const luacolor_t value);
    void set_unselected_color(const luacolor_t value);
    void set_hover(const bool value);
    void set_pstore(const bool value);
    void set_pstore_path(const std::string& value);
    void set_preload(const bool value);
    void set_cache(const size_t value);
    bool get_embedded_thumb();
    void set_embedded_thumb(const bool value);
    bool select(const std::string& value);
    bool select_at(const size_t x, const size_t y);
    bool select_path(const std::string& value);
    void reload();
    luabridge::LuaRef get_image();
};

// SPDX-License-Identifier: MIT
// Default configuration.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "gallery.hpp"
#include "imagelist.hpp"
#include "slideshow.hpp"
#include "viewer.hpp"

#include <array>
#include <filesystem>

namespace Defaults {

// general app
namespace app {
    constexpr bool use_overlay = false;
    constexpr bool fullscreen = false;
    constexpr bool decoration = true;
    constexpr uint32_t cursor_hide = 3000;
    constexpr const char* app_id = "swayimg";
}

// image list
namespace imglist {
    constexpr ImageList::Order order = ImageList::Order::Numeric;
    constexpr bool reverse = false;
    constexpr bool recursive = false;
    constexpr bool adjacent = false;
    constexpr bool fsmon = true;
}

// text layer
namespace text {
    constexpr const char* font = "monospace";
    constexpr size_t size = 24;
    constexpr bool enable = true;
    constexpr size_t overall = 5000;
    constexpr size_t status = 3000;
    constexpr ssize_t spacing = 0;
    constexpr size_t padding = 10;
    constexpr argb_t foreground = { 0xff, 0xcc, 0xcc, 0xcc };
    constexpr argb_t background = { 0x00, 0x00, 0x00, 0x00 };
    constexpr argb_t shadow = { 0xd0, 0x00, 0x00, 0x00 };
}

// image loader
namespace img {
    constexpr bool fix_orientation = true;
    constexpr bool embedded_thumb = true;
}

// rendering
namespace render {
    constexpr bool antialiasing =
#ifdef NDEBUG
        true;
#else
        false;
#endif // NDEBUG
}

// viewer
namespace viewer {
    constexpr bool auto_center = true;
    constexpr bool imagelist_loop = true;
    constexpr Viewer::Scale scale = Viewer::Scale::Optimal;
    constexpr Viewer::Position position = Viewer::Position::Center;
    constexpr argb_t window_bkg = { argb_t::max, 0, 0, 0 };
    constexpr bool tr_chessboard = true;
    constexpr size_t tr_cbsize = 20;
    constexpr argb_t tr_cbcolor0 = { argb_t::max, 0x33, 0x33, 0x33 };
    constexpr argb_t tr_cbcolor1 = { argb_t::max, 0x4c, 0x4c, 0x4c };
    constexpr argb_t tr_bgcolor = { argb_t::max, 0, 0, 0 };
    constexpr bool animation = true;
    constexpr size_t preload = 1;
    constexpr size_t history = 1;
    constexpr double pinch_factor = 1.0;
    constexpr argb_t mark_color = { argb_t::max, 0x80, 0x80, 0x80 };

    constexpr std::array text_scheme_tl = {
        "File:\t{name}",
        "Format:\t{format}",
        "File size:\t{sizehr}",
        "File time:\t{time}",
        "EXIF date:\t{meta.Exif.Photo.DateTimeOriginal}",
        "EXIF camera:\t{meta.Exif.Image.Model}"
    };
    constexpr std::array text_scheme_tr = {
        "Image:\t{list.index} of {list.total}",
        "Frame:\t{frame.index} of {frame.total}",
        "Size:\t{frame.width}x{frame.height}"
    };
    constexpr std::array text_scheme_bl = { "Scale: {scale}" };

    /** Set default input bindings. */
    void bind_inputs(Viewer* mode);
}

// slide show
namespace slideshow {
    constexpr Slideshow::Scale scale = Slideshow::Scale::FitWindow;
    constexpr Slideshow::Background window_bkg = Slideshow::Background::Auto;
    constexpr size_t history = 0;
    constexpr size_t duration = 5000;
    constexpr std::array text_scheme_tl = { "{name}" };

    /** Set default input bindings. */
    void bind_inputs(Slideshow* mode);
}

// gallery
namespace gallery {
    constexpr Gallery::Aspect aspect = Gallery::Aspect::Fill;
    constexpr size_t border_size = 5;
    constexpr double selected_scale = 1.15;
    constexpr size_t thumb_size = 200;
    constexpr size_t thumb_padding = 5;
    constexpr argb_t clr_window = { argb_t::max, 0x00, 0x00, 0x00 };
    constexpr argb_t clr_background = { argb_t::max, 0x20, 0x20, 0x20 };
    constexpr argb_t clr_select = { argb_t::max, 0x40, 0x40, 0x40 };
    constexpr argb_t clr_border = { argb_t::max, 0xaa, 0xaa, 0xaa };
    constexpr bool hover_select = true;
    constexpr bool preload = false;
    constexpr size_t cache_size = 100;
    constexpr bool pstore_enable = false;
    constexpr double pinch_factor = 1.0;
    constexpr argb_t mark_color = { argb_t::max, 0x80, 0x80, 0x80 };
    constexpr std::array text_scheme_tl = { "File:\t{name}" };
    constexpr std::array text_scheme_tr = { "{list.index} of {list.total}" };

    /** Set default input bindings. */
    void bind_inputs(Gallery* mode);

    /**
     * Get default path for persistent storage.
     * @return default path to persistent storage
     */
    std::filesystem::path pstore_path();
}
} // namespace Defaults

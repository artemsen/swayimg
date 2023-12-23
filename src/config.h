// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

#include <xkbcommon/xkbcommon.h>

// Background modes
#define COLOR_TRANSPARENT 0xff000000
#define BACKGROUND_GRID   0xfe000000

// Copy position or size from parent window
#define SAME_AS_PARENT 0xffffffff
// Copy size from image
#define SAME_AS_IMAGE 0

/** Order of file list. */
enum config_order {
    cfgord_none,  ///< Unsorted (system depended)
    cfgord_alpha, ///< Alphanumeric sort
    cfgord_random ///< Random order
};

/** Initial scaling of images. */
enum config_scale {
    cfgsc_optimal, ///< Fit to window, but not more than 100%
    cfgsc_fit,     ///< Fit to window size
    cfgsc_fill,    ///< Fill the window
    cfgsc_real     ///< Real image size (100%)
};

/** App configuration. */
struct config {
    const char* app_id;      ///< Window class/app_id name
    bool sway_wm;            ///< Enable/disable integration with Sway WM
    struct rect geometry;    ///< Window geometry
    bool fullscreen;         ///< Full screen mode
    bool antialiasing;       ///< Anti-aliasing
    argb_t background;       ///< Image background mode/color
    argb_t window;           ///< Window background mode/color
    enum config_scale scale; ///< Initial scale
    bool show_info;          ///< Show image info
    const char* font_face;   ///< Font name
    size_t font_size;        ///< Font size
    argb_t font_color;       ///< Font color
    bool slideshow;          ///< Slide show mode
    size_t slideshow_sec;    ///< Slide show mode timing
    enum config_order order; ///< File list order
    bool loop;               ///< File list loop mode
    bool recursive;          ///< Read directories recursively
    bool all_files;          ///< Open all files from the same directory
};
extern struct config config;

/**
 * Initialize configuration: set defaults and load from file.
 */
void config_init(void);

/**
 * Free configuration instance.
 */
void config_free(void);

// Configuration setters
bool config_set_scale(const char* val);
bool config_set_background(const char* val);
bool config_set_wndbkg(const char* val);
bool config_set_wndpos(const char* val);
bool config_set_wndsize(const char* val);
bool config_set_font_name(const char* val);
bool config_set_font_size(const char* val);
bool config_set_order(const char* val);
bool config_set_slideshow_sec(const char* val);
bool config_set_appid(const char* val);

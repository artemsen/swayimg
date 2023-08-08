// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

#include <xkbcommon/xkbcommon.h>

// Background modes
#define COLOR_TRANSPARENT 0xff000000
#define BACKGROUND_GRID   0xfe000000

// Max number of key bindings
#define MAX_KEYBINDINGS 128

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

/** Action associated with a key. */
enum config_action {
    cfgact_none,
    cfgact_first_file,
    cfgact_last_file,
    cfgact_prev_dir,
    cfgact_next_dir,
    cfgact_prev_file,
    cfgact_next_file,
    cfgact_prev_frame,
    cfgact_next_frame,
    cfgact_animation,
    cfgact_slideshow,
    cfgact_fullscreen,
    cfgact_step_left,
    cfgact_step_right,
    cfgact_step_up,
    cfgact_step_down,
    cfgact_zoom_in,
    cfgact_zoom_out,
    cfgact_zoom_optimal,
    cfgact_zoom_fit,
    cfgact_zoom_fill,
    cfgact_zoom_real,
    cfgact_zoom_reset,
    cfgact_rotate_left,
    cfgact_rotate_right,
    cfgact_flip_vertical,
    cfgact_flip_horizontal,
    cfgact_reload,
    cfgact_antialiasing,
    cfgact_info,
    cfgact_exec,
    cfgact_quit,
};

/** Key bindings. */
struct config_keybind {
    xkb_keysym_t key;
    enum config_action action;
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
    const char* exec_cmd;    ///< Command to execute
    struct config_keybind keybind[MAX_KEYBINDINGS]; ///< Key bindings table
};

/**
 * Initialize configuration: set defaults and load from file.
 * @return created configuration context
 */
struct config* config_init(void);

/**
 * Free configuration instance.
 * @param ctx configuration context
 */
void config_free(struct config* ctx);

// Configuration setters
bool config_set_scale(struct config* ctx, const char* val);
bool config_set_background(struct config* ctx, const char* val);
bool config_set_window(struct config* ctx, const char* val);
bool config_set_geometry(struct config* ctx, const char* val);
bool config_set_font_name(struct config* ctx, const char* val);
bool config_set_font_size(struct config* ctx, const char* val);
bool config_set_order(struct config* ctx, const char* val);
bool config_set_slideshow_sec(struct config* ctx, const char* val);
bool config_set_appid(struct config* ctx, const char* val);
bool config_set_exec_cmd(struct config* ctx, const char* val);

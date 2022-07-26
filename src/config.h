// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

// Background and frame modes
#define COLOR_TRANSPARENT 0xff000000
#define BACKGROUND_GRID   0xfe000000

/** Order of file list. */
enum config_order {
    cfgord_none,  ///< Unsorted
    cfgord_alpha, ///< Alphanumeric sort
    cfgord_random ///< Random order
};

/** Initial scaling of images. */
enum config_scale {
    cfgsc_optimal, ///< Fit to window, but not more than 100%
    cfgsc_fit,     ///< Fit to window size
    cfgsc_real     ///< Real image size (100%)
};

/** App configuration. */
struct config {
    const char* app_id;      ///< Window class/app_id name
    bool sway_wm;            ///< Enable/disable integration with Sway WM
    struct rect window;      ///< Window geometry
    bool fullscreen;         ///< Full screen mode
    argb_t background;       ///< Background mode/color
    argb_t frame;            ///< Frame mode/color
    enum config_scale scale; ///< Initial scale
    bool show_info;          ///< Show image info
    const char* font_face;   ///< Font name
    size_t font_size;        ///< Font size
    argb_t font_color;       ///< Font color
    enum config_order order; ///< File list order
    bool recursive;          ///< Read directories recursively
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

/**
 * Check if configuration is incompatible.
 * @param ctx configuration context
 * @return configuration status, true if configuration is ok
 */
bool config_check(const struct config* ctx);

/**
 * Set initial scale from one of predefined string (default, fit or real).
 * @param ctx configuration context
 * @param scale text scale description
 * @return false if value format is invalid
 */
bool config_set_scale(struct config* ctx, const char* scale);

/**
 * Set background type/color from RGB hex string.
 * @param ctx configuration context
 * @param background text background description
 * @return false if value format is invalid
 */
bool config_set_background(struct config* ctx, const char* background);

/**
 * Set frame type/color from RGB hex string.
 * @param ctx configuration context
 * @param frame text background description
 * @return false if value format is invalid
 */
bool config_set_frame(struct config* ctx, const char* frame);

/**
 * Set window geometry (position and size) from string "x,y,width,height".
 * @param ctx configuration context
 * @param geometry text geometry description
 * @return false if value format is invalid
 */
bool config_set_geometry(struct config* ctx, const char* geometry);

/**
 * Set font name.
 * @param ctx configuration context
 * @param font font name
 * @return false if value format is invalid
 */
bool config_set_font_name(struct config* ctx, const char* font);

/**
 * Set font size.
 * @param ctx configuration context
 * @param size font size
 * @return false if value format is invalid
 */
bool config_set_font_size(struct config* ctx, const char* size);

/**
 * Set order of the file list.
 * @param ctx configuration context
 * @param order text order description
 * @return false if value format is invalid
 */
bool config_set_order(struct config* ctx, const char* order);

/**
 * Set window class/app_id.
 * @param ctx configuration context
 * @param app_id window class/app_id to set
 * @return false if value format is invalid
 */
bool config_set_appid(struct config* ctx, const char* app_id);

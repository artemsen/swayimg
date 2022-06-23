// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "canvas.h"

/** App configuration. */
typedef struct {
    scale_t scale;         ///< Initial scale
    rect_t window;         ///< Window geometry
    uint32_t background;   ///< Background mode/color
    bool fullscreen;       ///< Full screen mode
    bool show_info;        ///< Show image info
    const char* font_face; ///< Font name and size (pango format)
    uint32_t font_color;   ///< Font color
    const char* app_id;    ///< Window class/app_id name
    bool sway_rules;       ///< Enable/disable Sway rules
} config_t;

/**
 * Initialize configuration: set defaults and load from file.
 * @return created configuration instance
 */
config_t* init_config(void);

/**
 * Free configuration instance.
 * @param[in] cfg configuration instance
 */
void free_config(config_t* cfg);

/**
 * Check if configuration is incompatible.
 * @param[in] cfg configuration instance to check
 * @return configuration status, true if configuration is ok
 */
bool check_config(const config_t* cfg);

/**
 * Set initial scale from one of predefined string (default, fit or real).
 * @param[out] cfg target configuration instance
 * @param[in] scale text scale description
 * @return false if value format is invalid
 */
bool set_scale_config(config_t* cfg, const char* scale);

/**
 * Set background type/color from RGB hex string.
 * @param[out] cfg target configuration instance
 * @param[in] background text background description
 * @return false if value format is invalid
 */
bool set_background_config(config_t* cfg, const char* background);

/**
 * Set window geometry (position and size) from string "x,y,width,height".
 * @param[out] cfg target configuration instance
 * @param[in] geometry text geometry description
 * @return false if value format is invalid
 */
bool set_geometry_config(config_t* cfg, const char* geometry);

/**
 * Set window class/app_id.
 * @param[out] cfg target configuration instance
 * @param[in] app_id window class/app_id to set
 * @return false if value format is invalid
 */
bool set_appid_config(config_t* cfg, const char* app_id);

/**
 * Set font name and size.
 * @param[out] cfg target configuration instance
 * @param[in] font font name and size in pango format
 * @return false if value format is invalid
 */
bool set_font_config(config_t* cfg, const char* font);

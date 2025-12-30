// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.h"

/* Config parameter: list of key/value. */
struct config_keyval {
    struct config_keyval* next; ///< Links to next entry
    char* value;                ///< Value
    char key[1];                ///< Key (variable lenght)
};

/** Config instance: list of sections with key/values */
struct config {
    struct config* next;          ///< Links to next section
    struct config_keyval* params; ///< List of key/value for this section
    char name[1];                 ///< Section name (variable lenght)
};

// Name of default config file to load
#define CFG_DEF_FILE "config"

// Section names
#define CFG_GENERAL        "general"
#define CFG_DRM            "drm"
#define CFG_VIEWER         "viewer"
#define CFG_SLIDESHOW      "slideshow"
#define CFG_GALLERY        "gallery"
#define CFG_LIST           "list"
#define CFG_FONT           "font"
#define CFG_INFO           "info"
#define CFG_INFO_VIEWER    CFG_INFO "." CFG_VIEWER
#define CFG_INFO_SLIDESHOW CFG_INFO "." CFG_SLIDESHOW
#define CFG_INFO_GALLERY   CFG_INFO "." CFG_GALLERY
#define CFG_KEYS           "keys"
#define CFG_KEYS_VIEWER    CFG_KEYS "." CFG_VIEWER
#define CFG_KEYS_SLIDESHOW CFG_KEYS "." CFG_SLIDESHOW
#define CFG_KEYS_GALLERY   CFG_KEYS "." CFG_GALLERY

// Configuration parameters
#define CFG_GNRL_MODE     "mode"
#define CFG_GNRL_SIZE     "size"
#define CFG_GNRL_POSITION "position"
#define CFG_GNRL_OVERLAY  "overlay"
#define CFG_GNRL_DECOR    "decoration"
#define CFG_GNRL_SIGUSR1  "sigusr1"
#define CFG_GNRL_SIGUSR2  "sigusr2"
#define CFG_GNRL_IPC      "ipc"
#define CFG_GNRL_APP_ID   "app_id"

#define CFG_DRM_PATH      "path"
#define CFG_DRM_CONNECTOR "connector"
#define CFG_DRM_MODE      "mode"

#define CFG_VIEW_WINDOW   "window"
#define CFG_VIEW_TRANSP   "transparency"
#define CFG_VIEW_SCALE    "scale"
#define CFG_VIEW_POSITION "position"
#define CFG_VIEW_AA       "antialiasing"
#define CFG_VIEW_LOOP     "loop"
#define CFG_VIEW_HISTORY  "history"
#define CFG_VIEW_PRELOAD  "preload"
#define CFG_VIEW_SSHOW_TM "time"

#define CFG_GLRY_SIZE           "size"
#define CFG_GLRY_PADDING        "padding"
#define CFG_GLRY_CACHE          "cache"
#define CFG_GLRY_PRELOAD        "preload"
#define CFG_GLRY_PSTORE         "pstore"
#define CFG_GLRY_ASPECT         "aspect"
#define CFG_GLRY_AA             "antialiasing"
#define CFG_GLRY_WINDOW         "window"
#define CFG_GLRY_BKG            "background"
#define CFG_GLRY_SELECT         "select"
#define CFG_GLRY_BORDER         "border"
#define CFG_GLRY_BORDER_WIDTH   "border_width"
#define CFG_GLRY_SHADOW         "shadow"
#define CFG_GLRY_SELECTED_SCALE "selected_scale"

#define CFG_LIST_FROMFILE  "from_file"
#define CFG_LIST_ORDER     "order"
#define CFG_LIST_REVERSE   "reverse"
#define CFG_LIST_RECURSIVE "recursive"
#define CFG_LIST_ALL       "all"
#define CFG_LIST_FSMON     "fsmon"

#define CFG_FONT_NAME   "name"
#define CFG_FONT_SIZE   "size"
#define CFG_FONT_COLOR  "color"
#define CFG_FONT_BKG    "background"
#define CFG_FONT_SHADOW "shadow"

#define CFG_INFO_SHOW     "show"
#define CFG_INFO_PADDING  "padding"
#define CFG_INFO_ITIMEOUT "info_timeout"
#define CFG_INFO_STIMEOUT "status_timeout"
#define CFG_INFO_TL       "top_left"
#define CFG_INFO_TR       "top_right"
#define CFG_INFO_BL       "bottom_left"
#define CFG_INFO_BR       "bottom_right"

// Some configuration values
#define CFG_YES        "yes"
#define CFG_NO         "no"
#define CFG_AUTO       "auto"
#define CFG_FROM_IMAGE "image"
#define CFG_FULLSCREEN "fullscreen"

/**
 * Create default configuration instance.
 * @return loaded config instance
 */
struct config* config_create(void);

/**
 * Free configuration instance.
 * @param cfg config instance
 */
void config_free(struct config* cfg);

/**
 * Load configuration from file.
 * @param cfg config instance
 * @param name config file name or absolute/relative path to the file
 * @return true if file was loaded
 */
bool config_load(struct config* cfg, const char* name);

/**
 * Get section.
 * @param cfg config instance
 * @param name section name
 * @return config section or NULL if not found
 */
const struct config* config_section(const struct config* cfg, const char* name);

/**
 * Set config option.
 * @param cfg config instance
 * @param section section name
 * @param key,value configuration parameters
 * @return false if section or key is unknown
 */
bool config_set(struct config* cfg, const char* section, const char* key,
                const char* value);

/**
 * Set config option from command line argument.
 * @param cfg config instance
 * @param arg command in format: "section.key=value"
 * @return false if section or key is unknown or argument is invalid
 */
bool config_set_arg(struct config* cfg, const char* arg);

/**
 * Get default config option.
 * @param section section name
 * @param key,value configuration parameters
 * @return default value for specified options
 */
const char* config_get_default(const char* section, const char* key);

/**
 * Get config option.
 * @param section config section
 * @param key,value configuration parameters
 * @return value from the config or default value
 */
const char* config_get(const struct config* section, const char* key);

/**
 * Get config parameter as string value restricted by specified array.
 * @param section config section
 * @param key property key
 * @param array array of possible values
 * @param array_sz number of strings in possible values array
 * @return index of the value or default value
 */
ssize_t config_get_oneof(const struct config* section, const char* key,
                         const char** array, size_t array_sz);

/**
 * Get config parameter as boolean value.
 * @param section config section
 * @param key property key
 * @return value or default value if user defined value is invalid
 */
bool config_get_bool(const struct config* section, const char* key);

/**
 * Get config parameter as integer value.
 * @param section config section
 * @param key property key
 * @return value or default value if user defined value is invalid
 */
ssize_t config_get_num(const struct config* section, const char* key,
                       ssize_t min_val, ssize_t max_val);

/**
 * Get config parameter as float value.
 * @param section config section
 * @param key property key
 * @return value or default value if user defined value is invalid
 */
float config_get_float(const struct config* section, const char* key,
                       float min_val, float max_val);

/**
 * Get config parameter as ARGB color value.
 * @param section config section
 * @param key property key
 * @return value or default value if user defined value is invalid
 */
argb_t config_get_color(const struct config* section, const char* key);

/**
 * Print error about invalid key format.
 * @param section section name
 * @param key configuration parameters
 */
void config_error_key(const char* section, const char* key);

/**
 * Print error about invalid value format.
 * @param section section name
 * @param value configuration parameters
 */
void config_error_val(const char* section, const char* value);

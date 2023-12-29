// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

// Background modes
#define COLOR_TRANSPARENT 0xff000000
#define BACKGROUND_GRID   0xfe000000

// Copy position or size from parent window
#define SAME_AS_PARENT 0xffffffff
// Copy size from image
#define SAME_AS_IMAGE 0

/** Initial scaling of images. */
enum config_scale {
    cfgsc_optimal,    ///< Fit to window, but not more than 100%
    cfgsc_fit,        ///< Fit to window size
    cfgsc_fit_width,  ///< Fit to window size
    cfgsc_fit_height, ///< Fit to window size
    cfgsc_fill,       ///< Fill the window
    cfgsc_real        ///< Real image size (100%)
};

/** Load status. */
enum config_status {
    cfgst_ok,
    cfgst_invalid_section,
    cfgst_invalid_key,
    cfgst_invalid_value,
};

/**
 * Custom section loader.
 * @param key,value configuration parameters
 * @return load status
 */
typedef enum config_status (*config_loader)(const char* key, const char* value);

/** App configuration. */
struct config {
    char* app_id;            ///< Window class/app_id name
    struct rect geometry;    ///< Window geometry
    bool fullscreen;         ///< Full screen mode
    bool antialiasing;       ///< Anti-aliasing
    argb_t background;       ///< Image background mode/color
    argb_t window;           ///< Window background mode/color
    enum config_scale scale; ///< Initial scale
    bool slideshow;          ///< Slide show mode
    size_t slideshow_sec;    ///< Slide show mode timing
};
extern struct config config;

/** Token description. */
struct config_token {
    const char* value;
    size_t len;
};

/**
 * Initialize configuration: set defaults and load from file.
 */
void config_init(void);

/**
 * Free configuration instance.
 */
void config_free(void);

/**
 * Set option.
 * @param section section name
 * @param key,value configuration parameters
 * @return load status
 */
enum config_status config_set(const char* section, const char* key,
                              const char* value);
/**
 * Execute config command to set option.
 * @param cmd command in format: "section.key=value"
 * @return false if error
 */
bool config_command(const char* cmd);

/**
 * Register custom section loader.
 * @param name statically allocated name of the section
 * @param loader section data handler
 */
void config_add_section(const char* name, config_loader loader);

/**
 * Convert text value to boolean.
 * @param text text to convert
 * @param flag target variable
 * @return false if text has invalid format
 */
bool config_parse_bool(const char* text, bool* flag);

/**
 * Parse text to number.
 * @param text text to convert
 * @param color output variable
 * @param base numeric base
 * @return false if text has invalid format
 */
bool config_parse_num(const char* text, ssize_t* value, int base);

/**
 * Parse text value to ARGB color.
 * @param text text to convert
 * @param color output variable
 * @return false if text has invalid format
 */
bool config_parse_color(const char* text, argb_t* color);

/**
 * Parse text value to tokens ("abc,def" -> "abc", "def").
 * @param text text to convert
 * @param delimeter delimeter character
 * @param torkens output array of parsed tokens
 * @param max_tokens max number of tokens (size of array)
 * @return real number of tokens in input string
 */
size_t config_parse_tokens(const char* text, char delimeter,
                           struct config_token* tokens, size_t max_tokens);

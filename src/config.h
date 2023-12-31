// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "types.h"

// Name of the general configuration section
#define GENERAL_CONFIG "general"

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
 * @param value output variable
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
 * Parse pair of numbers.
 * @param text text to convert
 * @param n1,n2 output values
 * @return false if values have invalid format
 */
bool config_parse_numpair(const char* text, long* n1, long* n2);

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

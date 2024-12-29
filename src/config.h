// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "memdata.h"
#include "pixmap.h"

/* Key/value config option. */
struct config_keyval {
    struct list list; ///< Links to prev/next entry
    char* key;        ///< Key
    char* value;      ///< Value
    bool used;        ///< Sanity checker
};

/** Config instance: list of sections with key/values */
struct config {
    struct list list;             ///< Links to prev/next entry
    char* name;                   ///< Section name
    struct config_keyval* params; ///< List of key/value for this section
};

/**
 * Load configuration from file.
 * @return loaded config instance or NULL if no config file found
 */
struct config* config_load(void);

/**
 * Free configuration instance.
 * @param cfg config instance
 */
void config_free(struct config* cfg);

/**
 * Check if all configuration parameters were read.
 * @param cfg config instance
 */
void config_check(struct config* cfg);

/**
 * Set config option.
 * @param cfg config instance
 * @param section section name
 * @param key,value configuration parameters
 */
void config_set(struct config** cfg, const char* section, const char* key,
                const char* value);
/**
 * Set config option from command line argument.
 * @param cfg config instance
 * @param arg command in format: "section.key=value"
 * @return false on invalid format
 */
bool config_set_arg(struct config** cfg, const char* arg);

/**
 * Get config option.
 * @param cfg config instance
 * @param section section name
 * @param key,value configuration parameters
 * @return value or NULL if section or key not found
 */
const char* config_get(struct config* cfg, const char* section,
                       const char* key);
/**
 * Get config parameter as string value.
 * @param cfg config instance
 * @param section,key section name and key
 * @param fallback default value used if parameter not found
 * @return value or fallback value if section or key not found
 */
const char* config_get_string(struct config* cfg, const char* section,
                              const char* key, const char* fallback);
/**
 * Get config parameter as boolean value.
 * @param cfg config instance
 * @param section,key section name and key
 * @param fallback default value used if parameter not found or invalid
 * @return value or fallback value if section or key not found or invalid
 */
bool config_get_bool(struct config* cfg, const char* section, const char* key,
                     bool fallback);
/**
 * Get config parameter as integer value.
 * @param cfg config instance
 * @param section,key section name and key
 * @param fallback default value used if parameter not found or invalid
 * @return value or fallback value if section or key not found or invalid
 */
ssize_t config_get_num(struct config* cfg, const char* section, const char* key,
                       ssize_t min_val, ssize_t max_val, ssize_t fallback);
/**
 * Get config parameter as ARGB color value.
 * @param cfg config instance
 * @param section,key section name and key
 * @param fallback default value used if parameter not found or invalid
 * @return value or fallback value if section or key not found or invalid
 */
argb_t config_get_color(struct config* cfg, const char* section,
                        const char* key, argb_t fallback);
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

/**
 * Expand path from environment variable.
 * @param prefix_env path prefix (var name)
 * @param postfix constant postfix
 * @return allocated buffer with path, caller should free it after use
 */
char* expand_path(const char* prefix_env, const char* postfix);

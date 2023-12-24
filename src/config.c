// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include "keybind.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

/** Base name of window class/app_id used by default */
#define APP_ID_BASE "swayimg"
/** Max length of window class/app_id name */
#define APP_ID_MAX 32

// Section names
#define SECTION_GENERAL "general"

/** Config file location. */
struct location {
    const char* prefix;  ///< Environment variable name
    const char* postfix; ///< Constant postfix
};

/** Section loader. */
struct config_section {
    const char* name;
    config_loader loader;
};
static struct config_section sections[3];

static const struct location config_locations[] = {
    { "XDG_CONFIG_HOME", "/swayimg/config" },
    { "HOME", "/.config/swayimg/config" },
    { "XDG_CONFIG_DIRS", "/swayimg/config" },
    { NULL, "/etc/xdg/swayimg/config" }
};

/** Singleton config instance. */
struct config config;

/**
 * Expand path from environment variable.
 * @param prefix_env path prefix (var name)
 * @param postfix constant postfix
 * @return allocated buffer with path, caller should free it after use
 */
static char* expand_path(const char* prefix_env, const char* postfix)
{
    char* path;
    const char* prefix;
    size_t prefix_len = 0;
    size_t postfix_len = strlen(postfix);

    if (prefix_env) {
        const char* delim;
        prefix = getenv(prefix_env);
        if (!prefix || !*prefix) {
            return NULL;
        }
        // use only the first directory if prefix is a list
        delim = strchr(prefix, ':');
        prefix_len = delim ? (size_t)(delim - prefix) : strlen(prefix);
    }

    // compose path
    path = malloc(prefix_len + postfix_len + 1 /* last null*/);
    if (path) {
        if (prefix_len) {
            memcpy(path, prefix, prefix_len);
        }
        memcpy(path + prefix_len, postfix, postfix_len + 1 /*last null*/);
    }

    return path;
}

/**
 * Convert text value to boolean.
 * @param text text to convert
 * @param value target variable
 * @return false if value has invalid format
 */
static bool set_boolean(const char* text, bool* value)
{
    bool rc = false;

    if (strcmp(text, "yes") == 0 || strcmp(text, "true") == 0) {
        *value = true;
        rc = true;
    } else if (strcmp(text, "no") == 0 || strcmp(text, "false") == 0) {
        *value = false;
        rc = true;
    }

    return rc;
}

/**
 * Convert text value to RGB color.
 * @param text text to convert
 * @param value target variable
 * @return false if value has invalid format
 */
static bool set_color(const char* text, uint32_t* value)
{
    char* endptr;
    unsigned long rgb;

    if (*text == '#') {
        ++text;
    }

    rgb = strtoul(text, &endptr, 16);
    if (*endptr || rgb > 0x00ffffff || errno == ERANGE ||
        (rgb == 0 && errno == EINVAL)) {
        return false;
    }
    *value = rgb;

    return true;
}

/**
 * Parse pair of numbers.
 * @param text text to parse
 * @param n1,n2 output values
 * @return false if values have invalid format
 */
static bool parse_numpair(const char* text, long* n1, long* n2)
{
    char* endptr;

    errno = 0;

    // first number
    *n1 = strtol(text, &endptr, 0);
    if (errno) {
        return false;
    }
    // skip delimeter
    while (*endptr && *endptr == ',') {
        ++endptr;
    }
    // second number
    *n2 = strtol(endptr, &endptr, 0);
    if (errno) {
        return false;
    }

    return *endptr == 0;
}

/**
 * Set (replace) string in config parameter.
 * @param src source string
 * @param dst destination buffer
 * @return false if not enough memory
 */
static bool set_string(const char* src, char** dst)
{
    const size_t len = strlen(src) + 1 /*last null*/;
    char* ptr = malloc(len);
    if (!ptr) {
        fprintf(stderr, "Not enough memory\n");
        return false;
    }
    memcpy(ptr, src, len);

    free(*dst);
    *dst = ptr;

    return true;
}

/**
 * Apply global property to configuration.
 * @param key property key
 * @param value property value
 * @return operation complete status, false if key was not handled
 */
static bool apply_conf(const char* key, const char* value)
{
    if (strcmp(key, "scale") == 0) {
        return config_set_scale(value);
    } else if (strcmp(key, "fullscreen") == 0) {
        return set_boolean(value, &config.fullscreen);
    } else if (strcmp(key, "background") == 0) {
        return config_set_background(value);
    } else if (strcmp(key, "wndbkg") == 0) {
        return config_set_wndbkg(value);
    } else if (strcmp(key, "wndpos") == 0) {
        return config_set_wndpos(value);
    } else if (strcmp(key, "wndsize") == 0) {
        return config_set_wndsize(value);
    } else if (strcmp(key, "info") == 0) {
        return set_boolean(value, &config.show_info);
    } else if (strcmp(key, "font") == 0) {
        return config_set_font_name(value);
    } else if (strcmp(key, "font-size") == 0) {
        return config_set_font_size(value);
    } else if (strcmp(key, "font-color") == 0) {
        return set_color(value, &config.font_color);
    } else if (strcmp(key, "order") == 0) {
        return config_set_order(value);
    } else if (strcmp(key, "loop") == 0) {
        return set_boolean(value, &config.loop);
    } else if (strcmp(key, "recursive") == 0) {
        return set_boolean(value, &config.recursive);
    } else if (strcmp(key, "all") == 0) {
        return set_boolean(value, &config.all_files);
    } else if (strcmp(key, "slideshow") == 0) {
        return config_set_slideshow_sec(value);
    } else if (strcmp(key, "antialiasing") == 0) {
        return set_boolean(value, &config.antialiasing);
    } else if (strcmp(key, "app_id") == 0) {
        return config_set_appid(value);
    } else if (strcmp(key, "sway") == 0) {
        return set_boolean(value, &config.sway_wm);
    }
    fprintf(stderr, "Invalid config key name: %s\n", key);
    return false;
}

/**
 * Load configuration from a file.
 * @param path full path to the file
 * @return operation complete status, false on error
 */
static bool load_config(const char* path)
{
    FILE* fd = NULL;
    char* buff = NULL;
    size_t buff_sz = 0;
    size_t line_num = 0;
    ssize_t nread;
    char* section = NULL;

    fd = fopen(path, "r");
    if (!fd) {
        return false;
    }

    while ((nread = getline(&buff, &buff_sz, fd)) != -1) {
        char* delim;
        char* value;
        char* line = buff;
        bool status = false;

        ++line_num;

        // trim spaces
        while (nread-- && isspace(line[nread])) {
            line[nread] = 0;
        }
        while (*line && isspace(*line)) {
            ++line;
        }

        // skip empty lines and comments
        if (!*line || *line == '#') {
            continue;
        }

        // check for section beginning
        if (*line == '[') {
            ssize_t len;
            ++line;
            delim = strchr(line, ']');
            if (!delim || line + 1 == delim) {
                fprintf(stderr, "Invalid section define in %s:%lu\n", path,
                        line_num);
                continue;
            }
            *delim = 0;
            len = delim - line + 1;
            section = realloc(section, len);
            memcpy(section, line, len);
            continue;
        }

        delim = strchr(line, '=');
        if (!delim) {
            fprintf(stderr, "Invalid key=value format in %s:%lu\n", path,
                    line_num);
            continue;
        }

        // trim spaces from start of value
        value = delim + 1;
        while (*value && isspace(*value)) {
            ++value;
        }
        // trim spaces from key
        *delim = 0;
        while (line != delim && isspace(*--delim)) {
            *delim = 0;
        }

        // apply configuration parameter from key/value pair
        if (!section || strcmp(section, SECTION_GENERAL) == 0) {
            status = apply_conf(line, value);
        } else {
            // get loader
            struct config_section* sec = NULL;
            for (size_t i = 0;
                 i < sizeof(sections) / sizeof(sections[0]) && sections[i].name;
                 ++i) {
                if (strcmp(sections[i].name, section) == 0) {
                    sec = &sections[i];
                    break;
                }
            }
            if (sec) {
                status = sec->loader(line, value);
            } else {
                fprintf(stderr, "Invalid section name: '%s'\n", section);
            }
        }
        if (!status) {
            fprintf(stderr, "Invalid configuration in %s:%lu\n", path,
                    line_num);
        }
    }

    free(buff);
    free(section);
    fclose(fd);

    return true;
}

void config_init(void)
{
    struct timespec ts;

    // default settings
    config.scale = cfgsc_optimal;
    config.background = BACKGROUND_GRID;
    config.window = COLOR_TRANSPARENT;
    config.sway_wm = true;
    config.geometry.x = SAME_AS_PARENT;
    config.geometry.y = SAME_AS_PARENT;
    config.geometry.width = SAME_AS_PARENT;
    config.geometry.height = SAME_AS_PARENT;
    config_set_font_name("monospace");
    config.font_color = 0xcccccc;
    config.font_size = 14;
    config.slideshow = false;
    config.slideshow_sec = 3;
    config.order = cfgord_alpha;
    config.loop = true;
    config.recursive = false;
    config.all_files = false;
    config.antialiasing = false;

    // create unique application id
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        char app_id[64];
        snprintf(app_id, sizeof(app_id), APP_ID_BASE "_%lx",
                 (ts.tv_sec << 32) | ts.tv_nsec);
        config_set_appid(app_id);
    } else {
        config_set_appid(APP_ID_BASE);
    }

    // find and load first available config file
    for (size_t i = 0;
         i < sizeof(config_locations) / sizeof(config_locations[0]); ++i) {
        const struct location* cl = &config_locations[i];
        char* path = expand_path(cl->prefix, cl->postfix);
        if (path && load_config(path)) {
            free(path);
            break;
        }
        free(path);
    }
}

void config_free(void)
{
    free((void*)config.font_face);
    free((void*)config.app_id);
}

void config_add_section(const char* name, config_loader loader)
{
    size_t index = 0;
    struct config_section* section = sections;
    while (section->name) {
        if (++index >= sizeof(sections) / sizeof(sections[0])) {
            return;
        }
        ++section;
    }
    section->name = name;
    section->loader = loader;
}

bool config_set_scale(const char* val)
{
    if (strcmp(val, "optimal") == 0) {
        config.scale = cfgsc_optimal;
    } else if (strcmp(val, "fit") == 0) {
        config.scale = cfgsc_fit;
    } else if (strcmp(val, "fill") == 0) {
        config.scale = cfgsc_fill;
    } else if (strcmp(val, "real") == 0) {
        config.scale = cfgsc_real;
    } else {
        fprintf(stderr, "Invalid scale: %s\n", val);
        fprintf(stderr, "Expected 'optimal', 'fit', 'fill', or 'real'.\n");
        return false;
    }
    return true;
}

bool config_set_background(const char* val)
{
    if (strcmp(val, "grid") == 0) {
        config.background = BACKGROUND_GRID;
    } else if (strcmp(val, "none") == 0) {
        config.background = COLOR_TRANSPARENT;
    } else if (!set_color(val, &config.background)) {
        fprintf(stderr, "Invalid image background: %s\n", val);
        fprintf(stderr, "Expected 'none', 'grid', or RGB hex value.\n");
        return false;
    }
    return true;
}

bool config_set_wndbkg(const char* val)
{
    if (strcmp(val, "none") == 0) {
        config.window = COLOR_TRANSPARENT;
    } else if (!set_color(val, &config.window)) {
        fprintf(stderr, "Invalid window background: %s\n", val);
        fprintf(stderr, "Expected 'none' or RGB hex value.\n");
        return false;
    }
    return true;
}

bool config_set_wndpos(const char* val)
{
    if (strcmp(val, "parent") == 0) {
        config.geometry.x = SAME_AS_PARENT;
        config.geometry.y = SAME_AS_PARENT;
        return true;
    } else {
        long x, y;
        if (parse_numpair(val, &x, &y)) {
            config.geometry.x = (ssize_t)x;
            config.geometry.y = (ssize_t)y;
            return true;
        }
    }
    fprintf(stderr, "Invalid window position: %s\n", val);
    fprintf(stderr, "Expected 'parent' or X,Y.\n");
    return false;
}

bool config_set_wndsize(const char* val)
{
    if (strcmp(val, "parent") == 0) {
        config.geometry.width = SAME_AS_PARENT;
        config.geometry.height = SAME_AS_PARENT;
        return true;
    } else if (strcmp(val, "image") == 0) {
        config.geometry.width = SAME_AS_IMAGE;
        config.geometry.height = SAME_AS_IMAGE;
        return true;
    } else {
        long width, height;
        if (parse_numpair(val, &width, &height) && width > 0 && height > 0) {
            config.geometry.width = (size_t)width;
            config.geometry.height = (size_t)height;
            return true;
        }
    }

    fprintf(stderr, "Invalid window size: %s\n", val);
    fprintf(stderr, "Expected 'parent', 'image', or WIDTH,HEIGHT.\n");
    return false;
}

bool config_set_font_name(const char* val)
{
    const size_t len = strlen(val);
    if (len == 0) {
        fprintf(stderr, "Invalid font name\n");
        return false;
    }
    return set_string(val, (char**)&config.font_face);
}

bool config_set_font_size(const char* val)
{
    char* endptr;
    const unsigned long sz = strtoul(val, &endptr, 10);
    if (*endptr || sz == 0) {
        fprintf(stderr, "Invalid font size\n");
        return false;
    }
    config.font_size = sz;
    return true;
}

bool config_set_order(const char* val)
{
    if (strcmp(val, "none") == 0) {
        config.order = cfgord_none;
    } else if (strcmp(val, "alpha") == 0) {
        config.order = cfgord_alpha;
    } else if (strcmp(val, "random") == 0) {
        config.order = cfgord_random;
    } else {
        fprintf(stderr, "Invalid file list order: %s\n", val);
        fprintf(stderr, "Expected 'none', 'alpha', or 'random'.\n");
        return false;
    }
    return true;
}

bool config_set_slideshow_sec(const char* val)
{
    char* endptr;
    const unsigned long sec = strtoul(val, &endptr, 10);
    if (*endptr || sec == 0) {
        fprintf(stderr, "Invalid slideshow duration\n");
        return false;
    }
    config.slideshow_sec = sec;
    return true;
}

bool config_set_appid(const char* val)
{
    const size_t len = strlen(val);
    if (len == 0 || len > APP_ID_MAX) {
        fprintf(stderr, "Invalid class/app_id: %s\n", val);
        fprintf(stderr, "Expected non-empty string up to %d chars.\n",
                APP_ID_MAX);
        return false;
    }
    return set_string(val, (char**)&config.app_id);
}

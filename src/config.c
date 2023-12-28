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

// Section names
#define SECTION_GENERAL "general"

/** Config file location. */
struct location {
    const char* prefix;  ///< Environment variable name
    const char* postfix; ///< Constant postfix
};

/** Section loader. */
struct section {
    const char* name;
    config_loader loader;
};

/** Config context. */
struct config_context {
    struct section* sections;
    size_t num_sections;
};
static struct config_context ctx;

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
 * Apply global property to configuration.
 * @param key property key
 * @param value property value
 * @return operation complete status
 */
static enum config_status load_general(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, "scale") == 0) {
        status = cfgst_ok;
        if (strcmp(value, "optimal") == 0) {
            config.scale = cfgsc_optimal;
        } else if (strcmp(value, "fit") == 0) {
            config.scale = cfgsc_fit;
        } else if (strcmp(value, "fill") == 0) {
            config.scale = cfgsc_fill;
        } else if (strcmp(value, "real") == 0) {
            config.scale = cfgsc_real;
        } else {
            status = cfgst_invalid_value;
        }
    } else if (strcmp(key, "fullscreen") == 0) {
        if (config_parse_bool(value, &config.fullscreen)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "background") == 0) {
        status = cfgst_ok;
        if (strcmp(value, "grid") == 0) {
            config.background = BACKGROUND_GRID;
        } else if (strcmp(value, "none") == 0) {
            config.background = COLOR_TRANSPARENT;
        } else if (!config_parse_color(value, &config.background)) {
            status = cfgst_invalid_value;
        }
    } else if (strcmp(key, "wndbkg") == 0) {
        if (strcmp(value, "none") == 0) {
            config.window = COLOR_TRANSPARENT;
            status = cfgst_ok;
        } else if (config_parse_color(value, &config.window)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "wndpos") == 0) {
        if (strcmp(value, "parent") == 0) {
            config.geometry.x = SAME_AS_PARENT;
            config.geometry.y = SAME_AS_PARENT;
            status = cfgst_ok;
        } else {
            long x, y;
            if (parse_numpair(value, &x, &y)) {
                config.geometry.x = (ssize_t)x;
                config.geometry.y = (ssize_t)y;
                status = cfgst_ok;
            }
        }
    } else if (strcmp(key, "wndsize") == 0) {
        if (strcmp(value, "parent") == 0) {
            config.geometry.width = SAME_AS_PARENT;
            config.geometry.height = SAME_AS_PARENT;
            status = cfgst_ok;
        } else if (strcmp(value, "image") == 0) {
            config.geometry.width = SAME_AS_IMAGE;
            config.geometry.height = SAME_AS_IMAGE;
            status = cfgst_ok;
        } else {
            long width, height;
            if (parse_numpair(value, &width, &height) && width > 0 &&
                height > 0) {
                config.geometry.width = (size_t)width;
                config.geometry.height = (size_t)height;
                status = cfgst_ok;
            }
        }
    } else if (strcmp(key, "slideshow") == 0) {
        ssize_t num;
        if (config_parse_num(value, &num, 0) && num != 0) {
            config.slideshow_sec = num;
            status = cfgst_ok;
        }
    } else if (strcmp(key, "antialiasing") == 0) {
        if (config_parse_bool(value, &config.antialiasing)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "app_id") == 0) {
        const size_t sz = strlen(value) + 1;
        char* ptr = realloc(config.app_id, sz);
        if (ptr) {
            memcpy(ptr, value, sz);
            config.app_id = ptr;
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
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
        const char* value;
        char* line = buff;
        enum config_status status;

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

        // load configuration parameter from key/value pair
        status = config_set(section ? section : SECTION_GENERAL, line, value);
        if (status != cfgst_ok) {
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
    config.geometry.x = SAME_AS_PARENT;
    config.geometry.y = SAME_AS_PARENT;
    config.geometry.width = SAME_AS_PARENT;
    config.geometry.height = SAME_AS_PARENT;
    config.slideshow = false;
    config.slideshow_sec = 3;
    config.antialiasing = false;

    // create unique application id
    const size_t idlen = 32;
    config.app_id = malloc(idlen);
    if (config.app_id) {
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
            snprintf(config.app_id, idlen, APP_ID_BASE "_%lx",
                     (ts.tv_sec << 32) | ts.tv_nsec);
        } else {
            strcpy(config.app_id, APP_ID_BASE);
        }
    }

    config_add_section(SECTION_GENERAL, load_general);

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
    free((void*)config.app_id);
    free(ctx.sections);
}

enum config_status config_set(const char* section, const char* key,
                              const char* value)
{
    enum config_status status = cfgst_invalid_section;

    for (size_t i = 0; i < ctx.num_sections; ++i) {
        if (strcmp(ctx.sections[i].name, section) == 0) {
            status = ctx.sections[i].loader(key, value);
            break;
        }
    }

    switch (status) {
        case cfgst_ok:
            break;
        case cfgst_invalid_section:
            fprintf(stderr, "Invalid section \"%s\"\n", section);
            break;
        case cfgst_invalid_key:
            fprintf(stderr, "Invalid key \"%s\"\n", key);
            break;
        case cfgst_invalid_value:
            fprintf(stderr, "Invalid value \"%s\"\n", value);
            break;
    }

    return status;
}

bool config_command(const char* cmd)
{
    char section[32];
    char key[32];
    const char* value;
    char* ptr;
    const char* it = cmd;

    // get section name
    ptr = section;
    while (*it != '.') {
        if (!*it || ptr >= section + sizeof(section) - 1) {
            goto format_error;
        }
        *ptr++ = *it++;
    }
    *ptr = 0; // last null
    ++it;     // skip delimeter

    // get key
    ptr = key;
    while (*it != '=') {
        if (!*it || ptr >= key + sizeof(key) - 1) {
            goto format_error;
        }
        *ptr++ = *it++;
    }
    *ptr = 0; // last null
    ++it;     // skip delimeter

    // get value
    value = it;

    // load setting
    return config_set(section, key, value) == cfgst_ok;

format_error:
    fprintf(stderr, "Invalid format: \"%s\"\n", cmd);
    return false;
}

void config_add_section(const char* name, config_loader loader)
{
    const size_t new_sz = (ctx.num_sections + 1) * sizeof(struct section);
    struct section* sections = realloc(ctx.sections, new_sz);
    if (sections) {
        ctx.sections = sections;
        ctx.sections[ctx.num_sections].name = name;
        ctx.sections[ctx.num_sections].loader = loader;
        ++ctx.num_sections;
    }
}

bool config_parse_bool(const char* text, bool* flag)
{
    bool rc = false;

    if (strcmp(text, "yes") == 0 || strcmp(text, "true") == 0) {
        *flag = true;
        rc = true;
    } else if (strcmp(text, "no") == 0 || strcmp(text, "false") == 0) {
        *flag = false;
        rc = true;
    }

    return rc;
}

bool config_parse_num(const char* text, ssize_t* value, int base)
{
    char* endptr;
    long long num;

    errno = 0;
    num = strtoll(text, &endptr, base);
    if (!*endptr && errno == 0) {
        *value = num;
        return true;
    }

    return false;
}

bool config_parse_color(const char* text, argb_t* color)
{
    ssize_t num;

    if (*text == '#') {
        ++text;
    }

    if (config_parse_num(text, &num, 16) && num >= 0 && num <= 0xffffffff) {
        *color = num;
        return true;
    }

    return false;
}

size_t config_parse_tokens(const char* text, char delimeter,
                           struct config_token* tokens, size_t max_tokens)
{
    size_t token_num = 0;

    while (*text) {
        struct config_token token;

        // skip spaces
        while (*text && isspace(*text)) {
            ++text;
        }
        if (!*text) {
            break;
        }

        // construct token
        if (*text == delimeter) {
            // empty token
            token.value = "";
            token.len = 0;
        } else {
            token.value = text;
            while (*text && *text != delimeter) {
                ++text;
            }
            token.len = text - token.value;
            // trim spaces
            while (token.len && isspace(token.value[token.len - 1])) {
                --token.len;
            }
        }

        // add to output array
        if (tokens && token_num < max_tokens) {
            memcpy(&tokens[token_num], &token, sizeof(token));
        }
        ++token_num;

        if (*text) {
            ++text; // skip delimeter
        }
    }

    return token_num;
}

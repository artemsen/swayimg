// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include "str.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                fprintf(stderr, "Invalid section define in %s:%zu\n", path,
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
            fprintf(stderr, "Invalid key=value format in %s:%zu\n", path,
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
        status = config_set(section, line, value);
        if (status != cfgst_ok) {
            fprintf(stderr, "Invalid configuration in %s:%zu\n", path,
                    line_num);
        }
    }

    free(buff);
    free(section);
    fclose(fd);

    return true;
}

void config_load(void)
{
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

void config_destroy(void)
{
    if (ctx.sections) {
        free(ctx.sections);
        ctx.sections = NULL;
    }
}

enum config_status config_set(const char* section, const char* key,
                              const char* value)
{
    enum config_status status = cfgst_invalid_section;

    if (!section || !*section) {
        fprintf(stderr, "Empty section name\n");
        return cfgst_invalid_section;
    }

    for (size_t i = 0; i < ctx.num_sections; ++i) {
        const struct section* sl = &ctx.sections[i];
        if (strcmp(sl->name, section) == 0) {
            status = sl->loader(key, value);
            if (status != cfgst_invalid_key) {
                break;
            }
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
    ++it;     // skip delimiter

    // get key
    ptr = key;
    while (*it != '=') {
        if (!*it || ptr >= key + sizeof(key) - 1) {
            goto format_error;
        }
        *ptr++ = *it++;
    }
    *ptr = 0; // last null
    ++it;     // skip delimiter

    // get value
    value = it;

    // load setting
    return config_set(section, key, value) == cfgst_ok;

format_error:
    fprintf(stderr, "Invalid format: \"%s\"\n", cmd);
    return false;
}

void config_add_loader(const char* section, config_loader loader)
{
    const size_t new_sz = (ctx.num_sections + 1) * sizeof(struct section);
    struct section* sections = realloc(ctx.sections, new_sz);
    if (sections) {
        ctx.sections = sections;
        ctx.sections[ctx.num_sections].name = section;
        ctx.sections[ctx.num_sections].loader = loader;
        ++ctx.num_sections;
    }
}

bool config_to_bool(const char* text, bool* flag)
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

bool config_to_color(const char* text, argb_t* color)
{
    ssize_t num;

    if (*text == '#' || isspace(*text)) {
        ++text;
    }

    if (!str_to_num(text, 0, &num, 16) || num < 0 ||
        (uint64_t)num > (uint64_t)0xffffffff) {
        return false;
    }

    if (strlen(text) > 6) { // value with alpha (RRGGBBAA)
        *color = (num >> 8) | ARGB_SET_A(num);
    } else {
        *color = num | ARGB_SET_A(0xff);
    }

    return true;
}

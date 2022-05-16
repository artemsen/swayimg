// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

config_t config = {
    .scale = scale_fit_or100,
    .background = BACKGROUND_GRID,
};

/** Config file location. */
struct config_locations {
    const char* prefix;  ///< Environment variable name
    const char* postfix; ///< Constant postfix
};
static const struct config_locations config_locations[] = {
    { "XDG_CONFIG_HOME", "/swayimg/config" },
    { "HOME", "/.config/swayimg/config" },
    { "XDG_CONFIG_DIRS", "/swayimg/config" },
    { NULL, "/etc/xdg/swayimg/config" }
};

/**
 * Expand path from environment variable.
 * @param[in] prefix_env path prefix (var name)
 * @param[in] postfix constant postfix
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
 * Apply property to configuration.
 * @param[in] key property key
 * @param[in] value property value
 */
static void apply_conf(const char* key, const char* value)
{
    const char* yes = "yes";
    const char* no = "no";

    if (strcmp(key, "scale") == 0) {
        if (strcmp(value, "default") == 0) {
            config.scale = scale_fit_or100;
        } else if (strcmp(value, "fit") == 0) {
            config.scale = scale_fit_window;
        } else if (strcmp(value, "real") == 0) {
            config.scale = scale_100;
        }
    } else if (strcmp(key, "fullscreen") == 0) {
        if (strcmp(value, yes) == 0) {
            config.fullscreen = true;
        } else if (strcmp(value, no) == 0) {
            config.fullscreen = false;
        }
    } else if (strcmp(key, "background") == 0) {
        set_background(value);
    } else if (strcmp(key, "info") == 0) {
        if (strcmp(value, yes) == 0) {
            config.show_info = true;
        } else if (strcmp(value, no) == 0) {
            config.show_info = false;
        }
    }
}

void load_config(void)
{
    FILE* fd = NULL;
    char* buff = NULL;
    size_t buff_sz = 0;
    ssize_t nread;

    for (size_t i = 0;
         !fd && i < sizeof(config_locations) / sizeof(config_locations[0]);
         ++i) {
        char* path = expand_path(config_locations[i].prefix,
                                 config_locations[i].postfix);
        if (path) {
            fd = fopen(path, "r");
            free(path);
        }
    }
    if (!fd) {
        // no config file found
        return;
    }

    while ((nread = getline(&buff, &buff_sz, fd)) != -1) {
        const char* value;
        char* delim;
        char* line = buff;
        // trim spaces
        while (nread-- && isspace(line[nread])) {
            line[nread] = 0;
        }
        while (*line && isspace(*line)) {
            ++line;
        }
        if (!*line || *line == '#') {
            continue; // skip empty lines and comments
        }
        delim = strchr(line, '=');
        if (!delim) {
            continue; // invalid format: delimiter not found
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
        apply_conf(line, value);
    }

    free(buff);
    fclose(fd);
}

bool set_scale(const char* value)
{
    if (strcmp(value, "default") == 0) {
        config.scale = scale_fit_or100;
    } else if (strcmp(value, "fit") == 0) {
        config.scale = scale_fit_window;
    } else if (strcmp(value, "real") == 0) {
        config.scale = scale_100;
    } else {
        return false;
    }
    return true;
}

bool set_background(const char* value)
{
    uint32_t bkg;

    if (strcmp(value, "grid") == 0) {
        bkg = BACKGROUND_GRID;
    } else {
        bkg = strtoul(value, NULL, 16);
        if (bkg > 0x00ffffff || errno == ERANGE ||
            (bkg == 0 && errno == EINVAL)) {
            return false;
        }
    }

    config.background = bkg;
    return true;
}

bool set_geometry(const char* value)
{
    int nums[4]; // x,y,width,height
    const char* ptr = value;
    size_t idx;

    for (idx = 0; *ptr && idx < sizeof(nums) / sizeof(nums[0]); ++idx) {
        nums[idx] = atoi(ptr);
        // skip digits
        while (isdigit(*ptr)) {
            ++ptr;
        }
        // skip delimiter
        while (*ptr && !isdigit(*ptr)) {
            ++ptr;
        }
    }

    if (idx == sizeof(nums) / sizeof(nums[0]) && !*ptr &&
        nums[2 /*width*/] > 0 && nums[3 /*height*/] > 0) {
        config.window.x = (int32_t)nums[0];
        config.window.y = (int32_t)nums[1];
        config.window.width = (uint32_t)nums[2];
        config.window.height = (uint32_t)nums[3];
        return true;
    }
    return false;
}

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

/**
 * Open user's configuration file.
 * @return file descriptior or NULL on errors
 */
static FILE* open_file(void)
{
    char path[64];
    size_t len;
    const char* postfix = "/swayimg/config";
    const size_t postfix_len = strlen(postfix);

    const char* config_dir = getenv("XDG_CONFIG_HOME");
    if (config_dir) {
        len = strlen(config_dir);
        if (len < sizeof(path)) {
            memcpy(path, config_dir, len + 1 /*last null*/);
        } else {
            len = 0;
        }
    } else {
        config_dir = getenv("HOME");
        len = config_dir ? strlen(config_dir) : 0;
        if (len && len < sizeof(path)) {
            memcpy(path, config_dir, len + 1 /*last null*/);
            const char* dir = "/.config";
            const size_t dlen = strlen(dir);
            if (len + dlen < sizeof(path)) {
                memcpy(path + len, dir, dlen + 1 /*last null*/);
                len += dlen;
            } else {
                len = 0;
            }
        }
    }

    if (len && len + postfix_len < sizeof(path)) {
        memcpy(path + len, postfix, postfix_len + 1 /*last null*/);
        return fopen(path, "r");
    }
    return NULL;
}

void load_config(void)
{
    char* buff = NULL;
    size_t buff_sz = 0;
    ssize_t nread;

    FILE* fd = open_file();
    if (!fd) {
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
    rect_t window;
    int* nums[] = { &window.x, &window.y, &window.width, &window.height };
    const char* ptr = value;
    size_t idx;

    for (idx = 0; *ptr && idx < sizeof(nums) / sizeof(nums[0]); ++idx) {
        *nums[idx] = atoi(ptr);
        // skip digits
        while (isdigit(*ptr)) {
            ++ptr;
        }
        // skip delimiter
        while (*ptr && !isdigit(*ptr)) {
            ++ptr;
        }
    }

    if (window.width <= 0 || window.height <= 0 ||
        idx != sizeof(nums) / sizeof(nums[0])) {
        return false;
    }

    config.window = window;
    return true;
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "config.h"

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

/** Config file location. */
typedef struct {
    const char* prefix;  ///< Environment variable name
    const char* postfix; ///< Constant postfix
} config_location_t;

static const config_location_t config_locations[] = {
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
 * Convert text value to boolean.
 * @param[in] text text to convert
 * @param[out] value target variable
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
 * Apply property to configuration.
 * @param[out] cfg target configuration instance
 * @param[in] key property key
 * @param[in] value property value
 * @return operation complete status, false if key was not handled
 */
static bool apply_conf(config_t* cfg, const char* key, const char* value)
{

    if (strcmp(key, "scale") == 0) {
        return set_scale_config(cfg, value);
    } else if (strcmp(key, "fullscreen") == 0) {
        return set_boolean(value, &cfg->fullscreen);
    } else if (strcmp(key, "background") == 0) {
        return set_background_config(cfg, value);
    } else if (strcmp(key, "info") == 0) {
        return set_boolean(value, &cfg->show_info);
    } else if (strcmp(key, "app_id") == 0) {
        return set_appid_config(cfg, value);
    } else if (strcmp(key, "rules") == 0) {
        return set_boolean(value, &cfg->sway_rules);
    }

    return false;
}

/**
 * Allocate and initialize default configuration instance.
 * @return created configuration instance
 */
static config_t* default_config(void)
{
    config_t* cfg;
    struct timespec ts;

    cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        return NULL;
    }

    cfg->scale = scale_fit_or100;
    cfg->background = BACKGROUND_GRID;
    cfg->sway_rules = true;

    // create unique application id
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        char app_id[64];
        snprintf(app_id, sizeof(app_id), APP_ID_BASE "_%lx",
                 (ts.tv_sec << 32) | ts.tv_nsec);
        set_appid_config(cfg, app_id);
    } else {
        set_appid_config(cfg, APP_ID_BASE);
    }
    if (!cfg->app_id) {
        free(cfg);
        return NULL;
    }

    return cfg;
}

config_t* init_config(void)
{
    config_t* cfg = NULL;
    FILE* fd = NULL;
    char* path = NULL;

    cfg = default_config();
    if (!cfg) {
        return NULL;
    }

    // find first available config file
    for (size_t i = 0;
         i < sizeof(config_locations) / sizeof(config_locations[0]); ++i) {
        const config_location_t* cl = &config_locations[i];
        path = expand_path(cl->prefix, cl->postfix);
        if (path) {
            fd = fopen(path, "r");
            if (fd) {
                break;
            }
        }
        free(path);
        path = NULL;
    }

    if (fd) {
        // read config file
        char* buff = NULL;
        size_t buff_sz = 0;
        size_t line_num = 0;
        ssize_t nread;

        while ((nread = getline(&buff, &buff_sz, fd)) != -1) {
            char* delim;
            const char* value;
            char* line = buff;
            ++line_num;
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
            // add configuration parameter from key/value pair
            if (!apply_conf(cfg, line, value)) {
                fprintf(stderr, "Invalid config file %s\n", path);
                fprintf(stderr, "Line %lu: [%s = %s]\n", line_num, line, value);
            }
        }
        free(buff);
        fclose(fd);
    }

    free(path);

    return cfg;
}

void free_config(config_t* cfg)
{
    if (cfg) {
        free((void*)cfg->app_id);
        free(cfg);
    }
}

bool check_config(const config_t* cfg)
{
    const char* err = NULL;

    if (cfg->window.width && !cfg->sway_rules) {
        err = "window geometry is set, but sway rules are disabled";
    }
    if (cfg->fullscreen && cfg->window.width) {
        err = "can not set geometry in full screen mode";
    }
    if (cfg->fullscreen && cfg->sway_rules) {
        err = "sway rules can not be used in full screen mode";
    }

    if (err) {
        fprintf(stderr, "Incompatible arguments: %s\n", err);
        return false;
    }

    return true;
}

bool set_scale_config(config_t* cfg, const char* scale)
{
    if (strcmp(scale, "default") == 0) {
        cfg->scale = scale_fit_or100;
    } else if (strcmp(scale, "fit") == 0) {
        cfg->scale = scale_fit_window;
    } else if (strcmp(scale, "real") == 0) {
        cfg->scale = scale_100;
    } else {
        fprintf(stderr, "Invalid scale: %s\n", scale);
        fprintf(stderr, "Expected 'default', 'fit', or 'real'.\n");
        return false;
    }
    return true;
}

bool set_background_config(config_t* cfg, const char* background)
{
    uint32_t bkg;

    if (strcmp(background, "grid") == 0) {
        bkg = BACKGROUND_GRID;
    } else {
        bkg = strtoul(background, NULL, 16);
        if (bkg > 0x00ffffff || errno == ERANGE ||
            (bkg == 0 && errno == EINVAL)) {
            fprintf(stderr, "Invalid background: %s\n", background);
            fprintf(stderr, "Expected 'grid' or RGB hex value.\n");
            return false;
        }
    }

    cfg->background = bkg;
    return true;
}

bool set_geometry_config(config_t* cfg, const char* geometry)
{
    int nums[4]; // x,y,width,height
    const char* ptr = geometry;
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
        cfg->window.x = (int32_t)nums[0];
        cfg->window.y = (int32_t)nums[1];
        cfg->window.width = (uint32_t)nums[2];
        cfg->window.height = (uint32_t)nums[3];
        return true;
    }

    fprintf(stderr, "Invalid window geometry: %s\n", geometry);
    fprintf(stderr, "Expected X,Y,W,H format.\n");
    return false;
}

bool set_appid_config(config_t* cfg, const char* app_id)
{
    char* ptr;
    size_t len;

    len = app_id ? strlen(app_id) : 0;
    if (len == 0 || len > APP_ID_MAX) {
        fprintf(stderr, "Invalid class/app_id: %s\n", app_id);
        fprintf(stderr, "Expected non-empty string up to %d chars.\n",
                APP_ID_MAX);
        return false;
    }

    ++len; // add last null

    ptr = malloc(len);
    if (!ptr) {
        fprintf(stderr, "Not enough memory\n");
        return false;
    }
    memcpy(ptr, app_id, len);

    free((void*)cfg->app_id);
    cfg->app_id = ptr;

    return true;
}

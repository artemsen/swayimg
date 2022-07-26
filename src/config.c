// SPDX-License-Identifier: MIT
// Program configuration.
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

// Default settings
#define DEFAULT_SCALE      cfgsc_optimal
#define DEFAULT_BACKGROUND BACKGROUND_GRID
#define DEFAULT_FRAME      COLOR_TRANSPARENT
#define DEFAULT_SWAYWM     true
#define DEFAULT_FONT_FACE  "monospace"
#define DEFAULT_FONT_SIZE  14
#define DEFAULT_FONT_COLOR 0xff00ff
#define DEFAULT_ORDER      cfgord_alpha
#define DEFAULT_RECURSIVE  false

/** Config file location. */
struct location {
    const char* prefix;  ///< Environment variable name
    const char* postfix; ///< Constant postfix
};

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
 * Apply property to configuration.
 * @param ctx configuration context
 * @param key property key
 * @param value property value
 * @return operation complete status, false if key was not handled
 */
static bool apply_conf(struct config* ctx, const char* key, const char* value)
{

    if (strcmp(key, "scale") == 0) {
        return config_set_scale(ctx, value);
    } else if (strcmp(key, "fullscreen") == 0) {
        return set_boolean(value, &ctx->fullscreen);
    } else if (strcmp(key, "background") == 0) {
        return config_set_background(ctx, value);
    } else if (strcmp(key, "frame") == 0) {
        return config_set_frame(ctx, value);
    } else if (strcmp(key, "info") == 0) {
        return set_boolean(value, &ctx->show_info);
    } else if (strcmp(key, "font") == 0) {
        return config_set_font_name(ctx, value);
    } else if (strcmp(key, "font-size") == 0) {
        return config_set_font_size(ctx, value);
    } else if (strcmp(key, "font-color") == 0) {
        return set_color(value, &ctx->font_color);
    } else if (strcmp(key, "order") == 0) {
        return config_set_order(ctx, value);
    } else if (strcmp(key, "recursive") == 0) {
        return set_boolean(value, &ctx->recursive);
    } else if (strcmp(key, "app_id") == 0) {
        return config_set_appid(ctx, value);
    } else if (strcmp(key, "sway") == 0) {
        return set_boolean(value, &ctx->sway_wm);
    }

    return false;
}

/**
 * Allocate and initialize default configuration instance.
 * @return created configuration instance
 */
static struct config* default_config(void)
{
    struct config* ctx;
    struct timespec ts;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->scale = DEFAULT_SCALE;
    ctx->background = BACKGROUND_GRID;
    ctx->frame = DEFAULT_FRAME;
    ctx->sway_wm = DEFAULT_SWAYWM;
    config_set_font_name(ctx, DEFAULT_FONT_FACE);
    ctx->font_color = DEFAULT_FONT_COLOR;
    ctx->font_size = DEFAULT_FONT_SIZE;
    ctx->order = DEFAULT_ORDER;
    ctx->recursive = DEFAULT_RECURSIVE;

    // create unique application id
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        char app_id[64];
        snprintf(app_id, sizeof(app_id), APP_ID_BASE "_%lx",
                 (ts.tv_sec << 32) | ts.tv_nsec);
        config_set_appid(ctx, app_id);
    } else {
        config_set_appid(ctx, APP_ID_BASE);
    }
    if (!ctx->app_id) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

struct config* config_init(void)
{
    struct config* ctx;
    FILE* fd = NULL;
    char* path = NULL;

    ctx = default_config();
    if (!ctx) {
        return NULL;
    }

    // find first available config file
    for (size_t i = 0;
         i < sizeof(config_locations) / sizeof(config_locations[0]); ++i) {
        const struct location* cl = &config_locations[i];
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
            if (!apply_conf(ctx, line, value)) {
                fprintf(stderr, "Invalid config file %s\n", path);
                fprintf(stderr, "Line %lu: [%s = %s]\n", line_num, line, value);
            }
        }
        free(buff);
        fclose(fd);
    }

    free(path);

    return ctx;
}

void config_free(struct config* ctx)
{
    if (ctx) {
        free((void*)ctx->font_face);
        free((void*)ctx->app_id);
        free(ctx);
    }
}

bool config_check(const struct config* ctx)
{
    const char* err = NULL;

    if (ctx->window.width && !ctx->sway_wm) {
        err = "window geometry is set, but sway rules are disabled";
    }
    if (ctx->fullscreen && ctx->window.width) {
        err = "can not set geometry in full screen mode";
    }
    if (ctx->fullscreen && ctx->sway_wm) {
        err = "sway rules can not be used in full screen mode";
    }

    if (err) {
        fprintf(stderr, "Incompatible arguments: %s\n", err);
        return false;
    }

    return true;
}

bool config_set_scale(struct config* ctx, const char* scale)
{
    if (strcmp(scale, "optimal") == 0) {
        ctx->scale = cfgsc_optimal;
    } else if (strcmp(scale, "fit") == 0) {
        ctx->scale = cfgsc_fit;
    } else if (strcmp(scale, "real") == 0) {
        ctx->scale = cfgsc_real;
    } else {
        fprintf(stderr, "Invalid scale: %s\n", scale);
        fprintf(stderr, "Expected 'optimal', 'fit', or 'real'.\n");
        return false;
    }
    return true;
}

bool config_set_background(struct config* ctx, const char* background)
{
    if (strcmp(background, "grid") == 0) {
        ctx->background = BACKGROUND_GRID;
    } else if (strcmp(background, "none") == 0) {
        ctx->background = COLOR_TRANSPARENT;
    } else if (!set_color(background, &ctx->background)) {
        fprintf(stderr, "Invalid background: %s\n", background);
        fprintf(stderr, "Expected 'none', 'grid', or RGB hex value.\n");
        return false;
    }
    return true;
}

bool config_set_frame(struct config* ctx, const char* frame)
{
    if (strcmp(frame, "none") == 0) {
        ctx->frame = COLOR_TRANSPARENT;
    } else if (!set_color(frame, &ctx->frame)) {
        fprintf(stderr, "Invalid frame color: %s\n", frame);
        fprintf(stderr, "Expected 'none' or RGB hex value.\n");
        return false;
    }
    return true;
}

bool config_set_geometry(struct config* ctx, const char* geometry)
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
        ctx->window.x = (int32_t)nums[0];
        ctx->window.y = (int32_t)nums[1];
        ctx->window.width = (uint32_t)nums[2];
        ctx->window.height = (uint32_t)nums[3];
        return true;
    }

    fprintf(stderr, "Invalid window geometry: %s\n", geometry);
    fprintf(stderr, "Expected X,Y,W,H format.\n");
    return false;
}

bool config_set_font_name(struct config* ctx, const char* font)
{
    const size_t len = font ? strlen(font) : 0;
    if (len == 0) {
        fprintf(stderr, "Invalid font name\n");
        return false;
    }
    return set_string(font, (char**)&ctx->font_face);
}

bool config_set_font_size(struct config* ctx, const char* size)
{
    char* endptr;
    const unsigned long sz = strtoul(size, &endptr, 10);
    if (*endptr || sz == 0) {
        fprintf(stderr, "Invalid font size\n");
        return false;
    }
    ctx->font_size = sz;
    return true;
}

bool config_set_order(struct config* ctx, const char* order)
{
    if (strcmp(order, "none") == 0) {
        ctx->order = cfgord_none;
    } else if (strcmp(order, "alpha") == 0) {
        ctx->order = cfgord_alpha;
    } else if (strcmp(order, "random") == 0) {
        ctx->order = cfgord_random;
    } else {
        fprintf(stderr, "Invalid file list order: %s\n", order);
        fprintf(stderr, "Expected 'none', 'alpha', or 'random'.\n");
        return false;
    }
    return true;
}

bool config_set_appid(struct config* ctx, const char* app_id)
{
    const size_t len = app_id ? strlen(app_id) : 0;
    if (len == 0 || len > APP_ID_MAX) {
        fprintf(stderr, "Invalid class/app_id: %s\n", app_id);
        fprintf(stderr, "Expected non-empty string up to %d chars.\n",
                APP_ID_MAX);
        return false;
    }
    return set_string(app_id, (char**)&ctx->app_id);
}

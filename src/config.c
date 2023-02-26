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

// Section names
#define SECTION_GENERAL "general"
#define SECTION_KEYBIND "keys"

// Default key bindings
static struct config_keybind default_bindings[] = {
    { XKB_KEY_Home, cfgact_first_file },
    { XKB_KEY_g, cfgact_first_file },
    { XKB_KEY_End, cfgact_last_file },
    { XKB_KEY_G, cfgact_last_file },
    { XKB_KEY_P, cfgact_prev_dir },
    { XKB_KEY_N, cfgact_next_dir },
    { XKB_KEY_SunPageUp, cfgact_prev_file },
    { XKB_KEY_p, cfgact_prev_file },
    { XKB_KEY_SunPageDown, cfgact_next_file },
    { XKB_KEY_n, cfgact_next_file },
    { XKB_KEY_space, cfgact_next_file },
    { XKB_KEY_F2, cfgact_prev_frame },
    { XKB_KEY_O, cfgact_prev_frame },
    { XKB_KEY_F3, cfgact_next_frame },
    { XKB_KEY_o, cfgact_next_frame },
    { XKB_KEY_F4, cfgact_animation },
    { XKB_KEY_s, cfgact_animation },
    { XKB_KEY_F9, cfgact_slideshow },
    { XKB_KEY_F11, cfgact_fullscreen },
    { XKB_KEY_f, cfgact_fullscreen },
    { XKB_KEY_Left, cfgact_step_left },
    { XKB_KEY_h, cfgact_step_left },
    { XKB_KEY_Right, cfgact_step_right },
    { XKB_KEY_l, cfgact_step_right },
    { XKB_KEY_Up, cfgact_step_up },
    { XKB_KEY_k, cfgact_step_up },
    { XKB_KEY_Down, cfgact_step_down },
    { XKB_KEY_j, cfgact_step_down },
    { XKB_KEY_equal, cfgact_zoom_in },
    { XKB_KEY_plus, cfgact_zoom_in },
    { XKB_KEY_minus, cfgact_zoom_out },
    { XKB_KEY_0, cfgact_zoom_real },
    { XKB_KEY_BackSpace, cfgact_zoom_reset },
    { XKB_KEY_F5, cfgact_rotate_left },
    { XKB_KEY_bracketleft, cfgact_rotate_left },
    { XKB_KEY_F6, cfgact_rotate_right },
    { XKB_KEY_bracketright, cfgact_rotate_right },
    { XKB_KEY_F7, cfgact_flip_vertical },
    { XKB_KEY_F8, cfgact_flip_horizontal },
    { XKB_KEY_a, cfgact_antialiasing },
    { XKB_KEY_r, cfgact_reload },
    { XKB_KEY_i, cfgact_info },
    { XKB_KEY_e, cfgact_exec },
    { XKB_KEY_Escape, cfgact_quit },
    { XKB_KEY_Return, cfgact_quit },
    { XKB_KEY_F10, cfgact_quit },
    { XKB_KEY_q, cfgact_quit },
};

/* Link between action id and its name in config file */
struct action_name {
    const char* name;
    enum config_action action;
};

static const struct action_name action_names[] = {
    { "none", cfgact_none },
    { "first_file", cfgact_first_file },
    { "last_file", cfgact_last_file },
    { "prev_dir", cfgact_prev_dir },
    { "next_dir", cfgact_next_dir },
    { "prev_file", cfgact_prev_file },
    { "next_file", cfgact_next_file },
    { "prev_frame", cfgact_prev_frame },
    { "next_frame", cfgact_next_frame },
    { "animation", cfgact_animation },
    { "slideshow", cfgact_slideshow },
    { "fullscreen", cfgact_fullscreen },
    { "step_left", cfgact_step_left },
    { "step_right", cfgact_step_right },
    { "step_up", cfgact_step_up },
    { "step_down", cfgact_step_down },
    { "zoom_in", cfgact_zoom_in },
    { "zoom_out", cfgact_zoom_out },
    { "zoom_real", cfgact_zoom_real },
    { "zoom_reset", cfgact_zoom_reset },
    { "rotate_left", cfgact_rotate_left },
    { "rotate_right", cfgact_rotate_right },
    { "flip_vertical", cfgact_flip_vertical },
    { "flip_horizontal", cfgact_flip_horizontal },
    { "antialiasing", cfgact_antialiasing },
    { "reload", cfgact_reload },
    { "info", cfgact_info },
    { "exec", cfgact_exec },
    { "quit", cfgact_quit },
};

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
 * Apply global property to configuration.
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
    } else if (strcmp(key, "window") == 0) {
        return config_set_window(ctx, value);
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
    } else if (strcmp(key, "loop") == 0) {
        return set_boolean(value, &ctx->loop);
    } else if (strcmp(key, "recursive") == 0) {
        return set_boolean(value, &ctx->recursive);
    } else if (strcmp(key, "all") == 0) {
        return set_boolean(value, &ctx->all_files);
    } else if (strcmp(key, "slideshow") == 0) {
        return config_set_slideshow_sec(ctx, value);
    } else if (strcmp(key, "exec") == 0) {
        return config_set_exec_cmd(ctx, value);
    } else if (strcmp(key, "app_id") == 0) {
        return config_set_appid(ctx, value);
    } else if (strcmp(key, "sway") == 0) {
        return set_boolean(value, &ctx->sway_wm);
    }
    fprintf(stderr, "Invalid config key name: %s\n", key);
    return false;
}

/**
 * Apply key binding to configuration.
 * @param ctx configuration context
 * @param key property key
 * @param value property value
 * @return operation complete status, false if key was not handled
 */
static bool apply_key(struct config* ctx, const char* key, const char* value)
{
    xkb_keysym_t keysym;
    enum config_action action;
    size_t index;

    // convert key name to code
    keysym = xkb_keysym_from_name(key, XKB_KEYSYM_NO_FLAGS);
    if (keysym == XKB_KEY_NoSymbol) {
        fprintf(stderr, "Invalid key binding: %s\n", key);
        return false;
    }

    // convert action name to code
    for (index = 0; index < sizeof(action_names) / sizeof(action_names[0]);
         ++index) {
        if (strcmp(value, action_names[index].name) == 0) {
            action = action_names[index].action;
            break;
        }
    }
    if (index == sizeof(action_names) / sizeof(action_names[0])) {
        fprintf(stderr, "Invalid binding action: %s\n", value);
        return false;
    }

    // replace previous binding
    for (index = 0; index < MAX_KEYBINDINGS; ++index) {
        struct config_keybind* bind = &ctx->keybind[index];
        if (bind->key == XKB_KEY_NoSymbol) {
            break;
        }
        if (bind->key == keysym) {
            bind->action = action;
            return true;
        }
    }

    // add new binding
    if (index == MAX_KEYBINDINGS) {
        fprintf(stderr, "Too many key bindins\n");
        return false;
    }
    ctx->keybind[index].key = keysym;
    ctx->keybind[index].action = action;

    return true;
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

    // default settings
    ctx->scale = cfgsc_optimal;
    ctx->background = BACKGROUND_GRID;
    ctx->window = COLOR_TRANSPARENT;
    ctx->sway_wm = true;
    config_set_font_name(ctx, "monospace");
    ctx->font_color = 0xcccccc;
    ctx->font_size = 14;
    ctx->slideshow = false;
    ctx->slideshow_sec = 3;
    ctx->order = cfgord_alpha;
    ctx->loop = true;
    ctx->recursive = false;
    ctx->all_files = false;
    ctx->antialiasing = false;
    config_set_exec_cmd(ctx, "echo '%'");
    memcpy(ctx->keybind, default_bindings, sizeof(default_bindings));

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

/**
 * Load configuration from a file.
 * @param ctx configuration context
 * @param path full path to the file
 * @return operation complete status, false on error
 */
static bool load_config(struct config* ctx, const char* path)
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

        // add configuration parameter from key/value pair
        if (!section || strcmp(section, SECTION_GENERAL) == 0) {
            status = apply_conf(ctx, line, value);
        } else if (strcmp(section, SECTION_KEYBIND) == 0) {
            status = apply_key(ctx, line, value);
        } else {
            fprintf(stderr, "Invalid section name: '%s'\n", section);
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

struct config* config_init(void)
{
    struct config* ctx;

    ctx = default_config();
    if (!ctx) {
        return NULL;
    }

    // find first available config file
    for (size_t i = 0;
         i < sizeof(config_locations) / sizeof(config_locations[0]); ++i) {
        const struct location* cl = &config_locations[i];
        char* path = expand_path(cl->prefix, cl->postfix);
        if (path && load_config(ctx, path)) {
            free(path);
            break;
        }
        free(path);
    }

    return ctx;
}

void config_free(struct config* ctx)
{
    if (ctx) {
        free((void*)ctx->font_face);
        free((void*)ctx->exec_cmd);
        free((void*)ctx->app_id);
        free(ctx);
    }
}

bool config_set_scale(struct config* ctx, const char* val)
{
    if (strcmp(val, "optimal") == 0) {
        ctx->scale = cfgsc_optimal;
    } else if (strcmp(val, "fit") == 0) {
        ctx->scale = cfgsc_fit;
    } else if (strcmp(val, "real") == 0) {
        ctx->scale = cfgsc_real;
    } else {
        fprintf(stderr, "Invalid scale: %s\n", val);
        fprintf(stderr, "Expected 'optimal', 'fit', or 'real'.\n");
        return false;
    }
    return true;
}

bool config_set_background(struct config* ctx, const char* val)
{
    if (strcmp(val, "grid") == 0) {
        ctx->background = BACKGROUND_GRID;
    } else if (strcmp(val, "none") == 0) {
        ctx->background = COLOR_TRANSPARENT;
    } else if (!set_color(val, &ctx->background)) {
        fprintf(stderr, "Invalid image background: %s\n", val);
        fprintf(stderr, "Expected 'none', 'grid', or RGB hex value.\n");
        return false;
    }
    return true;
}

bool config_set_window(struct config* ctx, const char* val)
{
    if (strcmp(val, "none") == 0) {
        ctx->window = COLOR_TRANSPARENT;
    } else if (!set_color(val, &ctx->window)) {
        fprintf(stderr, "Invalid window background: %s\n", val);
        fprintf(stderr, "Expected 'none' or RGB hex value.\n");
        return false;
    }
    return true;
}

bool config_set_geometry(struct config* ctx, const char* val)
{
    long nums[4]; // x,y,width,height
    const char* ptr = val;
    size_t idx;

    for (idx = 0; *ptr && idx < sizeof(nums) / sizeof(nums[0]); ++idx) {
        char* endptr;
        nums[idx] = strtol(ptr, &endptr, 0);
        if (ptr == endptr) {
            break;
        }
        ptr = endptr;
        while (*ptr && *ptr == ',') {
            ++ptr;
        }
    }

    if (idx == sizeof(nums) / sizeof(nums[0]) && !*ptr &&
        nums[2 /*width*/] > 0 && nums[3 /*height*/] > 0) {
        ctx->geometry.x = (ssize_t)nums[0];
        ctx->geometry.y = (ssize_t)nums[1];
        ctx->geometry.width = (size_t)nums[2];
        ctx->geometry.height = (size_t)nums[3];
        return true;
    }

    fprintf(stderr, "Invalid window geometry: %s\n", val);
    fprintf(stderr, "Expected X,Y,W,H format.\n");
    return false;
}

bool config_set_font_name(struct config* ctx, const char* val)
{
    const size_t len = strlen(val);
    if (len == 0) {
        fprintf(stderr, "Invalid font name\n");
        return false;
    }
    return set_string(val, (char**)&ctx->font_face);
}

bool config_set_font_size(struct config* ctx, const char* val)
{
    char* endptr;
    const unsigned long sz = strtoul(val, &endptr, 10);
    if (*endptr || sz == 0) {
        fprintf(stderr, "Invalid font size\n");
        return false;
    }
    ctx->font_size = sz;
    return true;
}

bool config_set_order(struct config* ctx, const char* val)
{
    if (strcmp(val, "none") == 0) {
        ctx->order = cfgord_none;
    } else if (strcmp(val, "alpha") == 0) {
        ctx->order = cfgord_alpha;
    } else if (strcmp(val, "random") == 0) {
        ctx->order = cfgord_random;
    } else {
        fprintf(stderr, "Invalid file list order: %s\n", val);
        fprintf(stderr, "Expected 'none', 'alpha', or 'random'.\n");
        return false;
    }
    return true;
}

bool config_set_slideshow_sec(struct config* ctx, const char* val)
{
    char* endptr;
    const unsigned long sec = strtoul(val, &endptr, 10);
    if (*endptr || sec == 0) {
        fprintf(stderr, "Invalid slideshow duration\n");
        return false;
    }
    ctx->slideshow_sec = sec;
    return true;
}

bool config_set_appid(struct config* ctx, const char* val)
{
    const size_t len = strlen(val);
    if (len == 0 || len > APP_ID_MAX) {
        fprintf(stderr, "Invalid class/app_id: %s\n", val);
        fprintf(stderr, "Expected non-empty string up to %d chars.\n",
                APP_ID_MAX);
        return false;
    }
    return set_string(val, (char**)&ctx->app_id);
}

bool config_set_exec_cmd(struct config* ctx, const char* val)
{
    const size_t len = strlen(val);
    if (len) {
        return set_string(val, (char**)&ctx->exec_cmd);
    }
    return true;
}

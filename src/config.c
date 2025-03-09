// SPDX-License-Identifier: MIT
// Program configuration.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include "memdata.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Default configuration. */
struct config_default {
    const char* section;
    const char* key;
    const char* value;
};
// clang-format off
static const struct config_default defaults[] = {
    { CFG_GENERAL,      CFG_GNRL_MODE,      "viewer"                 },
    { CFG_GENERAL,      CFG_GNRL_POSITION,  "parent"                 },
    { CFG_GENERAL,      CFG_GNRL_SIZE,      "parent"                 },
    { CFG_GENERAL,      CFG_GNRL_DECOR,     CFG_NO                   },
    { CFG_GENERAL,      CFG_GNRL_SIGUSR1,   "reload"                 },
    { CFG_GENERAL,      CFG_GNRL_SIGUSR2,   "next_file"              },
    { CFG_GENERAL,      CFG_GNRL_APP_ID,    "swayimg"                },

    { CFG_VIEWER,       CFG_VIEW_WINDOW,    "#00000000"              },
    { CFG_VIEWER,       CFG_VIEW_TRANSP,    "grid"                   },
    { CFG_VIEWER,       CFG_VIEW_SCALE,     "optimal"                },
    { CFG_VIEWER,       CFG_VIEW_KEEP_ZM,   CFG_NO                   },
    { CFG_VIEWER,       CFG_VIEW_POSITION,  "center"                 },
    { CFG_VIEWER,       CFG_VIEW_FIXED,     CFG_YES                  },
    { CFG_VIEWER,       CFG_VIEW_AA,        "mks13"                  },
    { CFG_VIEWER,       CFG_VIEW_SSHOW,     CFG_NO                   },
    { CFG_VIEWER,       CFG_VIEW_SSHOW_TM,  "3"                      },
    { CFG_VIEWER,       CFG_VIEW_HISTORY,   "1"                      },
    { CFG_VIEWER,       CFG_VIEW_PRELOAD,   "1"                      },

    { CFG_GALLERY,      CFG_GLRY_SIZE,      "200"                    },
    { CFG_GALLERY,      CFG_GLRY_CACHE,     "100"                    },
    { CFG_GALLERY,      CFG_GLRY_PSTORE,    CFG_NO                   },
    { CFG_GALLERY,      CFG_GLRY_FILL,      CFG_YES                  },
    { CFG_GALLERY,      CFG_GLRY_AA,        "mks13"                  },
    { CFG_GALLERY,      CFG_GLRY_WINDOW,    "#00000000"              },
    { CFG_GALLERY,      CFG_GLRY_BKG,       "#202020ff"              },
    { CFG_GALLERY,      CFG_GLRY_SELECT,    "#404040ff"              },
    { CFG_GALLERY,      CFG_GLRY_BORDER,    "#000000ff"              },
    { CFG_GALLERY,      CFG_GLRY_SHADOW,    "#000000ff"              },

    { CFG_LIST,         CFG_LIST_ORDER,     "alpha"                  },
    { CFG_LIST,         CFG_LIST_LOOP,      CFG_YES                  },
    { CFG_LIST,         CFG_LIST_RECURSIVE, CFG_NO                   },
    { CFG_LIST,         CFG_LIST_ALL,       CFG_NO                   },

    { CFG_FONT,         CFG_FONT_NAME,      "monospace"              },
    { CFG_FONT,         CFG_FONT_SIZE,      "14"                     },
    { CFG_FONT,         CFG_FONT_COLOR,     "#ccccccff"              },
    { CFG_FONT,         CFG_FONT_SHADOW,    "#000000d0"              },
    { CFG_FONT,         CFG_FONT_BKG,       "#00000000"              },

    { CFG_INFO,         CFG_INFO_SHOW,      CFG_YES                  },
    { CFG_INFO,         CFG_INFO_ITIMEOUT,  "5"                      },
    { CFG_INFO,         CFG_INFO_STIMEOUT,  "3"                      },

    { CFG_INFO_VIEWER,  CFG_INFO_CN,        "none"                   },
    { CFG_INFO_VIEWER,  CFG_INFO_TL,        "+name,+format,+filesize,+imagesize,+exif" },
    { CFG_INFO_VIEWER,  CFG_INFO_TR,        "index"                  },
    { CFG_INFO_VIEWER,  CFG_INFO_BL,        "scale,frame"            },
    { CFG_INFO_VIEWER,  CFG_INFO_BR,        "status"                 },

    { CFG_INFO_GALLERY, CFG_INFO_CN,        "none"                   },
    { CFG_INFO_GALLERY, CFG_INFO_TL,        "none"                   },
    { CFG_INFO_GALLERY, CFG_INFO_TR,        "none"                   },
    { CFG_INFO_GALLERY, CFG_INFO_BL,        "none"                   },
    { CFG_INFO_GALLERY, CFG_INFO_BR,        "name,status"            },

    { CFG_KEYS_VIEWER,  "F1",               "help"                   },
    { CFG_KEYS_VIEWER,  "Home",             "first_file"             },
    { CFG_KEYS_VIEWER,  "End",              "last_file"              },
    { CFG_KEYS_VIEWER,  "Prior",            "prev_file"              },
    { CFG_KEYS_VIEWER,  "Next",             "next_file"              },
    { CFG_KEYS_VIEWER,  "Space",            "next_file"              },
    { CFG_KEYS_VIEWER,  "Shift+d",          "prev_dir"               },
    { CFG_KEYS_VIEWER,  "d",                "next_dir"               },
    { CFG_KEYS_VIEWER,  "Shift+r",          "rand_file"              },
    { CFG_KEYS_VIEWER,  "Shift+o",          "prev_frame"             },
    { CFG_KEYS_VIEWER,  "o",                "next_frame"             },
    { CFG_KEYS_VIEWER,  "c",                "skip_file"              },
    { CFG_KEYS_VIEWER,  "Shift+s",          "slideshow"              },
    { CFG_KEYS_VIEWER,  "s",                "animation"              },
    { CFG_KEYS_VIEWER,  "f",                "fullscreen"             },
    { CFG_KEYS_VIEWER,  "Return",           "mode"                   },
    { CFG_KEYS_VIEWER,  "Left",             "step_left 10"           },
    { CFG_KEYS_VIEWER,  "Right",            "step_right 10"          },
    { CFG_KEYS_VIEWER,  "Up",               "step_up 10"             },
    { CFG_KEYS_VIEWER,  "Down",             "step_down 10"           },
    { CFG_KEYS_VIEWER,  "Equal",            "zoom +10"               },
    { CFG_KEYS_VIEWER,  "Plus",             "zoom +10"               },
    { CFG_KEYS_VIEWER,  "Minus",            "zoom -10"               },
    { CFG_KEYS_VIEWER,  "w",                "zoom width"             },
    { CFG_KEYS_VIEWER,  "Shift+w",          "zoom height"            },
    { CFG_KEYS_VIEWER,  "z",                "zoom fit"               },
    { CFG_KEYS_VIEWER,  "Shift+z",          "zoom fill"              },
    { CFG_KEYS_VIEWER,  "0",                "zoom real"              },
    { CFG_KEYS_VIEWER,  "BackSpace",        "zoom optimal"           },
    { CFG_KEYS_VIEWER,  "Alt+z",            "keep_zoom"              },
    { CFG_KEYS_VIEWER,  "bracketleft",      "rotate_left"            },
    { CFG_KEYS_VIEWER,  "bracketright",     "rotate_right"           },
    { CFG_KEYS_VIEWER,  "m",                "flip_vertical"          },
    { CFG_KEYS_VIEWER,  "Shift+m",          "flip_horizontal"        },
    { CFG_KEYS_VIEWER,  "a",                "antialiasing next"      },
    { CFG_KEYS_VIEWER,  "Shift+a",          "antialiasing prev"      },
    { CFG_KEYS_VIEWER,  "r",                "reload"                 },
    { CFG_KEYS_VIEWER,  "i",                "info"                   },
    { CFG_KEYS_VIEWER,  "Shift+Delete",     "exec rm -f '%'; skip_file" },
    { CFG_KEYS_VIEWER,  "Escape",           "exit"                   },
    { CFG_KEYS_VIEWER,  "q",                "exit"                   },
    { CFG_KEYS_VIEWER,  "ScrollLeft",       "step_right 5"           },
    { CFG_KEYS_VIEWER,  "ScrollRight",      "step_left 5"            },
    { CFG_KEYS_VIEWER,  "ScrollUp",         "step_up 5"              },
    { CFG_KEYS_VIEWER,  "ScrollDown",       "step_down 5"            },
    { CFG_KEYS_VIEWER,  "Ctrl+ScrollUp",    "zoom +10"               },
    { CFG_KEYS_VIEWER,  "Ctrl+ScrollDown",  "zoom -10"               },
    { CFG_KEYS_VIEWER,  "Shift+ScrollUp",   "prev_file"              },
    { CFG_KEYS_VIEWER,  "Shift+ScrollDown", "next_file"              },
    { CFG_KEYS_VIEWER,  "Alt+ScrollUp",     "prev_frame"             },
    { CFG_KEYS_VIEWER,  "Alt+ScrollDown",   "next_frame"             },

    { CFG_KEYS_GALLERY, "F1",               "help"                   },
    { CFG_KEYS_GALLERY, "Home",             "first_file"             },
    { CFG_KEYS_GALLERY, "End",              "last_file"              },
    { CFG_KEYS_GALLERY, "Left",             "step_left"              },
    { CFG_KEYS_GALLERY, "Right",            "step_right"             },
    { CFG_KEYS_GALLERY, "Up",               "step_up"                },
    { CFG_KEYS_GALLERY, "Down",             "step_down"              },
    { CFG_KEYS_GALLERY, "Prior",            "page_up"                },
    { CFG_KEYS_GALLERY, "Next",             "page_down"              },
    { CFG_KEYS_GALLERY, "c",                "skip_file"              },
    { CFG_KEYS_GALLERY, "f",                "fullscreen"             },
    { CFG_KEYS_GALLERY, "Return",           "mode"                   },
    { CFG_KEYS_GALLERY, "a",                "antialiasing next"      },
    { CFG_KEYS_GALLERY, "Shift+a",          "antialiasing prev"      },
    { CFG_KEYS_GALLERY, "r",                "reload"                 },
    { CFG_KEYS_GALLERY, "i",                "info"                   },
    { CFG_KEYS_GALLERY, "Shift+Delete",     "exec rm -f '%'; skip_file" },
    { CFG_KEYS_GALLERY, "Escape",           "exit"                   },
    { CFG_KEYS_GALLERY, "q",                "exit"                   },
    { CFG_KEYS_GALLERY, "ScrollLeft",       "step_right"             },
    { CFG_KEYS_GALLERY, "ScrollRight",      "step_left"              },
    { CFG_KEYS_GALLERY, "ScrollUp",         "step_up"                },
    { CFG_KEYS_GALLERY, "ScrollDown",       "step_down"              },
};
// clang-format on

/** Config file location. */
struct location {
    const char* prefix;  ///< Environment variable name
    const char* postfix; ///< Constant postfix
};

static const struct location config_locations[] = {
    { "XDG_CONFIG_HOME", "/swayimg/config"         },
    { "HOME",            "/.config/swayimg/config" },
    { "XDG_CONFIG_DIRS", "/swayimg/config"         },
    { NULL,              "/etc/xdg/swayimg/config" }
};

/**
 * Load configuration from a file.
 * @param cfg config instance
 * @param path full path to the file
 * @return false on errors
 */
static bool load(struct config* cfg, const char* path)
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
            char* new_section;
            ++line;
            delim = strchr(line, ']');
            if (!delim || line + 1 == delim) {
                fprintf(stderr, "WARNING: Invalid config line in %s:%zu\n",
                        path, line_num);
                continue;
            }
            *delim = 0;
            len = delim - line + 1;
            new_section = realloc(section, len);
            if (new_section) {
                section = new_section;
                memcpy(section, line, len);
            }
            continue;
        }

        if (!section) {
            fprintf(stderr,
                    "WARNING: Config parameter without section in %s:%zu\n",
                    path, line_num);
            continue;
        }

        delim = strchr(line, '=');
        if (!delim) {
            fprintf(stderr, "WARNING: Invalid config line in %s:%zu\n", path,
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

        // save configuration parameter
        config_set(cfg, section, line, value);
    }

    free(buff);
    free(section);
    fclose(fd);

    return true;
}

/**
 * Create key/value entry.
 * @param key,value config param
 * @return key/value entry
 */
static struct config_keyval* create_keyval(const char* key, const char* value)
{
    const size_t key_sz = strlen(key) + 1 /*last null*/;
    const size_t value_sz = strlen(value) + 1 /*last null*/;

    struct config_keyval* kv =
        calloc(1, sizeof(struct config_keyval) + key_sz + value_sz);
    if (kv) {
        kv->key = (char*)kv + sizeof(struct config_keyval);
        memcpy(kv->key, key, key_sz);
        kv->value = (char*)kv + sizeof(struct config_keyval) + key_sz;
        memcpy(kv->value, value, value_sz);
    }

    return kv;
}

/**
 * Create section in the config instance.
 * @param cfg config instance
 * @param name section name
 */
static void create_section(struct config** cfg, const char* name)
{
    const size_t sz = strlen(name) + 1 /*last null*/;
    struct config* section = calloc(1, sizeof(struct config) + sz);
    if (section) {
        section->name = (char*)section + sizeof(struct config);
        memcpy(section->name, name, sz);
        *cfg = list_add(*cfg, section);
    }
}

/**
 * Convert text value to boolean config.
 * @param text source config value
 * @param value parsed value
 * @param false if text has invalid format
 */
static bool text_to_bool(const char* text, bool* value)
{
    if (strcmp(text, CFG_YES) == 0) {
        *value = true;
    } else if (strcmp(text, CFG_NO) == 0) {
        *value = false;
    } else {
        return false;
    }
    return true;
}

/**
 * Convert text value to color.
 * @param text source config value
 * @param value parsed value
 * @param false if text has invalid format
 */
static bool text_to_color(const char* text, argb_t* value)
{
    char* endptr;
    argb_t color;

    while (*text == '#' || isspace(*text)) {
        ++text;
    }

    errno = 0;

    color = strtoull(text, &endptr, 16);

    if (endptr && !*endptr && errno == 0) {
        if (strlen(text) > 6) { // value with alpha (RRGGBBAA)
            color = (color >> 8) | ARGB_SET_A(color);
        } else {
            color |= ARGB_SET_A(0xff);
        }
        *value = color;
        return true;
    }

    return false;
}

struct config* config_load(void)
{
    struct config* cfg = NULL;

    // create sections
    create_section(&cfg, CFG_GENERAL);
    create_section(&cfg, CFG_VIEWER);
    create_section(&cfg, CFG_GALLERY);
    create_section(&cfg, CFG_LIST);
    create_section(&cfg, CFG_FONT);
    create_section(&cfg, CFG_INFO);
    create_section(&cfg, CFG_INFO_VIEWER);
    create_section(&cfg, CFG_INFO_GALLERY);
    create_section(&cfg, CFG_KEYS_VIEWER);
    create_section(&cfg, CFG_KEYS_GALLERY);

    // load default config
    for (size_t i = 0; i < ARRAY_SIZE(defaults); ++i) {
        const struct config_default* def = &defaults[i];
        list_for_each(cfg, struct config, section) {
            if (strcmp(def->section, section->name) == 0) {
                struct config_keyval* kv = create_keyval(def->key, def->value);
                if (kv) {
                    section->params = list_add(section->params, kv);
                }
                break;
            }
        }
    }

    // find and load first available config file
    for (size_t i = 0; i < ARRAY_SIZE(config_locations); ++i) {
        const struct location* cl = &config_locations[i];
        char* path = config_expand_path(cl->prefix, cl->postfix);
        const bool loaded = path && load(cfg, path);
        free(path);
        if (loaded) {
            break;
        }
    }

    return cfg;
}

void config_free(struct config* cfg)
{
    list_for_each(cfg, struct config, section) {
        list_for_each(section->params, struct config_keyval, kv) {
            free(kv);
        }
        free(section);
    }
}

bool config_set(struct config* cfg, const char* section, const char* key,
                const char* value)
{
    struct config_keyval* kv = NULL;
    struct config* cs = NULL;

    if (!value || !*value) {
        fprintf(stderr,
                "WARNING: Empty config value for key \"%s\" in section \"%s\" "
                "is not alowed\n",
                key, section);
        return false;
    }

    // search for config section
    list_for_each(cfg, struct config, it) {
        if (strcmp(section, it->name) == 0) {
            cs = it;
            break;
        }
    }
    if (!cs) {
        fprintf(stderr, "WARNING: Unknown config section \"%s\"\n", section);
        return false;
    }

    // search for existing key/value
    list_for_each(cs->params, struct config_keyval, it) {
        if (strcmp(key, it->key) == 0) {
            kv = it;
            break;
        }
    }

    // check if config key is valid for the current section
    if (strcmp(section, CFG_KEYS_VIEWER) != 0 &&
        strcmp(section, CFG_KEYS_GALLERY) != 0 && !kv) {
        fprintf(stderr,
                "WARNING: Unknown config key \"%s\" in section \"%s\"\n", key,
                section);
        return false;
    }

    // remove existing key/value
    if (kv) {
        cs->params = list_remove(kv);
        free(kv);
    }

    // add new key/value
    kv = create_keyval(key, value);
    if (kv) {
        cs->params = list_add(cs->params, kv);
    }

    return true;
}

bool config_set_arg(struct config* cfg, const char* arg)
{
    char section[32];
    char key[32];
    const char* ptr;
    struct str_slice slices[2];
    size_t size;

    // split section.key and value
    size = str_split(arg, '=', slices, ARRAY_SIZE(slices));
    if (size <= 1) {
        fprintf(stderr, "WARNING: Invalid config argument format: \"%s\"\n",
                arg);
        return false;
    }

    // split section and key
    ptr = slices[0].value + slices[0].len;
    while (*ptr != '.') {
        if (--ptr < arg) {
            fprintf(stderr, "WARNING: Invalid config argument format: \"%s\"\n",
                    arg);
            return false;
        }
    }

    // section name
    size = ptr - slices[0].value;
    if (size > sizeof(section) - 1) {
        size = sizeof(section) - 1;
    }
    memcpy(section, slices[0].value, size);
    section[size] = 0;

    // key name
    ++ptr; // skip dot
    size = slices[0].len - (ptr - slices[0].value);
    if (size > sizeof(key) - 1) {
        size = sizeof(key) - 1;
    }
    memcpy(key, ptr, size);
    key[size] = 0;

    return config_set(cfg, section, key, slices[1].value);
}

const char* config_get_default(const char* section, const char* key)
{
    for (size_t i = 0; i < ARRAY_SIZE(defaults); ++i) {
        const struct config_default* def = &defaults[i];
        if (strcmp(section, def->section) == 0 && strcmp(key, def->key) == 0) {
            return def->value;
        }
    }

    fprintf(
        stderr,
        "WARNING: Default value for key \"%s\" in section \"%s\" not found\n",
        key, section);
    return "";
}

const char* config_get(const struct config* cfg, const char* section,
                       const char* key)
{
    list_for_each(cfg, const struct config, cs) {
        if (strcmp(section, cs->name) == 0) {
            list_for_each(cs->params, const struct config_keyval, kv) {
                if (strcmp(key, kv->key) == 0) {
                    const char* value = kv->value;
                    if (!*kv->value) {
                        value = config_get_default(section, key);
                        fprintf(stderr,
                                "WARNING: "
                                "Empty value for the key \"%s\" in section "
                                "\"%s\" is not allowed, "
                                "the default value \"%s\" will be used\n",
                                key, section, value);
                    }
                    return value;
                }
            }
        }
    }

    fprintf(stderr,
            "WARNING: Value for key \"%s\" in section \"%s\" not found\n", key,
            section);
    return "";
}

ssize_t config_get_oneof(const struct config* cfg, const char* section,
                         const char* key, const char** array, size_t array_sz)
{
    const char* value = config_get(cfg, section, key);
    ssize_t index = str_search_index(array, array_sz, value, 0);

    if (index == -1) {
        fprintf(stderr,
                "WARNING: "
                "Invalid config value \"%s = %s\" in section \"%s\": "
                "expected one of: ",
                key, value, section);
        for (size_t i = 0; i < array_sz; ++i) {
            fprintf(stderr, "%s, ", array[i]);
        }
        value = config_get_default(section, key);
        fprintf(stderr, "the default value \"%s\" will be used\n", value);
        index = str_search_index(array, array_sz, value, 0);
    }

    return index >= 0 ? index : 0;
}

bool config_get_bool(const struct config* cfg, const char* section,
                     const char* key)
{
    bool boolean = false;
    const char* value = config_get(cfg, section, key);

    if (!text_to_bool(value, &boolean)) {
        text_to_bool(config_get_default(section, key), &boolean);
        fprintf(stderr,
                "WARNING: "
                "Invalid config value \"%s = %s\" in section \"%s\": "
                "expected \"" CFG_YES "\" or \"" CFG_NO "\", "
                "the default value \"%s\" will be used\n",
                key, value, section, boolean ? CFG_YES : CFG_NO);
    }

    return boolean;
}

ssize_t config_get_num(const struct config* cfg, const char* section,
                       const char* key, ssize_t min_val, ssize_t max_val)
{
    ssize_t num = 0;
    const char* value = config_get(cfg, section, key);

    if (!str_to_num(value, 0, &num, 0) || num < min_val || num > max_val) {
        str_to_num(config_get_default(section, key), 0, &num, 0);
        fprintf(stderr,
                "WARNING: "
                "Invalid config value \"%s = %s\" in section \"%s\": "
                "expected integer in range %zd-%zd, "
                "the default value %zd will be used\n",
                key, value, section, min_val, max_val, num);
    }

    return num;
}

argb_t config_get_color(const struct config* cfg, const char* section,
                        const char* key)
{
    argb_t color = 0;
    const char* value = config_get(cfg, section, key);

    if (!text_to_color(value, &color)) {
        text_to_color(config_get_default(section, key), &color);
        fprintf(stderr,
                "WARNING: "
                "Invalid color value \"%s = %s\" in section \"%s\": "
                "expected RGB(A) format (e.g. #11223344), "
                "the default value #%08x will be used\n",
                key, value, section, color);
    }

    return color;
}

char* config_expand_path(const char* prefix_env, const char* postfix)
{
    char* path = NULL;

    if (prefix_env) {
        const char* delim;
        size_t prefix_len = 0;
        const char* prefix = getenv(prefix_env);
        if (!prefix || !*prefix) {
            return NULL;
        }
        // use only the first directory if prefix is a list
        delim = strchr(prefix, ':');
        prefix_len = delim ? (size_t)(delim - prefix) : strlen(prefix);
        str_append(prefix, prefix_len, &path);
    }

    str_append(postfix, 0, &path);

    return path;
}

void config_error_key(const char* section, const char* key)
{
    fprintf(stderr, "WARNING: Invalid config key \"%s\" in section \"%s\"\n",
            key, section);
}

void config_error_val(const char* section, const char* value)
{
    fprintf(stderr, "WARNING: Invalid config value \"%s\" in section \"%s\"\n",
            value, section);
}

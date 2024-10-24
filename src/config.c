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
static const struct config_default defaults[] = {
    { "general",      "mode",             "viewer"                                   },
    { "general",      "position",         "parent"                                   },
    { "general",      "size",             "parent"                                   },
    { "general",      "sigusr1",          "reload"                                   },
    { "general",      "sigusr2",          "next_file"                                },
    { "general",      "app_id",           "swayimg"                                  },

    { "viewer",       "window",           "#00000000"                                },
    { "viewer",       "transparency",     "grid"                                     },
    { "viewer",       "scale",            "optimal"                                  },
    { "viewer",       "fixed",            "yes"                                      },
    { "viewer",       "antialiasing",     "no"                                       },
    { "viewer",       "slideshow",        "no"                                       },
    { "viewer",       "slideshow_time",   "3"                                        },
    { "viewer",       "history",          "1"                                        },
    { "viewer",       "preload",          "1"                                        },

    { "gallery",      "size",             "200"                                      },
    { "gallery",      "cache",            "100"                                      },
    { "gallery",      "fill",             "yes"                                      },
    { "gallery",      "antialiasing",     "no"                                       },
    { "gallery",      "window",           "#00000000"                                },
    { "gallery",      "background",       "#202020ff"                                },
    { "gallery",      "select",           "#404040ff"                                },
    { "gallery",      "border",           "#000000ff"                                },
    { "gallery",      "shadow",           "#000000ff"                                },

    { "list",         "order",            "alpha"                                    },
    { "list",         "loop",             "yes"                                      },
    { "list",         "recursive",        "no"                                       },
    { "list",         "all",              "yes"                                      },

    { "font",         "name",             "monospace"                                },
    { "font",         "size",             "14"                                       },
    { "font",         "color",            "#ccccccff"                                },
    { "font",         "shadow",           "#000000a0"                                },

    { "info",         "show",             "yes"                                      },
    { "info",         "info_timeout",     "5"                                        },
    { "info",         "status_timeout",   "3"                                        },

    { "info.viewer",  "top_left",         "+name,+format,+filesize,+imagesize,+exif" },
    { "info.viewer",  "top_right",        "index"                                    },
    { "info.viewer",  "bottom_left",      "scale,frame"                              },
    { "info.viewer",  "bottom_right",     "status"                                   },

    { "info.gallery", "top_left",         "none"                                     },
    { "info.gallery", "top_right",        "none"                                     },
    { "info.gallery", "bottom_left",      "none"                                     },
    { "info.gallery", "bottom_right",     "name,status"                              },

    { "keys.viewer",  "F1",               "help"                                     },
    { "keys.viewer",  "Home",             "first_file"                               },
    { "keys.viewer",  "End",              "last_file"                                },
    { "keys.viewer",  "Prior",            "prev_file"                                },
    { "keys.viewer",  "Next",             "next_file"                                },
    { "keys.viewer",  "Space",            "next_file"                                },
    { "keys.viewer",  "Shift+d",          "prev_dir"                                 },
    { "keys.viewer",  "d",                "next_dir"                                 },
    { "keys.viewer",  "Shift+o",          "prev_frame"                               },
    { "keys.viewer",  "o",                "next_frame"                               },
    { "keys.viewer",  "c",                "skip_file"                                },
    { "keys.viewer",  "Shift+s",          "slideshow"                                },
    { "keys.viewer",  "s",                "animation"                                },
    { "keys.viewer",  "f",                "fullscreen"                               },
    { "keys.viewer",  "Return",           "mode"                                     },
    { "keys.viewer",  "Left",             "step_left 10"                             },
    { "keys.viewer",  "Right",            "step_right 10"                            },
    { "keys.viewer",  "Up",               "step_up 10"                               },
    { "keys.viewer",  "Down",             "step_down 10"                             },
    { "keys.viewer",  "Equal",            "zoom +10"                                 },
    { "keys.viewer",  "Plus",             "zoom +10"                                 },
    { "keys.viewer",  "Minus",            "zoom -10"                                 },
    { "keys.viewer",  "w",                "zoom width"                               },
    { "keys.viewer",  "Shift+w",          "zoom height"                              },
    { "keys.viewer",  "z",                "zoom fit"                                 },
    { "keys.viewer",  "Shift+z",          "zoom fill"                                },
    { "keys.viewer",  "0",                "zoom real"                                },
    { "keys.viewer",  "BackSpace",        "zoom optimal"                             },
    { "keys.viewer",  "bracketleft",      "rotate_left"                              },
    { "keys.viewer",  "bracketright",     "rotate_right"                             },
    { "keys.viewer",  "m",                "flip_vertical"                            },
    { "keys.viewer",  "Shift+m",          "flip_horizontal"                          },
    { "keys.viewer",  "a",                "antialiasing"                             },
    { "keys.viewer",  "r",                "reload"                                   },
    { "keys.viewer",  "i",                "info"                                     },
    { "keys.viewer",  "Shift+Delete",     "exec rm '%'; skip_file"                   },
    { "keys.viewer",  "Escape",           "exit"                                     },
    { "keys.viewer",  "q",                "exit"                                     },
    { "keys.viewer",  "ScrollLeft",       "step_right 5"                             },
    { "keys.viewer",  "ScrollRight",      "step_left 5"                              },
    { "keys.viewer",  "ScrollUp",         "step_up 5"                                },
    { "keys.viewer",  "ScrollDown",       "step_down 5"                              },
    { "keys.viewer",  "Ctrl+ScrollUp",    "zoom +10"                                 },
    { "keys.viewer",  "Ctrl+ScrollDown",  "zoom -10"                                 },
    { "keys.viewer",  "Shift+ScrollUp",   "prev_file"                                },
    { "keys.viewer",  "Shift+ScrollDown", "next_file"                                },
    { "keys.viewer",  "Alt+ScrollUp",     "prev_frame"                               },
    { "keys.viewer",  "Alt+ScrollDown",   "next_frame"                               },

    { "keys.gallery", "F1",               "help"                                     },
    { "keys.gallery", "Home",             "first_file"                               },
    { "keys.gallery", "End",              "last_file"                                },
    { "keys.gallery", "Left",             "step_left"                                },
    { "keys.gallery", "Right",            "step_right"                               },
    { "keys.gallery", "Up",               "step_up"                                  },
    { "keys.gallery", "Down",             "step_down"                                },
    { "keys.gallery", "Prior",            "page_up"                                  },
    { "keys.gallery", "Next",             "page_down"                                },
    { "keys.gallery", "c",                "skip_file"                                },
    { "keys.gallery", "f",                "fullscreen"                               },
    { "keys.gallery", "Return",           "mode"                                     },
    { "keys.gallery", "a",                "antialiasing"                             },
    { "keys.gallery", "r",                "reload"                                   },
    { "keys.gallery", "i",                "info"                                     },
    { "keys.gallery", "Shift+Delete",     "exec rm '%'; skip_file"                   },
    { "keys.gallery", "Escape",           "exit"                                     },
    { "keys.gallery", "q",                "exit"                                     },
    { "keys.gallery", "ScrollLeft",       "step_right"                               },
    { "keys.gallery", "ScrollRight",      "step_left"                                },
    { "keys.gallery", "ScrollUp",         "step_up"                                  },
    { "keys.gallery", "ScrollDown",       "step_down"                                },
};

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
 * Get section entry.
 * @param cfg config instance
 * @param name section name
 * @return pointer to section entry or NULL if not found
 */
static struct config* get_section(struct config* cfg, const char* name)
{
    list_for_each(cfg, struct config, it) {
        if (strcmp(name, it->name) == 0) {
            return it;
        }
    }
    return NULL;
}

char* expand_path(const char* prefix_env, const char* postfix)
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
 * @return loaded config instance or NULL on errors
 */
static bool load(const char* path, struct config** cfg)
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

struct config* config_load(void)
{
    struct config* cfg = NULL;

    // set defaults
    for (size_t i = 0; i < ARRAY_SIZE(defaults); ++i) {
        const struct config_default* def = &defaults[i];
        config_set(&cfg, def->section, def->key, def->value);
    }

    // find and load first available config file
    for (size_t i = 0; i < ARRAY_SIZE(config_locations); ++i) {
        const struct location* cl = &config_locations[i];
        char* path = expand_path(cl->prefix, cl->postfix);
        const bool loaded = path && load(path, &cfg);
        free(path);
        if (loaded) {
            break;
        }
    }

    return cfg;
}

void config_free(struct config* cfg)
{
    // free resources
    list_for_each(cfg, struct config, section) {
        list_for_each(section->params, struct config_keyval, kv) {
            free(kv);
        }
        free(section);
    }
}

void config_check(struct config* cfg)
{
    // sanity checker: all config parameters should be read
    list_for_each(cfg, struct config, section) {
        list_for_each(section->params, struct config_keyval, kv) {
            if (!kv->used) {
                fprintf(stderr,
                        "WARNING: Unknown config parameter \"%s = %s\" in "
                        "section \"%s\"\n",
                        kv->key, kv->value, section->name);
            }
        }
    }
}

void config_set(struct config** cfg, const char* section, const char* key,
                const char* value)
{
    struct config_keyval* kv;
    struct config* cs;

    cs = get_section(*cfg, section);
    if (!cs) {
        // add new section
        const size_t sz = strlen(section) + 1 /*last null*/;
        cs = calloc(1, sizeof(struct config) + sz);
        if (!cs) {
            return;
        }
        cs->name = (char*)cs + sizeof(struct config);
        memcpy(cs->name, section, sz);
        *cfg = list_add(*cfg, cs);
    } else {
        // remove existing entry
        list_for_each(cs->params, struct config_keyval, it) {
            if (strcmp(key, it->key) == 0) {
                cs->params = list_remove(it);
                free(it);
                break;
            }
        }
    }

    kv = create_keyval(key, value);
    if (kv) {
        cs->params = list_add(cs->params, kv);
    }
}

bool config_set_arg(struct config** cfg, const char* arg)
{
    char section[32];
    char key[32];
    const char* ptr;
    struct str_slice slices[2];
    size_t size;

    // split section.key and value
    size = str_split(arg, '=', slices, ARRAY_SIZE(slices));
    if (size <= 1) {
        return false;
    }

    // split section and key
    ptr = slices[0].value + slices[0].len;
    while (*ptr != '.') {
        if (--ptr < arg) {
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

    config_set(cfg, section, key, slices[1].value);

    return true;
}

const char* config_get(struct config* cfg, const char* section, const char* key)
{
    struct config* cs = get_section(cfg, section);

    if (cs) {
        list_for_each(cs->params, struct config_keyval, it) {
            if (strcmp(key, it->key) == 0) {
                it->used = true;
                return it->value;
            }
        }
    }

    return NULL;
}

const char* config_get_string(struct config* cfg, const char* section,
                              const char* key, const char* fallback)
{
    const char* value = config_get(cfg, section, key);
    return value ? value : fallback;
}

bool config_get_bool(struct config* cfg, const char* section, const char* key,
                     bool fallback)
{
    const char* value = config_get(cfg, section, key);

    if (value) {
        if (strcmp(value, "yes") == 0 || strcmp(value, "true") == 0) {
            return true;
        } else if (strcmp(value, "no") == 0 || strcmp(value, "false") == 0) {
            return false;
        } else {
            fprintf(stderr,
                    "WARNING: "
                    "Invalid config value \"%s = %s\" in section \"%s\": "
                    "expected \"yes\" or \"no\"\n",
                    key, value, section);
        }
    }

    return fallback;
}

ssize_t config_get_num(struct config* cfg, const char* section, const char* key,
                       ssize_t min_val, ssize_t max_val, ssize_t fallback)
{
    const char* value = config_get(cfg, section, key);

    if (value) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num >= min_val && num <= max_val) {
            return num;
        } else {
            fprintf(stderr,
                    "WARNING: "
                    "Invalid config value \"%s = %s\" in section \"%s\": "
                    "expected integer in range %zd-%zd\n",
                    key, value, section, min_val, max_val);
        }
    }

    return fallback;
}

argb_t config_get_color(struct config* cfg, const char* section,
                        const char* key, argb_t fallback)
{
    const char* value = config_get(cfg, section, key);

    if (value) {
        char* endptr;
        argb_t color;
        while (*value == '#' || isspace(*value)) {
            ++value;
        }
        errno = 0;
        color = strtoull(value, &endptr, 16);
        if (endptr && !*endptr && errno == 0) {
            if (strlen(value) > 6) { // value with alpha (RRGGBBAA)
                color = (color >> 8) | ARGB_SET_A(color);
            } else {
                color |= ARGB_SET_A(0xff);
            }
            return color;
        } else {
            fprintf(stderr,
                    "WARNING: "
                    "Invalid color value \"%s = %s\" in section \"%s\": "
                    "expected RGB(A) format, e.g. #11223344\n",
                    key, value, section);
        }
    }

    return fallback;
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

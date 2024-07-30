// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "info.h"

#include "application.h"
#include "config.h"
#include "imagelist.h"
#include "loader.h"
#include "str.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

// clang-format off

/** Display modes. */
enum info_mode {
    mode_viewer,
    mode_gallery,
    mode_off,
};
static const char* mode_names[] = {
    [mode_viewer] = INFO_MODE_VIEWER,
    [mode_gallery] = INFO_MODE_GALLERY,
    [mode_off] = INFO_MODE_OFF,
};
#define MODES_NUM 2

// number of types in `enum info_field`
#define INFO_FIELDS_NUM 10

// max number of lines in one positioned block
#define MAX_LINES (INFO_FIELDS_NUM + 10 /* EXIF and duplicates */)

/** Field names. */
static const char* field_names[] = {
    [info_file_name] = "name",
    [info_file_path] = "path",
    [info_file_size] = "filesize",
    [info_image_format] = "format",
    [info_image_size] = "imagesize",
    [info_exif] = "exif",
    [info_frame] = "frame",
    [info_index] = "index",
    [info_scale] = "scale",
    [info_status] = "status",
};

/** Block position names. */
static const char* position_names[] = {
    [text_center] = "center",
    [text_top_left] = "top_left",
    [text_top_right] = "top_right",
    [text_bottom_left] = "bottom_left",
    [text_bottom_right] = "bottom_right",
};
#define INFO_POSITION_NUM ARRAY_SIZE(position_names)

/** Scheme of displayed field (line(s) of text). */
struct field_scheme {
    enum info_field type; ///< Field type
    bool title;           ///< Print/hide field title
};

// Defaults
static const struct field_scheme default_viewer_tl[] = {
    { .type = info_file_name,    .title = true },
    { .type = info_image_format, .title = true },
    { .type = info_file_size,    .title = true },
    { .type = info_image_size,   .title = true },
    { .type = info_exif,         .title = true },
};
static const struct field_scheme default_viewer_tr[] = {
    { .type = info_index, .title = false },
};
static const struct field_scheme default_viewer_bl[] = {
    { .type = info_scale, .title = false },
    { .type = info_frame, .title = false },
};
static const struct field_scheme default_viewer_br[] = {
    { .type = info_status, .title = false },
};
static const struct field_scheme default_gallery_br[] = {
    { .type = info_file_name, .title = false },
    { .type = info_status,    .title = false },
};

// clang-format on

#define SET_DEFAULT(m, p, d)                     \
    ctx.scheme[m][p].fields_num = ARRAY_SIZE(d); \
    ctx.scheme[m][p].fields = malloc(sizeof(d)); \
    memcpy(ctx.scheme[m][p].fields, d, sizeof(d))

/** Info scheme: set of fields in one of screen positions. */
struct block_scheme {
    struct field_scheme* fields; ///< Array of fields
    size_t fields_num;           ///< Size of array
};

/** Info timeout description. */
struct info_timeout {
    int fd;          ///< Timer FD
    size_t duration; ///< Timeout duration in seconds
    bool active;     ///< Current state
};

/** Info data context. */
struct info_context {
    enum info_mode mode; ///< Currently active mode

    struct info_timeout info;   ///< Text info timeout
    struct info_timeout status; ///< Status message timeout

    struct text_keyval* exif_lines; ///< EXIF data lines
    size_t exif_num;                ///< Number of lines in EXIF data

    struct text_keyval fields[INFO_FIELDS_NUM];               ///< Info data
    struct block_scheme scheme[MODES_NUM][INFO_POSITION_NUM]; ///< Info scheme
};

/** Global info context. */
static struct info_context ctx;

/** Notification callback: handle timer event. */
static void on_timeout(void* data)
{
    struct info_timeout* timeout = data;
    struct itimerspec ts = { 0 };

    timeout->active = false;
    timerfd_settime(timeout->fd, 0, &ts, NULL);
    app_redraw();
}

/**
 * Initialize timer.
 * @param timeout timer instance
 */
static void timeout_init(struct info_timeout* timeout)
{
    timeout->fd = -1;
    timeout->active = true;
    if (timeout->duration != 0) {
        timeout->fd =
            timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
        if (timeout->fd != -1) {
            app_watch(timeout->fd, on_timeout, timeout);
        }
    }
}

/**
 * Reset/restart timer.
 * @param timeout timer instance
 */
static void timeout_reset(struct info_timeout* timeout)
{
    timeout->active = true;
    if (timeout->fd != -1) {
        struct itimerspec ts = { .it_value.tv_sec = timeout->duration };
        timerfd_settime(timeout->fd, 0, &ts, NULL);
    }
}

/**
 * Close timer FD.
 * @param timeout timer instance
 */
static void timeout_close(struct info_timeout* timeout)
{
    if (timeout->fd != -1) {
        close(timeout->fd);
    }
}

/**
 * Import meta data from image.
 * @param image source image
 */
static void import_exif(const struct image* image)
{
    struct text_keyval* line;
    const size_t buf_size = image->num_info * sizeof(*line);

    // free previuos lines
    for (size_t i = 0; i < ctx.exif_num; ++i) {
        free(ctx.exif_lines[i].key.data);
        free(ctx.exif_lines[i].value.data);
    }
    ctx.exif_num = 0;

    if (image->num_info == 0) {
        return;
    }

    line = realloc(ctx.exif_lines, buf_size);
    if (!line) {
        return;
    }
    memset(line, 0, buf_size);

    ctx.exif_num = image->num_info;
    ctx.exif_lines = line;

    for (size_t i = 0; i < ctx.exif_num; ++i) {
        const struct image_info* exif = &image->info[i];
        char key[64];
        snprintf(key, sizeof(key), "%s:", exif->key);
        font_render(key, &line->key);
        font_render(exif->value, &line->value);
        ++line;
    }
}

/**
 * Parse and load scheme from config line.
 * @param config line to parse
 * @param scheme destination scheme description
 * @return true if config parsed successfully
 */
static bool parse_scheme(const char* config, struct block_scheme* scheme)
{
    struct str_slice slices[MAX_LINES];
    size_t slices_num;
    struct field_scheme* fields;

    // split into fields slices
    slices_num = str_split(config, ',', slices, ARRAY_SIZE(slices));
    if (slices_num > ARRAY_SIZE(slices)) {
        slices_num = ARRAY_SIZE(slices);
    }

    fields = realloc(scheme->fields, slices_num * sizeof(*fields));
    if (!fields) {
        return false;
    }
    scheme->fields = fields;
    scheme->fields_num = 0;

    for (size_t i = 0; i < slices_num; ++i) {
        struct field_scheme* field = &scheme->fields[scheme->fields_num];
        ssize_t field_idx;
        struct str_slice* sl = &slices[i];

        // title show/hide ('+' at the beginning)
        field->title = (sl->len > 0 && *sl->value == '+');
        if (field->title) {
            ++sl->value;
            --sl->len;
        }

        // field type
        field_idx = str_index(field_names, sl->value, sl->len);
        if (field_idx >= 0) {
            field->type = field_idx;
            scheme->fields_num++;
        } else if (sl->len == 4 && strncmp(sl->value, "none", sl->len) == 0) {
            continue; // special value, just skip
        } else {
            return false; // invalid field name
        }
    }

    if (scheme->fields_num == 0) {
        free(scheme->fields);
        scheme->fields = NULL;
    }

    return true;
}

/** Custom section loader, see `config_loader` for details. */
static enum config_status load_config_viewer(const char* key, const char* value)
{
    const ssize_t pos = str_index(position_names, key, 0);
    if (pos < 0) {
        return cfgst_invalid_key;
    }
    if (!parse_scheme(value, &ctx.scheme[mode_viewer][pos])) {
        return cfgst_invalid_value;
    }
    return cfgst_ok;
}

/** Custom section loader, see `config_loader` for details. */
static enum config_status load_config_gallery(const char* key,
                                              const char* value)
{
    const ssize_t pos = str_index(position_names, key, 0);
    if (pos < 0) {
        return cfgst_invalid_key;
    }
    if (!parse_scheme(value, &ctx.scheme[mode_gallery][pos])) {
        return cfgst_invalid_value;
    }
    return cfgst_ok;
}

/** Custom section loader, see `config_loader` for details. */
static enum config_status load_config_common(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, "show") == 0) {
        bool opt;
        if (config_to_bool(value, &opt)) {
            ctx.mode = mode_off;
            status = cfgst_ok;
        }
    } else if (strcmp(key, "info_timeout") == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num >= 0 && num < 1024) {
            ctx.info.duration = num;
            status = cfgst_ok;
        }
    } else if (strcmp(key, "status_timeout") == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num >= 0 && num < 1024) {
            ctx.status.duration = num;
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void info_create(void)
{
    // set defaults
    ctx.info.duration = 5;
    ctx.status.duration = 3;
    ctx.mode = mode_viewer;
    SET_DEFAULT(mode_viewer, text_top_left, default_viewer_tl);
    SET_DEFAULT(mode_viewer, text_top_right, default_viewer_tr);
    SET_DEFAULT(mode_viewer, text_bottom_left, default_viewer_bl);
    SET_DEFAULT(mode_viewer, text_bottom_right, default_viewer_br);
    SET_DEFAULT(mode_gallery, text_bottom_right, default_gallery_br);

    // register configuration loader
    config_add_loader("info", load_config_common);
    config_add_loader("info.viewer", load_config_viewer);
    config_add_loader("info.gallery", load_config_gallery);
}

void info_init(void)
{
    font_render("File name:", &ctx.fields[info_file_name].key);
    font_render("File path:", &ctx.fields[info_file_path].key);
    font_render("File size:", &ctx.fields[info_file_size].key);
    font_render("Image format:", &ctx.fields[info_image_format].key);
    font_render("Image size:", &ctx.fields[info_image_size].key);
    font_render("Frame:", &ctx.fields[info_frame].key);
    font_render("Index:", &ctx.fields[info_index].key);
    font_render("Scale:", &ctx.fields[info_scale].key);
    font_render("Status:", &ctx.fields[info_status].key);

    timeout_init(&ctx.info);
    timeout_init(&ctx.status);
}

void info_destroy(void)
{
    timeout_close(&ctx.info);
    timeout_close(&ctx.status);

    for (size_t i = 0; i < ctx.exif_num; ++i) {
        free(ctx.exif_lines[i].key.data);
        free(ctx.exif_lines[i].value.data);
    }

    for (size_t i = 0; i < MODES_NUM; ++i) {
        for (size_t j = 0; j < INFO_POSITION_NUM; ++j) {
            free(ctx.scheme[i][j].fields);
        }
    }

    for (size_t i = 0; i < INFO_FIELDS_NUM; ++i) {
        free(ctx.fields[i].key.data);
        free(ctx.fields[i].value.data);
    }
}

void info_switch(const char* mode)
{
    if (!ctx.info.active) {
        timeout_reset(&ctx.info);
        return;
    }

    if (mode && *mode) {
        const ssize_t mode_num = str_index(mode_names, mode, 0);
        if (mode_num >= 0) {
            ctx.mode = mode_num;
            return;
        }
    } else {
        ++ctx.mode;
        if (ctx.mode > mode_off) {
            ctx.mode = 0;
        }
    }

    timeout_reset(&ctx.info);
}

bool info_enabled(void)
{
    return (ctx.mode != mode_off);
}

void info_reset(const struct image* image)
{
    const size_t mib = 1024 * 1024;
    const char unit = image->file_size >= mib ? 'M' : 'K';
    const float sz =
        (float)image->file_size / (image->file_size >= mib ? mib : 1024);

    font_render(image->name, &ctx.fields[info_file_name].value);
    font_render(image->source, &ctx.fields[info_file_path].value);
    font_render(image->format, &ctx.fields[info_image_format].value);

    info_update(info_file_size, "%.02f %ciB", sz, unit);
    info_update(info_image_size, "%zux%zu", image->frames[0].pm.width,
                image->frames[0].pm.height);

    import_exif(image);

    info_update(info_frame, NULL);
    info_update(info_scale, NULL);

    timeout_reset(&ctx.info);
}

void info_update(enum info_field field, const char* fmt, ...)
{
    struct text_surface* surface = &ctx.fields[field].value;
    va_list args;
    int len;
    char* text;

    if (!fmt) {
        free(surface->data);
        memset(surface, 0, sizeof(*surface));
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }
    text = malloc(len + 1 /* last null */);
    if (!text) {
        return;
    }
    va_start(args, fmt);
    vsprintf(text, fmt, args);
    va_end(args);

    font_render(text, surface);

    free(text);

    if (field == info_status) {
        timeout_reset(&ctx.status);
    }
}

void info_print(struct pixmap* window)
{
    if (ctx.mode == mode_off || !ctx.info.active) {
        // print only status
        if (ctx.fields[info_status].value.width && ctx.status.active) {
            for (size_t i = 0; i < INFO_POSITION_NUM; ++i) {
                const struct block_scheme* block = &ctx.scheme[ctx.mode][i];
                for (size_t j = 0; j < block->fields_num; ++j) {
                    const struct field_scheme* field = &block->fields[j];
                    if (field->type == info_status) {
                        struct text_keyval status = ctx.fields[info_status];
                        if (!field->title) {
                            memset(&status.key, 0, sizeof(status.key));
                        }
                        text_print_keyval(window, i, &status, 1);
                        break;
                    }
                }
            }
        }
        return;
    }

    for (size_t i = 0; i < INFO_POSITION_NUM; ++i) {
        struct text_keyval lines[MAX_LINES] = { 0 };
        const struct block_scheme* block = &ctx.scheme[ctx.mode][i];
        size_t lnum = 0;

        for (size_t j = 0; j < block->fields_num; ++j) {
            const struct field_scheme* field = &block->fields[j];
            const struct text_keyval* origin = &ctx.fields[field->type];

            switch (field->type) {
                case info_exif:
                    for (size_t n = 0; n < ctx.exif_num; ++n) {
                        if (lnum < ARRAY_SIZE(lines)) {
                            if (field->title) {
                                lines[lnum].key = ctx.exif_lines[n].key;
                            }
                            lines[lnum++].value = ctx.exif_lines[n].value;
                        }
                    }
                    break;
                case info_status:
                    if (origin->value.width && ctx.status.active) {
                        if (field->title) {
                            lines[lnum].key = origin->key;
                        }
                        lines[lnum++].value = origin->value;
                    }
                    break;
                default:
                    if (origin->value.width) {
                        if (field->title) {
                            lines[lnum].key = origin->key;
                        }
                        lines[lnum++].value = origin->value;
                    }
                    break;
            }
            if (lnum >= ARRAY_SIZE(lines)) {
                break;
            }
        }

        if (lnum) {
            text_print_keyval(window, i, lines, lnum);
        }
    }
}

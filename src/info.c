// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "info.h"

#include "application.h"
#include "array.h"
#include "font.h"
#include "imglist.h"
#include "keybind.h"
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

/** Limit on the length of the meta info key/value */
#define MAX_META_KEY_LEN   32
#define MAX_META_VALUE_LEN 128

/** Display modes. */
enum info_mode {
    mode_viewer,
    mode_gallery,
    mode_off,
};
static const char* mode_names[] = {
    [mode_viewer] = CFG_MODE_VIEWER,
    [mode_gallery] = CFG_MODE_GALLERY,
    [mode_off] = "off",
};
#define MODES_NUM 2

// clang-format off
/** Field names. */
static const char* field_names[] = {
    [info_file_name] = "name",
    [info_file_dir] = "dir",
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
#define FIELDS_NUM ARRAY_SIZE(field_names)

/** Positions of text info block. */
enum block_position {
    pos_center,
    pos_top_left,
    pos_top_right,
    pos_bottom_left,
    pos_bottom_right,
};
/** Block position names. */
static const char* position_names[] = {
    [pos_center] = CFG_INFO_CN,
    [pos_top_left] = CFG_INFO_TL,
    [pos_top_right] = CFG_INFO_TR,
    [pos_bottom_left] = CFG_INFO_BL,
    [pos_bottom_right] = CFG_INFO_BR,
};
#define POSITION_NUM ARRAY_SIZE(position_names)
// clang-format on

// Max number of lines in one positioned block
#define MAX_LINES (FIELDS_NUM + 10 /* EXIF and duplicates */)

// Space between text layout and window edge
#define TEXT_PADDING 10

/** Scheme of displayed field (line(s) of text). */
struct field_scheme {
    enum info_field type; ///< Field type
    bool title;           ///< Print/hide field title
};

/** Key/value text surface. */
struct keyval {
    struct text_surface key;
    struct text_surface value;
};

/** Info scheme: set of fields in one of screen positions. */
struct block_scheme {
    struct field_scheme* fields; ///< Array of fields
    size_t fields_num;           ///< Size of array
};

/** Info timeout description. */
struct info_timeout {
    int fd;         ///< Timer FD
    size_t timeout; ///< Timeout duration in seconds
    bool active;    ///< Current state
};

/** Info data context. */
struct info_context {
    enum info_mode mode; ///< Currently active mode

    struct info_timeout info;   ///< Text info timeout
    struct info_timeout status; ///< Status message timeout

    struct array* help; ///< Help layer lines
    struct array* meta; ///< Image meta data (EXIF etc)

    struct keyval fields[FIELDS_NUM];                    ///< Info data
    struct block_scheme scheme[MODES_NUM][POSITION_NUM]; ///< Info scheme
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
    if (timeout->timeout != 0) {
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
        struct itimerspec ts = { .it_value.tv_sec = timeout->timeout };
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

/** Free help buffer. */
static void free_help(void)
{
    if (ctx.help) {
        for (size_t i = 0; i < ctx.help->size; ++i) {
            struct text_surface* line = arr_nth(ctx.help, i);
            free(line->data);
        }
        arr_free(ctx.help);
        ctx.help = NULL;
    }
}

/** Free image meta info buffer. */
static void free_meta(void)
{
    if (ctx.meta) {
        for (size_t i = 0; i < ctx.meta->size; ++i) {
            struct keyval* kv = arr_nth(ctx.meta, i);
            free(kv->key.data);
            free(kv->value.data);
        }
        arr_free(ctx.meta);
        ctx.meta = NULL;
    }
}

/**
 * Print centered text block.
 * @param wnd destination window
 */
static void print_help(struct pixmap* window)
{
    const size_t line_height =
        ((struct text_surface*)arr_nth(ctx.help, 0))->height;
    const size_t row_max = (window->height - TEXT_PADDING * 2) / line_height;
    const size_t columns =
        (ctx.help->size / row_max) + (ctx.help->size % row_max ? 1 : 0);
    const size_t rows =
        (ctx.help->size / columns) + (ctx.help->size % columns ? 1 : 0);
    const size_t col_space = line_height;
    size_t total_width = 0;
    size_t top = 0;
    size_t left = 0;

    // calculate total width
    for (size_t col = 0; col < columns; ++col) {
        size_t max_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            struct text_surface* line = arr_nth(ctx.help, row + col * rows);
            if (!line) {
                break;
            }
            if (max_width < line->width) {
                max_width = line->width;
            }
        }
        total_width += max_width;
    }
    total_width += col_space * (columns - 1);

    // top left corner of the centered text block
    if (total_width < ui_get_width()) {
        left = window->width / 2 - total_width / 2;
    }
    if (rows * line_height < ui_get_height()) {
        top = window->height / 2 - (rows * line_height) / 2;
    }

    // put text on window
    for (size_t col = 0; col < columns; ++col) {
        size_t y = top;
        size_t col_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            struct text_surface* line = arr_nth(ctx.help, row + col * rows);
            if (!line) {
                break;
            }
            font_print(window, left, y, line);
            if (col_width < line->width) {
                col_width = line->width;
            }
            y += line_height;
        }
        left += col_width + col_space;
    }
}

/**
 * Print info block with key/value text.
 * @param wnd destination window
 * @param pos block position
 * @param lines array of key/value lines to print
 * @param lines_num total number of lines
 */
static void print_keyval(struct pixmap* wnd, enum block_position pos,
                         const struct keyval* lines, size_t lines_num)
{
    size_t max_key_width = 0;
    const size_t height = lines[0].value.height;

    // calc max width of keys, used if block on the left side
    for (size_t i = 0; i < lines_num; ++i) {
        if (lines[i].key.width > max_key_width) {
            max_key_width = lines[i].key.width;
        }
    }
    max_key_width += height / 2;

    // draw info block
    for (size_t i = 0; i < lines_num; ++i) {
        const struct text_surface* key = &lines[i].key;
        const struct text_surface* value = &lines[i].value;
        size_t y = 0;
        size_t x_key = 0;
        size_t x_val = 0;

        // calculate line position
        switch (pos) {
            case pos_center:
                return; // not supported (not used anywhere)
            case pos_top_left:
                y = TEXT_PADDING + i * height;
                if (key->data) {
                    x_key = TEXT_PADDING;
                    x_val = TEXT_PADDING + max_key_width;
                } else {
                    x_val = TEXT_PADDING;
                }
                break;
            case pos_top_right:
                y = TEXT_PADDING + i * height;
                x_val = wnd->width - TEXT_PADDING - value->width;
                if (key->data) {
                    x_key = x_val - key->width - TEXT_PADDING;
                }
                break;
            case pos_bottom_left:
                y = wnd->height - TEXT_PADDING - height * lines_num +
                    i * height;
                if (key->data) {
                    x_key = TEXT_PADDING;
                    x_val = TEXT_PADDING + max_key_width;
                } else {
                    x_val = TEXT_PADDING;
                }
                break;
            case pos_bottom_right:
                y = wnd->height - TEXT_PADDING - height * lines_num +
                    i * height;
                x_val = wnd->width - TEXT_PADDING - value->width;
                if (key->data) {
                    x_key = x_val - key->width - TEXT_PADDING;
                }
                break;
        }

        if (key->data) {
            font_print(wnd, x_key, y, key);
        }
        font_print(wnd, x_val, y, value);
    }
}

/**
 * Import meta data from image.
 * @param image source image
 */
static void import_meta(const struct image* img)
{
    const struct array* info = img->data ? img->data->info : NULL;

    free_meta();

    if (!info) {
        return;
    }

    ctx.meta = arr_create(info->size, sizeof(struct keyval));
    if (!ctx.meta) {
        return;
    }

    for (size_t i = 0; i < info->size; ++i) {
        const struct imginfo* md = arr_nth((struct array*)info, i);
        struct keyval* kv = arr_nth(ctx.meta, i);
        char key[MAX_META_KEY_LEN];
        char value[MAX_META_VALUE_LEN];
        size_t len;

        // limit key
        len = strlen(md->key);
        if (len > sizeof(key) - 2 /* : and last null */) {
            len = sizeof(key) - 2;
        }
        memcpy(key, md->key, len);
        key[len] = ':';
        key[len + 1] = 0;

        // limit value
        len = strlen(md->value) + 1 /* last null */;
        if (len <= sizeof(value)) {
            memcpy(value, md->value, len);
        } else {
            const char elipsis[] = "...";
            len = sizeof(value) - sizeof(elipsis);
            memcpy(value, md->value, len);
            memcpy(value + len, elipsis, sizeof(elipsis));
        }

        font_render(key, &kv->key);
        font_render(value, &kv->value);
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

void info_init(const struct config* cfg)
{
    for (size_t i = 0; i < MODES_NUM; ++i) {
        const char* section;
        section = (i == mode_viewer ? CFG_INFO_VIEWER : CFG_INFO_GALLERY);
        for (size_t j = 0; j < POSITION_NUM; ++j) {
            const char* position = position_names[j];
            const char* format = config_get(cfg, section, position);
            if (!parse_scheme(format, &ctx.scheme[i][j])) {
                config_error_val(section, format);
                format = config_get_default(section, position);
                parse_scheme(format, &ctx.scheme[i][j]);
            }
        }
    }

    ctx.mode =
        config_get_bool(cfg, CFG_INFO, CFG_INFO_SHOW) ? mode_viewer : mode_off;
    ctx.info.timeout =
        config_get_num(cfg, CFG_INFO, CFG_INFO_ITIMEOUT, 0, 1024);
    timeout_init(&ctx.info);

    ctx.status.timeout =
        config_get_num(cfg, CFG_INFO, CFG_INFO_STIMEOUT, 0, 1024);
    timeout_init(&ctx.status);

    info_reinit();
}

void info_reinit(void)
{
    font_render("File name:", &ctx.fields[info_file_name].key);
    font_render("Directory:", &ctx.fields[info_file_dir].key);
    font_render("File path:", &ctx.fields[info_file_path].key);
    font_render("File size:", &ctx.fields[info_file_size].key);
    font_render("Image format:", &ctx.fields[info_image_format].key);
    font_render("Image size:", &ctx.fields[info_image_size].key);
    font_render("Frame:", &ctx.fields[info_frame].key);
    font_render("Index:", &ctx.fields[info_index].key);
    font_render("Scale:", &ctx.fields[info_scale].key);
    font_render("Status:", &ctx.fields[info_status].key);
}

void info_destroy(void)
{
    timeout_close(&ctx.info);
    timeout_close(&ctx.status);

    free_help();
    free_meta();

    for (size_t i = 0; i < MODES_NUM; ++i) {
        for (size_t j = 0; j < POSITION_NUM; ++j) {
            free(ctx.scheme[i][j].fields);
        }
    }

    for (size_t i = 0; i < FIELDS_NUM; ++i) {
        free(ctx.fields[i].key.data);
        free(ctx.fields[i].value.data);
    }
}

void info_switch(const char* mode)
{
    timeout_reset(&ctx.info);

    if (!ctx.info.active) {
        return;
    }
    if (mode && *mode) {
        const ssize_t mode_num = str_index(mode_names, mode, 0);
        if (mode_num >= 0) {
            ctx.mode = mode_num;
        }
    } else {
        ++ctx.mode;
        if (ctx.mode > mode_off) {
            ctx.mode = mode_viewer;
        }
    }
}

void info_switch_help(void)
{
    if (ctx.help) {
        free_help(); // switch help off, free resources
    } else {
        // get number of bindings
        size_t num = 0;
        list_for_each(keybind_get(), struct keybind, it) {
            if (it->help) {
                ++num;
            }
        }
        if (num == 0) {
            return;
        }

        // create help layer in reverse order
        ctx.help = arr_create(num, sizeof(struct text_surface));
        if (!ctx.help) {
            return;
        }
        list_for_each(keybind_get(), struct keybind, it) {
            if (it->help) {
                struct text_surface* line = arr_nth(ctx.help, --num);
                font_render(it->help, line);
            }
        }
    }
}

bool info_help_active(void)
{
    return ctx.help;
}

bool info_enabled(void)
{
    return (ctx.mode != mode_off);
}

void info_reset(const struct image* img)
{
    const size_t mib = 1024 * 1024;
    const char unit = img->file_size >= mib ? 'M' : 'K';
    const float sz =
        (float)img->file_size / (img->file_size >= mib ? mib : 1024);
    const size_t list_size = imglist_size();

    info_update(info_file_name, "%s", img->name);
    info_update(info_file_path, "%s", img->source);
    if (img->data) {
        info_update(info_file_dir, "%s", img->data->parent);
        info_update(info_image_format, "%s", img->data->format);
    } else {
        info_update(info_file_dir, NULL);
        info_update(info_image_format, NULL);
    }

    info_update(info_file_size, "%.02f %ciB", sz, unit);

    if (img->data && img->data->frames) {
        struct imgframe* frame = arr_nth(img->data->frames, 0);
        info_update(info_image_size, "%zux%zu", frame->pm.width,
                    frame->pm.height);
    } else {
        info_update(info_image_size, NULL);
    }

    if (list_size > 1) {
        info_update(info_index, "%zu of %zu", img->index, list_size);
    }

    import_meta(img);

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
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
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
    if (info_help_active()) {
        print_help(window);
    }

    if (ctx.mode == mode_off || !ctx.info.active) {
        // print only status
        if (ctx.fields[info_status].value.width && ctx.status.active) {
            const size_t btype = app_is_viewer() ? mode_viewer : mode_gallery;
            for (size_t i = 0; i < POSITION_NUM; ++i) {
                const struct block_scheme* block = &ctx.scheme[btype][i];
                for (size_t j = 0; j < block->fields_num; ++j) {
                    const struct field_scheme* field = &block->fields[j];
                    if (field->type == info_status) {
                        struct keyval status = ctx.fields[info_status];
                        if (!field->title) {
                            memset(&status.key, 0, sizeof(status.key));
                        }
                        print_keyval(window, i, &status, 1);
                        break;
                    }
                }
            }
        }
        return;
    }

    for (size_t i = 0; i < POSITION_NUM; ++i) {
        struct keyval lines[MAX_LINES] = { 0 };
        const struct block_scheme* block = &ctx.scheme[ctx.mode][i];
        size_t lnum = 0;

        for (size_t j = 0; j < block->fields_num; ++j) {
            const struct field_scheme* field = &block->fields[j];
            const struct keyval* origin = &ctx.fields[field->type];

            switch (field->type) {
                case info_exif:
                    if (ctx.meta) {
                        for (size_t n = 0; n < ctx.meta->size; ++n) {
                            if (lnum < ARRAY_SIZE(lines)) {
                                struct keyval* kv = arr_nth(ctx.meta, n);
                                if (field->title) {
                                    lines[lnum].key = kv->key;
                                }
                                lines[lnum++].value = kv->value;
                            }
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
            print_keyval(window, i, lines, lnum);
        }
    }
}

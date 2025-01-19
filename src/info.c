// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "info.h"

#include "application.h"
#include "font.h"
#include "keybind.h"
#include "loader.h"
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

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
// clang-format on

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
    [pos_center] = CFG_INFO_CN,       [pos_top_left] = CFG_INFO_TL,
    [pos_top_right] = CFG_INFO_TR,    [pos_bottom_left] = CFG_INFO_BL,
    [pos_bottom_right] = CFG_INFO_BR,
};
#define POSITION_NUM ARRAY_SIZE(position_names)

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

    struct text_surface* help; ///< Help layer lines
    size_t help_num;           ///< Number of lines in help

    struct keyval* exif_lines; ///< EXIF data lines
    size_t exif_num;           ///< Number of lines in EXIF data

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

/**
 * Print centered text block.
 * @param wnd destination window
 */
static void print_help(struct pixmap* window)
{
    const size_t line_height = ctx.help[0].height;
    const size_t row_max = (window->height - TEXT_PADDING * 2) / line_height;
    const size_t columns =
        (ctx.help_num / row_max) + (ctx.help_num % row_max ? 1 : 0);
    const size_t rows =
        (ctx.help_num / columns) + (ctx.help_num % columns ? 1 : 0);
    const size_t col_space = line_height;
    size_t total_width = 0;
    size_t top = 0;
    size_t left = 0;

    // calculate total width
    for (size_t col = 0; col < columns; ++col) {
        size_t max_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            const size_t index = row + col * rows;
            if (index >= ctx.help_num) {
                break;
            }
            if (max_width < ctx.help[index].width) {
                max_width = ctx.help[index].width;
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
            const size_t index = row + col * rows;
            if (index >= ctx.help_num) {
                break;
            }
            font_print(window, left, y, &ctx.help[index]);
            if (col_width < ctx.help[index].width) {
                col_width = ctx.help[index].width;
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
static void import_exif(const struct image* image)
{
    struct keyval* lines;
    const size_t num_entries = list_size(&image->info->list);
    const size_t buf_size = num_entries * sizeof(*lines);

    // free previous lines
    for (size_t i = 0; i < ctx.exif_num; ++i) {
        free(ctx.exif_lines[i].key.data);
        free(ctx.exif_lines[i].value.data);
    }
    ctx.exif_num = 0;

    if (num_entries == 0) {
        return;
    }

    lines = realloc(ctx.exif_lines, buf_size);
    if (!lines) {
        return;
    }
    memset(lines, 0, buf_size);

    ctx.exif_num = num_entries;
    ctx.exif_lines = lines;

    list_for_each(image->info, const struct image_info, it) {
        char key[64];
        snprintf(key, sizeof(key), "%s:", it->key);
        font_render(key, &lines->key);
        font_render(it->value, &lines->value);
        ++lines;
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

    font_render("File name:", &ctx.fields[info_file_name].key);
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

    for (size_t i = 0; i < ctx.exif_num; ++i) {
        free(ctx.exif_lines[i].key.data);
        free(ctx.exif_lines[i].value.data);
    }

    for (size_t i = 0; i < MODES_NUM; ++i) {
        for (size_t j = 0; j < POSITION_NUM; ++j) {
            free(ctx.scheme[i][j].fields);
        }
    }

    for (size_t i = 0; i < FIELDS_NUM; ++i) {
        free(ctx.fields[i].key.data);
        free(ctx.fields[i].value.data);
    }

    for (size_t i = 0; i < ctx.help_num; i++) {
        free(ctx.help[i].data);
    }
    free(ctx.help);
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
        for (size_t i = 0; i < ctx.help_num; i++) {
            free(ctx.help[i].data);
        }
        free(ctx.help);
        ctx.help = NULL;
        ctx.help_num = 0;
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
        ctx.help = calloc(1, num * sizeof(*ctx.help));
        if (!ctx.help) {
            return;
        }
        ctx.help_num = num;
        list_for_each(keybind_get(), struct keybind, it) {
            if (it->help) {
                font_render(it->help, &ctx.help[--num]);
            }
        }
    }
}

bool info_help_active(void)
{
    return !!ctx.help_num;
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
            print_keyval(window, i, lines, lnum);
        }
    }
}

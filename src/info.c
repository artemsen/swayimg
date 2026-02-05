// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "info.h"

#include "application.h"
#include "fdpoll.h"
#include "font.h"
#include "exiftool.h"
#include "ui/ui.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Limit on the length of the meta info key/value */
#define MAX_META_KEY_LEN   32
#define MAX_META_VALUE_LEN 128

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
    [info_exiftool] = "exiftool",
    [info_frame] = "frame",
    [info_index] = "index",
    [info_scale] = "scale",
    [info_status] = "status",
};

/** Positions of text info block. */
enum position {
    pos_top_left,
    pos_top_right,
    pos_bottom_left,
    pos_bottom_right,
};

/** Block position names. */
static const char* position_names[] = {
    [pos_top_left] = CFG_INFO_TL,
    [pos_top_right] = CFG_INFO_TR,
    [pos_bottom_left] = CFG_INFO_BL,
    [pos_bottom_right] = CFG_INFO_BR,
};

// clang-format on

// Max number of lines in one positioned block
#define MAX_LINES (ARRAY_SIZE(field_names) + 10 /* EXIF and duplicates */)

/** Key/value text surface. */
struct keyval {
    struct text_surface key;
    struct text_surface value;
};

/** Single field of the scheme. */
struct field {
    enum info_field type; ///< Field type
    bool title;           ///< Print/hide field title
};

/** Info scheme: set of fields in each corners. */
struct scheme {
    struct scheme* next;     ///< Pointer to the next scheme
    struct array* fields[4]; ///< Fields in each corners
    char name[1];            ///< Scheme name (viewer/gallery/...)
};

/** Info timeout description. */
struct timeout {
    int fd;       ///< Timer FD
    size_t delay; ///< Timeout duration in ms
    bool active;  ///< Current state
};

/** Info data context. */
struct info_context {
    bool show;      ///< Show/hide flag
    size_t padding; ///< Text padding

    struct timeout info;   ///< Text info timeout
    struct timeout status; ///< Status message timeout

    struct scheme* schemes; ///< List of available schemes
    struct scheme* current; ///< Currently active scheme

    struct array* help; ///< Help layer lines
    const char* exiftool_args; ///< User arg string for exiftool
    struct array* meta; ///< Image meta data (EXIF etc)
    struct keyval fields[ARRAY_SIZE(field_names)]; ///< Info data
};

/** Global info context. */
static struct info_context ctx;

/** Notification callback: handle timer event. */
static void on_timeout(void* data)
{
    struct timeout* timeout = data;
    fdtimer_reset(timeout->fd, 0, 0);
    timeout->active = false;
    app_redraw();
}

/**
 * Initialize timer.
 * @param timeout timer instance
 */
static void timeout_init(struct timeout* timeout)
{
    timeout->active = true;
    timeout->fd = fdtimer_add(on_timeout, timeout);
}

/**
 * Reset/restart timer.
 * @param timeout timer instance
 */
static void timeout_reset(struct timeout* timeout)
{
    timeout->active = true;
    fdtimer_reset(timeout->fd, timeout->delay, 0);
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
    const size_t row_max = (window->height - ctx.padding * 2) / line_height;
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
static void print_keyval(struct pixmap* wnd, enum position pos,
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
            case pos_top_left:
                y = ctx.padding + i * height;
                if (key->data) {
                    x_key = ctx.padding;
                    x_val = ctx.padding + max_key_width;
                } else {
                    x_val = ctx.padding;
                }
                break;
            case pos_top_right:
                y = ctx.padding + i * height;
                x_val = wnd->width - ctx.padding - value->width;
                if (key->data) {
                    x_key = x_val - key->width - ctx.padding;
                }
                break;
            case pos_bottom_left:
                y = wnd->height - ctx.padding - height * lines_num + i * height;
                if (key->data) {
                    x_key = ctx.padding;
                    x_val = ctx.padding + max_key_width;
                } else {
                    x_val = ctx.padding;
                }
                break;
            case pos_bottom_right:
                y = wnd->height - ctx.padding - height * lines_num + i * height;
                x_val = wnd->width - ctx.padding - value->width;
                if (key->data) {
                    x_key = x_val - key->width - ctx.padding;
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
static void import_meta(const struct image* img);


/**
 * Ensure the current image has the exiftool data loaded if the user wants it.
 * @param img source image or NULL, if called to update the last seen image
 * @return img or the last img if metadata needs re-rendering to updated img exif
 */
static const struct image* ensure_correct_exif(const struct image* img)
{
    // For calls from print_block when we first realize the user actually wants exif
    static const struct image* last;
    // Ensure exif is from exiftool (slow).
    // Gets enabled by print_block that runs just after it
    // if exif is in one of the sections to be printed.
    // -> Always disable, so that if no exif section is printed,
    // we don't waste time on the lookup either.
    // 0 = no, 1 = yes, 2 = load now and disable (called from init)
    static uint8_t should_load_exif = 0;

    if (img) {
        last = img;
        should_load_exif = should_load_exif ? 2 : 0;
    } else { // call from print_block (image is not available there)
        img = last;
        should_load_exif = 1;
    }

    // Attempt to load exif if it is to be shown and hasn't been loaded yet
    if (should_load_exif && *ctx.exiftool_args && img->data && !img->data->used_exiftool) {
        // TODO: make the loading async
        query_exiftool(img, ctx.exiftool_args);

        should_load_exif = should_load_exif == 2 ? 0 : 1;
        return img;
    }
    return NULL;
}

/**
 * Print info block with key/value text.
 * @param wnd destination window
 * @param pos block position
 * @param fields array of fields to print
 */
static void print_block(struct pixmap* wnd, enum position pos,
                        struct array* fields)
{
    struct keyval lines[MAX_LINES] = { 0 };
    size_t lnum;

    if (!fields) {
        return; // nothing to show
    }

    lnum = 0;
    for (size_t i = 0; i < fields->size && lnum < ARRAY_SIZE(lines); ++i) {
        const struct field* field = arr_nth(fields, i);
        const struct keyval* origin = &ctx.fields[field->type];

        if (field->type == info_status) {
            if (!ctx.status.active) {
                continue;
            }
        } else {
            if (!ctx.info.active) {
                continue;
            }
        }

        if (field->type == info_exif) {
            // Notify that exif will be displayed and check if we should update
            // Hack to get the exif immediately if the user just switched the info on
            const struct image* to_rerender = ensure_correct_exif(NULL);
            if (to_rerender) {
                import_meta(to_rerender);
            }

            if (ctx.meta) {
                for (size_t j = 0;
                     j < ctx.meta->size && lnum < ARRAY_SIZE(lines); ++j) {
                    const struct keyval* kv = arr_nth(ctx.meta, j);
                    if (field->title) {
                        lines[lnum].key = kv->key;
                    }
                    lines[lnum++].value = kv->value;
                }
            }
            continue;
        }

        if (origin->value.width) {
            if (field->title) {
                lines[lnum].key = origin->key;
            }
            lines[lnum++].value = origin->value;
        }
    }

    if (lnum) {
        print_keyval(wnd, pos, lines, lnum);
    }
}

/**
 * Import meta data from image.
 * @param image source image
 */
static void import_meta(const struct image* img)
{
    ensure_correct_exif(img);

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
 * Parse fields from config line.
 * @param config line to parse
 * @return array with set of fields
 */
static struct array* parse_fields(const char* conf)
{
    const char* none = "none";
    const size_t nlen = strlen(none);
    struct array* fields;
    struct str_slice slices[MAX_LINES];
    size_t slices_num;
    size_t index;

    // split config line into fields slices
    slices_num = str_split(conf, ',', slices, ARRAY_SIZE(slices));
    if (slices_num > ARRAY_SIZE(slices)) {
        slices_num = ARRAY_SIZE(slices);
    }

    fields = arr_create(slices_num, sizeof(struct field));
    if (!fields) {
        return NULL;
    }

    index = 0;
    for (size_t i = 0; i < slices_num; ++i) {
        struct field* field = arr_nth(fields, index);
        ssize_t field_type;
        struct str_slice* sl = &slices[i];

        // title show/hide ('+' at the beginning)
        field->title = (sl->len > 0 && *sl->value == '+');
        if (field->title) {
            ++sl->value;
            --sl->len;
        }

        // handle "none"
        if (sl->len == nlen && strncmp(sl->value, none, nlen) == 0) {
            continue;
        }

        // field type
        field_type = str_index(field_names, sl->value, sl->len);
        if (field_type < 0) {
            fprintf(stderr, "Invalid info field: %.*s\n", (int)sl->len,
                    sl->value);
            continue;
        }

        field->type = field_type;
        ++index;
    }

    if (index == 0) {
        arr_free(fields);
        fields = NULL;
    } else if (index != fields->size) {
        arr_resize(fields, index);
    }

    return fields;
}

/**
 * Load scheme from configuration.
 * @param name name of the scheme
 * @param params list of config parameters
 * @return pointer to constructed scheme or NULL on errors
 */
static struct scheme* create_scheme(const char* name,
                                    const struct config_keyval* params)
{
    const size_t name_len = strlen(name);
    struct scheme* scheme = calloc(1, sizeof(struct scheme) + name_len);
    if (!scheme) {
        return NULL;
    }

    memcpy(scheme->name, name, name_len);

    list_for_each(params, const struct config_keyval, kv) {
        const ssize_t bpos = str_index(position_names, kv->key, 0);
        if (bpos < 0) {
            fprintf(stderr, "Invalid info position: %s\n", kv->key);
            continue;
        }
        scheme->fields[bpos] = parse_fields(kv->value);
    }

    return scheme;
}

void info_init(const struct config* cfg, const char* mode)
{
    const struct config* section;

    // load all schemes
    const char* modes[] = { CFG_VIEWER, CFG_SLIDESHOW, CFG_GALLERY };
    for (size_t i = 0; i < ARRAY_SIZE(modes); ++i) {
        struct scheme* scheme;
        char name[16] = { 0 };
        snprintf(name, sizeof(name), CFG_INFO ".%s", modes[i]);
        section = config_section(cfg, name);
        scheme = create_scheme(modes[i], section->params);
        if (scheme) {
            scheme->next = ctx.schemes;
            ctx.schemes = scheme;
        }
    }

    ctx.current = ctx.schemes;

    section = config_section(cfg, CFG_INFO);
    ctx.show = config_get_bool(section, CFG_INFO_SHOW);
    ctx.padding = config_get_num(section, CFG_INFO_PADDING, 0, 256);
    ctx.info.delay = config_get_num(section, CFG_INFO_ITIMEOUT, 0, 1024) * 1000;
    timeout_init(&ctx.info);
    const char* exiftool_args = config_get(section, CFG_INFO_EXIFTOOL_ARGS);
    if (strcmp(exiftool_args, "none") == 0) {
        ctx.exiftool_args = "";
    } else {
        ctx.exiftool_args = strdup(exiftool_args);
    }
    ctx.status.delay =
        config_get_num(section, CFG_INFO_STIMEOUT, 0, 1024) * 1000;
    timeout_init(&ctx.status);

    info_set_default(mode, 0);
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
    free_help();
    free_meta();

    for (size_t i = 0; i < ARRAY_SIZE(ctx.fields); ++i) {
        free(ctx.fields[i].key.data);
        free(ctx.fields[i].value.data);
    }

    while (ctx.schemes) {
        struct scheme* next = ctx.schemes->next;
        for (size_t i = 0; i < ARRAY_SIZE(ctx.schemes->fields); ++i) {
            arr_free(ctx.schemes->fields[i]);
        }
        free(ctx.schemes);
        ctx.schemes = next;
    }
}

void info_switch(const char* expression)
{
    struct str_slice modes[6];
    size_t modes_num = 0;

    if (!ctx.info.active && ctx.show) {
        timeout_reset(&ctx.info);
        return; // no switch, just show existing scheme hidden by timeout
    }

    if (expression && *expression) {
        modes_num = str_split(expression, ',', modes, ARRAY_SIZE(modes));
    }

    if (modes_num == 0) { // switch to next available mode
        struct scheme* next = ctx.current->next;
        ctx.show = !!next;
        ctx.current = next ? next : ctx.schemes;
    } else { // switch between specified modes
        const char* cur_name = ctx.show ? ctx.current->name : "off";
        const size_t cur_len = strlen(cur_name);
        size_t mode_idx = SIZE_MAX;

        if (modes_num > ARRAY_SIZE(modes)) {
            modes_num = ARRAY_SIZE(modes);
        }

        // find current mode slice
        for (size_t i = 0; i < modes_num; ++i) {
            if (modes[i].len == cur_len &&
                strncmp(modes[i].value, cur_name, cur_len) == 0) {
                mode_idx = i;
                break;
            }
        }

        // get next mode slice
        if (mode_idx == SIZE_MAX || ++mode_idx >= modes_num) {
            mode_idx = 0;
        }

        // set next mode
        if (modes[mode_idx].len == 3 &&
            strncmp(modes[mode_idx].value, "off", 3) == 0) {
            ctx.show = false;
        } else if (info_set_default(modes[mode_idx].value,
                                    modes[mode_idx].len)) {
            ctx.show = true;
        }
    }

    timeout_reset(&ctx.info);
}

bool info_set_default(const char* name, size_t len)
{
    struct scheme* it = ctx.schemes;

    if (len == 0) {
        len = strlen(name);
    }

    while (it) {
        if (strlen(it->name) == len && strncmp(name, it->name, len) == 0) {
            ctx.current = it;
            return true;
        }
        it = it->next;
    }

    fprintf(stderr, "Invalid info scheme: %.*s\n", (int)len, name);
    return false;
}

void info_reset(const struct image* img)
{
    const size_t mib = 1024 * 1024;
    const char unit = img->file_size >= mib ? 'M' : 'K';
    const float sz =
        (float)img->file_size / (img->file_size >= mib ? mib : 1024);

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

void info_update_index(enum info_field field, size_t current, size_t total)
{
    if (total > 1) {
        info_update(field, "%zd of %zd", current, total);
    } else {
        info_update(field, NULL);
    }
}

void info_print(struct pixmap* window)
{
    if (ctx.show) {
        for (size_t i = 0; i < ARRAY_SIZE(ctx.current->fields); ++i) {
            print_block(window, i, ctx.current->fields[i]);
        }
    }
    if (help_visible()) {
        print_help(window);
    }
}

void help_show(const struct keybind* kb)
{
    const struct keybind* it;
    size_t lines;

    free_help();

    // get number of help lines
    lines = 0;
    it = kb;
    while (it) {
        if (it->help) {
            ++lines;
        }
        it = it->next;
    }

    // create help layer
    ctx.help = arr_create(lines, sizeof(struct text_surface));
    if (!ctx.help) {
        return;
    }
    it = kb;
    while (it) {
        if (it->help) {
            struct text_surface* line = arr_nth(ctx.help, --lines);
            font_render(it->help, line);
        }
        it = it->next;
    }
}

void help_hide(void)
{
    free_help();
}

bool help_visible(void)
{
    return ctx.help;
}

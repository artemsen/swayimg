// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "info.h"

#include "config.h"
#include "imagelist.h"
#include "loader.h"
#include "str.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off

// Section name in the config file
#define CONFIG_SECTION "info"

#define INFO_FIELDS_NUM 10

/** Display modes. */
enum info_mode {
    info_mode_full,
    info_mode_brief,
    info_mode_off,
};
static const char* mode_names[] = {
    [info_mode_full] = "full",
    [info_mode_brief] = "brief",
    [info_mode_off] = "off",
};
#define MODES_NUM 2

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
    [text_top_left] = "topleft",
    [text_top_right] = "topright",
    [text_bottom_left] = "bottomleft",
    [text_bottom_right] = "bottomright",
};
#define INFO_POSITION_NUM ARRAY_SIZE(position_names)

// Defaults
static const enum info_field default_full_top_left[] = {
    info_file_name,
    info_image_format,
    info_file_size,
    info_image_size,
    info_exif,
};
static const enum info_field default_full_top_right[] = {
    info_index,
};
static const enum info_field default_full_bottom_left[] = {
    info_scale,
    info_frame,
};
static const enum info_field default_bottom_right[] = {
    info_status,
};
static const enum info_field default_brief_top_left[] = {
    info_index,
};

// clang-format on

#define SET_DEFAULT(m, p, d)                               \
    ctx.blocks[m][p].scheme_sz = sizeof(d) / sizeof(d[0]); \
    ctx.blocks[m][p].scheme = malloc(sizeof(d));           \
    memcpy(ctx.blocks[m][p].scheme, d, sizeof(d))

/** Single info block. */
struct info_block {
    struct text_keyval* lines;
    enum info_field* scheme;
    size_t scheme_sz;
};

/** Info data context. */
struct info_context {
    enum info_mode mode;
    int timeout;
    struct text_keyval* exif_lines;
    size_t exif_num;
    struct text_keyval fields[INFO_FIELDS_NUM];
    struct info_block blocks[MODES_NUM][INFO_POSITION_NUM];
};

/** Global info context. */
static struct info_context ctx;

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
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum info_mode mode;
    struct info_block* block = NULL;
    struct str_slice slices[INFO_FIELDS_NUM];
    size_t num_slices;
    enum info_field scheme[INFO_FIELDS_NUM];
    size_t scheme_sz = 0;
    ssize_t index;

    if (strcmp(key, "timeout") == 0) {
        ssize_t num, num_max;
        bool is_percent;
        size_t len = strlen(value);

        if (len == 0) {
            return cfgst_invalid_value;
        }
        is_percent = (value[len - 1] == '%');
        if (is_percent) {
            --len;
        }
        num_max = is_percent ? 100 : 86400;
        if (!str_to_num(value, len, &num, 0) && num >= 0 && num <= num_max) {
            return cfgst_invalid_value;
        }
        ctx.timeout = num * (is_percent ? -1 : 1);
        return cfgst_ok;
    }

    if (strcmp(key, "mode") == 0) {
        index = str_index(mode_names, value, 0);
        if (index < 0) {
            return cfgst_invalid_value;
        }
        ctx.mode = index;
        return cfgst_ok;
    }

    // parse key (mode.position)
    if (str_split(key, '.', slices, 2) != 2) {
        return cfgst_invalid_key;
    }

    // get mode
    index =
        str_search_index(mode_names, MODES_NUM, slices[0].value, slices[0].len);
    if (index < 0) {
        return cfgst_invalid_value;
    }
    mode = index;

    // get position and its block
    index = str_index(position_names, slices[1].value, slices[1].len);
    if (index < 0) {
        return cfgst_invalid_value;
    }
    block = &ctx.blocks[mode][index];

    // split into list fileds
    num_slices =
        str_split(value, ',', slices, sizeof(slices) / sizeof(slices[0]));
    if (num_slices > sizeof(slices) / sizeof(slices[0])) {
        num_slices = sizeof(slices) / sizeof(slices[0]);
    }
    for (size_t i = 0; i < num_slices; ++i) {
        index = str_index(field_names, slices[i].value, slices[i].len);
        if (index >= 0) {
            scheme[scheme_sz++] = index;
        } else {
            if (slices[i].len == 0 ||
                (slices[i].len == 4 &&
                 strncmp(slices[i].value, "none", 4) == 0)) {
                continue; // skip empty fields
            }
            return cfgst_invalid_value;
        }
    }

    // set new scheme
    if (scheme_sz) {
        block->scheme =
            realloc(block->scheme, scheme_sz * sizeof(enum info_field));
        memcpy(block->scheme, scheme, scheme_sz * sizeof(enum info_field));
    } else {
        free(block->scheme);
        block->scheme = NULL;
    }
    block->scheme_sz = scheme_sz;

    return cfgst_ok;
}

void info_create(void)
{
    // set defaults
    ctx.mode = info_mode_full;
    ctx.timeout = 0;
    SET_DEFAULT(info_mode_full, text_top_left, default_full_top_left);
    SET_DEFAULT(info_mode_full, text_top_right, default_full_top_right);
    SET_DEFAULT(info_mode_full, text_bottom_left, default_full_bottom_left);
    SET_DEFAULT(info_mode_full, text_bottom_right, default_bottom_right);
    SET_DEFAULT(info_mode_brief, text_top_left, default_brief_top_left);
    SET_DEFAULT(info_mode_brief, text_bottom_right, default_bottom_right);

    // register configuration loader
    config_add_loader(CONFIG_SECTION, load_config);
}

void info_init(void)
{
    font_render("File name:", &ctx.fields[info_file_name].key);
    font_render("File path:", &ctx.fields[info_file_path].key);
    font_render("File size:", &ctx.fields[info_file_size].key);
    font_render("Image format:", &ctx.fields[info_image_format].key);
    font_render("Image size:", &ctx.fields[info_image_size].key);
}

void info_destroy(void)
{
    for (size_t i = 0; i < ctx.exif_num; ++i) {
        free(ctx.exif_lines[i].key.data);
        free(ctx.exif_lines[i].value.data);
    }

    for (size_t i = 0; i < MODES_NUM; ++i) {
        for (size_t j = 0; j < INFO_POSITION_NUM; ++j) {
            free(ctx.blocks[i][j].lines);
            free(ctx.blocks[i][j].scheme);
        }
    }

    for (size_t i = 0; i < INFO_FIELDS_NUM; ++i) {
        free(ctx.fields[i].key.data);
        free(ctx.fields[i].value.data);
    }
}

void info_set_mode(const char* mode)
{
    if (mode && *mode) {
        const size_t num_modes = sizeof(mode_names) / sizeof(mode_names[0]);
        for (size_t i = 0; i < num_modes; ++i) {
            if (strcmp(mode, mode_names[i]) == 0) {
                ctx.mode = i;
                return;
            }
        }
    }

    ++ctx.mode;
    if (ctx.mode > info_mode_off) {
        ctx.mode = 0;
    }
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
}

size_t info_height(enum text_position pos)
{
    const struct info_block* block;
    size_t lines_num;

    if (ctx.mode == info_mode_off || pos == text_center) {
        return 0;
    }

    block = &ctx.blocks[ctx.mode][pos];
    lines_num = block->scheme_sz;

    for (size_t i = 0; i < block->scheme_sz; ++i) {
        switch (block->scheme[i]) {
            case info_exif:
                --lines_num;
                lines_num += ctx.exif_num;
                break;
            case info_frame:
                if (!ctx.fields[info_frame].value.data) {
                    --lines_num;
                }
                break;
            case info_status:
                if (!ctx.fields[info_status].value.data) {
                    --lines_num;
                }
                break;
            case info_index:
                if (image_list_size() == 1) {
                    --lines_num;
                }
                break;
            default:
                break;
        }
    }

    return lines_num;
}

const struct text_keyval* info_lines(enum text_position pos)
{
    const size_t lines_num = info_height(pos);
    struct info_block* block;
    struct text_keyval* line;

    if (ctx.mode == info_mode_off) {
        return 0;
    }

    block = &ctx.blocks[ctx.mode][pos];

    line = realloc(block->lines, lines_num * sizeof(*line));
    if (!line) {
        return NULL;
    }

    block->lines = line;

    for (size_t i = 0; i < block->scheme_sz; ++i) {
        switch (block->scheme[i]) {
            case info_exif:
                memcpy(line, ctx.exif_lines, ctx.exif_num * sizeof(*line));
                line += ctx.exif_num;
                break;
            case info_frame:
                if (ctx.fields[info_frame].value.data) {
                    memcpy(line, &ctx.fields[block->scheme[i]], sizeof(*line));
                    ++line;
                }
                break;
            case info_status:
                if (ctx.fields[info_status].value.data) {
                    memcpy(line, &ctx.fields[block->scheme[i]], sizeof(*line));
                    ++line;
                }
                break;
            case info_index:
                if (image_list_size() > 1) {
                    memcpy(line, &ctx.fields[block->scheme[i]], sizeof(*line));
                    ++line;
                }
                break;
            default:
                memcpy(line, &ctx.fields[block->scheme[i]], sizeof(*line));
                ++line;
                break;
        }
    }

    return block->lines;
}

int info_timeout(void)
{
    return ctx.timeout;
}

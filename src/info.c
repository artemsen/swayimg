// SPDX-License-Identifier: MIT
// Image info: text blocks with image meta data.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#include "info.h"

#include "canvas.h"
#include "config.h"
#include "imagelist.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// clang-format off

// Section name in the config file
#define CONFIG_SECTION "info"

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

/** Available fields. */
enum info_field {
    info_file_name,
    info_file_path,
    info_file_size,
    info_image_format,
    info_image_size,
    info_exif,
    info_frame,
    info_index,
    info_scale,
    info_status,
};
#define INFO_FIELDS_NUM 10

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
    [info_top_left] = "topleft",
    [info_top_right] = "topright",
    [info_bottom_left] = "bottomleft",
    [info_bottom_right] = "bottomright",
};

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
    struct info_line* lines;
    enum info_field* scheme;
    size_t scheme_sz;
};

/** Info data context. */
struct info_context {
    enum info_mode mode;

    const char* file;
    struct info_line* exif_lines;
    size_t exif_num;
    size_t frame;
    size_t frame_total;
    size_t index;
    size_t width;
    size_t height;
    size_t scale;
    struct info_line fields[INFO_FIELDS_NUM];
    struct info_block blocks[MODES_NUM][INFO_POSITION_NUM];
};
static struct info_context ctx;

/**
 * Convert ansi string to wide char format.
 * @param text source string to encode
 * @return pointer to wide string, caller must free it
 */
static wchar_t* ansi_to_wide(wchar_t* buf, const char* text)
{
    wchar_t* ptr;
    size_t len;

    len = mbstowcs(NULL, text, 0);
    if (len == (size_t)-1) {
        goto error;
    }

    ++len; // last null

    ptr = realloc(buf, len * sizeof(wchar_t));
    if (!ptr) {
        goto error;
    }

    mbstowcs(ptr, text, len);

    return ptr;

error:
    if (buf) {
        *buf = 0;
    }
    return buf;
}

/**
 * Copy wide string (strdup).
 * @param text source text to copy
 * @return pointer to the allocated buffer
 */
static wchar_t* copy_wide(const wchar_t* text)
{
    const size_t len = (wcslen(text) + 1) * sizeof(wchar_t);
    wchar_t* buffer = malloc(len);
    if (buffer) {
        memcpy(buffer, text, len);
    }
    return buffer;
}

/**
 * Update field value.
 * @param field field to update
 * @param fmt value format
 */
__attribute__((format(printf, 2, 3))) static void
update_field(enum info_field field, const char* fmt, ...)
{
    va_list args;
    char buffer[256];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    ctx.fields[field].value = ansi_to_wide(ctx.fields[field].value, buffer);
}

/**
 * Import meta data from image.
 * @param image source image
 */
static void import_exif(const struct image* image)
{
    struct info_line* line;

    for (size_t i = 0; i < ctx.exif_num; ++i) {
        free(ctx.exif_lines[i].key);
        free(ctx.exif_lines[i].value);
    }
    ctx.exif_num = 0;

    if (image->num_info == 0) {
        return;
    }

    line = realloc(ctx.exif_lines, image->num_info * sizeof(struct info_line));
    if (!line) {
        return;
    }

    ctx.exif_num = image->num_info;
    ctx.exif_lines = line;

    for (size_t i = 0; i < ctx.exif_num; ++i) {
        const struct image_info* exif = &image->info[i];
        line->key = ansi_to_wide(NULL, exif->key);
        line->value = ansi_to_wide(NULL, exif->value);
        ++line;
    }
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum info_mode mode = info_mode_off;
    struct info_block* block = NULL;
    struct config_token tokens[INFO_FIELDS_NUM];
    size_t token_num;
    enum info_field scheme[INFO_FIELDS_NUM];
    size_t scheme_sz = 0;

    if (strcmp(key, "mode") == 0) {
        const size_t num_modes = sizeof(mode_names) / sizeof(mode_names[0]);
        for (size_t i = 0; i < num_modes; ++i) {
            if (strcmp(value, mode_names[i]) == 0) {
                ctx.mode = i;
                return cfgst_ok;
            }
        }
        return cfgst_invalid_value;
    }

    // tokenize key (mode.position)
    if (config_parse_tokens(key, '.', tokens,
                            sizeof(tokens) / sizeof(tokens[0])) != 2) {
        return cfgst_invalid_key;
    }

    // get mode
    for (size_t i = 0; i < MODES_NUM; ++i) {
        const char* name = mode_names[i];
        if (strlen(name) == tokens[0].len &&
            strncmp(tokens[0].value, name, tokens[0].len) == 0) {
            mode = i;
            break;
        }
    }
    if (mode == info_mode_off) {
        return cfgst_invalid_key;
    }

    // get position and its block
    for (size_t i = 0; i < INFO_POSITION_NUM; ++i) {
        const char* name = position_names[i];
        if (strlen(name) == tokens[1].len &&
            strncmp(tokens[1].value, name, tokens[1].len) == 0) {
            block = &ctx.blocks[mode][i];
            break;
        }
    }
    if (!block) {
        return cfgst_invalid_key;
    }

    // tokenize value (list of fields)
    token_num = config_parse_tokens(value, ',', tokens,
                                    sizeof(tokens) / sizeof(tokens[0]));
    // convert tokens to fields
    if (token_num > sizeof(tokens) / sizeof(tokens[0])) {
        token_num = sizeof(tokens) / sizeof(tokens[0]);
    }
    for (size_t i = 0; i < token_num; ++i) {
        bool found = false;

        // skip empty fields
        if (tokens[i].len == 0 ||
            (tokens[i].len == 4 && strncmp(tokens[i].value, "none", 4) == 0)) {
            continue;
        }

        for (size_t j = 0; !found && j < INFO_FIELDS_NUM; ++j) {
            const char* name = field_names[j];
            found = strlen(name) == tokens[i].len &&
                strncmp(tokens[i].value, name, tokens[i].len) == 0;
            if (found) {
                scheme[scheme_sz++] = j;
            }
        }
        if (!found) {
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

void info_init(void)
{
    // standart fields
    ctx.fields[info_file_name].key = copy_wide(L"File name");
    ctx.fields[info_file_path].key = copy_wide(L"File path");
    ctx.fields[info_file_size].key = copy_wide(L"File size");
    ctx.fields[info_image_format].key = copy_wide(L"Image format");
    ctx.fields[info_image_size].key = copy_wide(L"Image size");

    // set defaults
    ctx.mode = info_mode_full;
    ctx.frame = UINT32_MAX;
    ctx.index = UINT32_MAX;
    SET_DEFAULT(info_mode_full, info_top_left, default_full_top_left);
    SET_DEFAULT(info_mode_full, info_top_right, default_full_top_right);
    SET_DEFAULT(info_mode_full, info_bottom_left, default_full_bottom_left);
    SET_DEFAULT(info_mode_full, info_bottom_right, default_bottom_right);
    SET_DEFAULT(info_mode_brief, info_top_left, default_brief_top_left);
    SET_DEFAULT(info_mode_brief, info_bottom_right, default_bottom_right);

    // register configuration loader
    config_add_section(CONFIG_SECTION, load_config);
}

void info_free(void)
{
    for (size_t i = 0; i < MODES_NUM; ++i) {
        for (size_t j = 0; j < INFO_POSITION_NUM; ++j) {
            free(ctx.blocks[i][j].lines);
            free(ctx.blocks[i][j].scheme);
        }
    }
    for (size_t i = 0; i < INFO_FIELDS_NUM; ++i) {
        free(ctx.fields[i].key);
        free(ctx.fields[i].value);
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

void info_update(size_t frame_idx)
{
    const struct image_entry entry = image_list_current();
    const struct image* image = entry.image;
    const struct image_frame* frame = &image->frames[frame_idx];
    const size_t scale_percent = canvas_get_scale() * 100;

    if (ctx.file != image->file_path) {
        const size_t mib = 1024 * 1024;
        const char unit = image->file_size >= mib ? 'M' : 'K';
        const float sz =
            (float)image->file_size / (image->file_size >= mib ? mib : 1024);

        update_field(info_file_size, "%.02f %ciB", sz, unit);

        import_exif(image);

        ctx.fields[info_file_name].value =
            ansi_to_wide(ctx.fields[info_file_name].value, image->file_name);
        ctx.fields[info_file_path].value =
            ansi_to_wide(ctx.fields[info_file_path].value, image->file_path);
        ctx.fields[info_image_format].value =
            ansi_to_wide(ctx.fields[info_image_format].value, image->format);

        ctx.frame = UINT32_MAX; // this will refresh frame info

        ctx.file = image->file_path;
    }

    if (ctx.frame != frame_idx || ctx.frame_total != image->num_frames) {
        ctx.frame = frame_idx;
        ctx.frame_total = image->num_frames;
        update_field(info_frame, "%lu of %lu", ctx.frame + 1, ctx.frame_total);
    }

    if (ctx.index != entry.index) {
        ctx.index = entry.index;
        update_field(info_index, "%lu of %lu", ctx.index + 1,
                     image_list_size());
    }

    if (ctx.scale != scale_percent) {
        ctx.scale = scale_percent;
        update_field(info_scale, "%ld%%", ctx.scale);
    }

    if (ctx.width != frame->width || ctx.height != frame->height) {
        ctx.width = frame->width;
        ctx.height = frame->height;
        update_field(info_image_size, "%lux%lu", ctx.width, ctx.height);
    }
}

void info_set_status(const char* fmt, ...)
{
    va_list args;
    char buffer[256];

    if (!fmt) {
        if (ctx.fields[info_status].value) {
            *ctx.fields[info_status].value = 0;
        }
        return;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    ctx.fields[info_status].value =
        ansi_to_wide(ctx.fields[info_status].value, buffer);
}

size_t info_height(enum info_position pos)
{
    const struct info_block* block;
    size_t lines_num;

    if (ctx.mode == info_mode_off) {
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
                if (ctx.frame_total == 1) {
                    --lines_num;
                }
                break;
            case info_status:
                if (!ctx.fields[info_status].value ||
                    !*ctx.fields[info_status].value) {
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

const struct info_line* info_lines(enum info_position pos)
{
    const size_t lines_num = info_height(pos);
    struct info_block* block;
    struct info_line* line;

    if (ctx.mode == info_mode_off) {
        return 0;
    }

    block = &ctx.blocks[ctx.mode][pos];

    line = realloc(block->lines, lines_num * sizeof(struct info_line));
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
                if (ctx.frame_total != 1) {
                    memcpy(line, &ctx.fields[block->scheme[i]], sizeof(*line));
                    ++line;
                }
                break;
            case info_status:
                if (ctx.fields[info_status].value &&
                    *ctx.fields[info_status].value) {
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

// SPDX-License-Identifier: MIT
// Text renderer.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "text.h"

#include "config.h"
#include "ui.h"

#include <string.h>

// Space between text layout and window edge
#define TEXT_PADDING 10

/** Text context description. */
struct text {
    argb_t color;  ///< Font color
    argb_t shadow; ///< Font shadow color
};

/** Global text context. */
static struct text ctx;

/**
 * Draw text surface on window.
 * @param wnd destination window
 * @param x,y text position
 * @param text text surface to draw
 */
static void put_text(struct pixmap* wnd, size_t x, size_t y,
                     const struct text_surface* text)
{
    if (ARGB_GET_A(ctx.shadow)) {
        size_t shadow_offset = text->height / 16;
        if (shadow_offset < 1) {
            shadow_offset = 1;
        }
        pixmap_apply_mask(wnd, x + shadow_offset, y + shadow_offset, text->data,
                          text->width, text->height, ctx.shadow);
    }

    pixmap_apply_mask(wnd, x, y, text->data, text->width, text->height,
                      ctx.color);
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, "color") == 0) {
        if (config_to_color(value, &ctx.color)) {
            status = cfgst_ok;
        }
    } else if (strcmp(key, "shadow") == 0) {
        if (strcmp(value, "none") == 0) {
            ctx.shadow = 0;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.shadow)) {
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void text_create(void)
{
    // default settings
    ctx.color = 0xffcccccc;
    ctx.shadow = 0x80000000;

    // register configuration loader
    config_add_loader(FONT_CONFIG_SECTION, load_config);
}

void text_print(struct pixmap* wnd, enum text_position pos,
                const struct text_surface* text)
{
    size_t x = 0, y = 0;

    // calculate line position
    switch (pos) {
        case text_center:
            x = wnd->width / 2 - text->width / 2;
            y = wnd->height / 2 - text->height / 2;
            break;
        case text_top_left:
            x = TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case text_top_right:
            x = wnd->width - TEXT_PADDING - text->width;
            y = TEXT_PADDING;
            break;
        case text_bottom_left:
            x = TEXT_PADDING;
            y = wnd->height - TEXT_PADDING - text->height;
            break;
        case text_bottom_right:
            x = wnd->width - TEXT_PADDING - text->width;
            y = wnd->height - TEXT_PADDING - text->height;
            break;
    }

    put_text(wnd, x, y, text);
}

void text_print_lines(struct pixmap* wnd, enum text_position pos,
                      const struct text_surface* lines, size_t lines_num)
{
    if (pos != text_center) {
        return; // not supported (not used anywhere)
    }

    const size_t line_height = lines[0].height;
    const size_t row_max = (wnd->height - TEXT_PADDING * 2) / line_height;
    const size_t columns =
        (lines_num / row_max) + (lines_num % row_max ? 1 : 0);
    const size_t rows = (lines_num / columns) + (lines_num % columns ? 1 : 0);
    const size_t col_space = line_height;
    size_t total_width = 0;
    size_t top = 0;
    size_t left = 0;

    // calculate total width
    for (size_t col = 0; col < columns; ++col) {
        size_t max_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            const size_t index = row + col * rows;
            if (index >= lines_num) {
                break;
            }
            if (max_width < lines[index].width) {
                max_width = lines[index].width;
            }
        }
        total_width += max_width;
    }
    total_width += col_space * (columns - 1);

    // top left corner of the centered text block
    if (total_width < ui_get_width()) {
        left = wnd->width / 2 - total_width / 2;
    }
    if (rows * line_height < ui_get_height()) {
        top = wnd->height / 2 - (rows * line_height) / 2;
    }

    // put text on window
    for (size_t col = 0; col < columns; ++col) {
        size_t y = top;
        size_t col_width = 0;
        for (size_t row = 0; row < rows; ++row) {
            const size_t index = row + col * rows;
            if (index >= lines_num) {
                break;
            }
            put_text(wnd, left, y, &lines[index]);
            if (col_width < lines[index].width) {
                col_width = lines[index].width;
            }
            y += line_height;
        }
        left += col_width + col_space;
    }
}

void text_print_keyval(struct pixmap* wnd, enum text_position pos,
                       const struct text_keyval* lines, size_t lines_num)
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
            case text_center:
                return; // not supported (not used anywhere)
            case text_top_left:
                y = TEXT_PADDING + i * height;
                if (key->data) {
                    x_key = TEXT_PADDING;
                    x_val = TEXT_PADDING + max_key_width;
                } else {
                    x_val = TEXT_PADDING;
                }
                break;
            case text_top_right:
                y = TEXT_PADDING + i * height;
                x_val = wnd->width - TEXT_PADDING - value->width;
                if (key->data) {
                    x_key = x_val - key->width - TEXT_PADDING;
                }
                break;
            case text_bottom_left:
                y = wnd->height - TEXT_PADDING - height * lines_num +
                    i * height;
                if (key->data) {
                    x_key = TEXT_PADDING;
                    x_val = TEXT_PADDING + max_key_width;
                } else {
                    x_val = TEXT_PADDING;
                }
                break;
            case text_bottom_right:
                y = wnd->height - TEXT_PADDING - height * lines_num +
                    i * height;
                x_val = wnd->width - TEXT_PADDING - value->width;
                if (key->data) {
                    x_key = x_val - key->width - TEXT_PADDING;
                }
                break;
        }

        if (key->data) {
            put_text(wnd, x_key, y, key);
            x_key += key->width;
        }
        put_text(wnd, x_val, y, value);
    }
}

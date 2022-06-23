// SPDX-License-Identifier: MIT
// Text renderer based on the Pango framework.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "text.h"

#include <pango/pango.h>
#include <pango/pangocairo.h>

/** Text padding: space between text layout and window edge. */
#define TEXT_PADDING 10

/** Text renderer context. */
struct text_render {
    PangoFontDescription* font; ///< Font description
    uint32_t color;             ///< Font color
};

text_render_t* init_text(config_t* cfg)
{
    text_render_t* ctx;

    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->font = pango_font_description_from_string(cfg->font_face);
    if (!ctx->font) {
        free(ctx);
        return NULL;
    }

    ctx->color = cfg->font_color;

    return ctx;
}

void free_text(text_render_t* ctx)
{
    if (ctx) {
        if (ctx->font) {
            pango_font_description_free(ctx->font);
        }
        free(ctx);
    }
}

void print_text(text_render_t* ctx, cairo_t* cairo, text_position_t pos,
                const char* text)
{
    PangoLayout* layout;
    PangoTabArray* tab;
    cairo_surface_t* surface;
    int x = 0, y = 0;
    int wnd_w = 0, wnd_h = 0;
    int txt_w = 0, txt_h = 0;

    layout = pango_cairo_create_layout(cairo);
    if (!layout) {
        return;
    }

    // get window size
    surface = cairo_get_target(cairo);
    wnd_w = cairo_image_surface_get_width(surface);
    wnd_h = cairo_image_surface_get_height(surface);

    // setup layout
    pango_layout_set_font_description(layout, ctx->font);
    pango_layout_set_text(layout, text, -1);

    // magic, need to handle tabs in a right way
    tab = pango_tab_array_new_with_positions(1, true, PANGO_TAB_LEFT, 200);
    pango_layout_set_tabs(layout, tab);
    pango_tab_array_free(tab);

    // calculate layout position
    pango_layout_get_size(layout, &txt_w, &txt_h);
    txt_w /= PANGO_SCALE;
    txt_h /= PANGO_SCALE;
    switch (pos) {
        case text_top_left:
            x = TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case text_top_right:
            x = wnd_w - txt_w - TEXT_PADDING;
            y = TEXT_PADDING;
            break;
        case text_bottom_left:
            x = TEXT_PADDING;
            y = wnd_h - txt_h - TEXT_PADDING;
            break;
        case text_bottom_right:
            x = wnd_w - txt_w - TEXT_PADDING;
            y = wnd_h - txt_h - TEXT_PADDING;
            break;
    }

    // put layout on cairo surface
    cairo_set_source_rgb(cairo, RGB_RED(ctx->color), RGB_GREEN(ctx->color),
                         RGB_BLUE(ctx->color));
    cairo_move_to(cairo, x, y);
    pango_cairo_show_layout(cairo, layout);
    cairo_identity_matrix(cairo);

    g_object_unref(layout);
}

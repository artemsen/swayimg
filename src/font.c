// SPDX-License-Identifier: MIT
// Font renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.h"

#include "array.h"

// font related
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define POINT_FACTOR 64.0 // default points per pixel for 26.6 format
#define SPACE_WH_REL 2.0

#define BACKGROUND_PADDING 5

/** Font context. */
struct font {
    FT_Library lib;    ///< Font lib instance
    FT_Face face;      ///< Font face instance
    size_t size;       ///< Font size in points
    argb_t color;      ///< Font color
    argb_t shadow;     ///< Font shadow color
    argb_t background; ///< Font background
};

/** Global font context instance. */
static struct font ctx;

/**
 * Get path to the font file by its name.
 * @param name font name
 * @param font_file output buffer for file path
 * @param len size of buffer
 * @return false if font not found
 */
static bool search_font_file(const char* name, char* font_file, size_t len)
{
    FcConfig* fc = NULL;

    font_file[0] = 0;
    font_file[len - 1] = 0;

    if (FcInit()) {
        fc = FcInitLoadConfigAndFonts();
        if (fc) {
            FcPattern* fc_name = NULL;
            fc_name = FcNameParse((const FcChar8*)name);
            if (fc_name) {
                FcPattern* fc_font = NULL;
                FcResult result;
                FcConfigSubstitute(fc, fc_name, FcMatchPattern);
                FcDefaultSubstitute(fc_name);
                fc_font = FcFontMatch(fc, fc_name, &result);
                if (fc_font) {
                    FcChar8* path = NULL;
                    if (FcPatternGetString(fc_font, FC_FILE, 0, &path) ==
                        FcResultMatch) {
                        strncpy(font_file, (const char*)path, len - 1);
                    }
                    FcPatternDestroy(fc_font);
                }
                FcPatternDestroy(fc_name);
            }
            FcConfigDestroy(fc);
        }
        FcFini();
    }

    return *font_file;
}

/**
 * Calc size of the surface and allocate memory for the mask.
 * @param text string to print
 * @param surface text surface
 * @return base line offset or SIZE_MAX on errors
 */
static size_t allocate_surface(const wchar_t* text,
                               struct text_surface* surface)
{
    const FT_Size_Metrics* metrics = &ctx.face->size->metrics;
    const size_t space_size = metrics->x_ppem / SPACE_WH_REL;
    const size_t height = metrics->height / POINT_FACTOR;
    size_t base_offset =
        (ctx.face->ascender * (metrics->y_scale / 65536.0)) / POINT_FACTOR;
    size_t width = 0;
    uint8_t* data = NULL;
    size_t data_size;

    // get total width
    while (*text) {
        if (*text == L' ') {
            width += space_size;
        } else if (FT_Load_Char(ctx.face, *text, FT_LOAD_RENDER) == 0) {
            const FT_GlyphSlot glyph = ctx.face->glyph;
            width += glyph->advance.x / POINT_FACTOR;
            if ((FT_Int)base_offset < glyph->bitmap_top) {
                base_offset = glyph->bitmap_top;
            }
        }
        ++text;
    }

    // allocate surface buffer
    data_size = width * height;
    if (data_size) {
        data = realloc(surface->data, data_size);
        if (!data) {
            return SIZE_MAX;
        }
        surface->width = width;
        surface->height = height;
        surface->data = data;
        memset(surface->data, 0, data_size);
    }

    return base_offset;
}

void font_init(const struct config* cfg)
{
    char font_file[256];
    const char* font_name;

    // load font
    font_name = config_get(cfg, CFG_FONT, CFG_FONT_NAME);
    if (!search_font_file(font_name, font_file, sizeof(font_file)) ||
        FT_Init_FreeType(&ctx.lib) != 0 ||
        FT_New_Face(ctx.lib, font_file, 0, &ctx.face) != 0) {
        fprintf(stderr, "WARNING: Unable to load font %s\n", font_name);
        return;
    }

    // set font size
    ctx.size = config_get_num(cfg, CFG_FONT, CFG_FONT_SIZE, 1, 256);
    FT_Set_Char_Size(ctx.face, ctx.size * POINT_FACTOR, 0, 96, 0);

    // color/background/shadow parameters
    ctx.color = config_get_color(cfg, CFG_FONT, CFG_FONT_COLOR);
    ctx.background = config_get_color(cfg, CFG_FONT, CFG_FONT_BKG);
    ctx.shadow = config_get_color(cfg, CFG_FONT, CFG_FONT_SHADOW);
}

void font_set_scale(double scale)
{
    FT_Set_Char_Size(ctx.face, ctx.size * POINT_FACTOR, 0, 96 * scale, 0);
}

void font_destroy(void)
{
    if (ctx.face) {
        FT_Done_Face(ctx.face);
    }
    if (ctx.lib) {
        FT_Done_FreeType(ctx.lib);
    }
}

bool font_render(const char* text, struct text_surface* surface)
{
    size_t space_size;
    size_t base_offset;
    wchar_t* wide;
    wchar_t* it;
    size_t x = 0;

    if (!ctx.face) {
        return false;
    }
    if (!text || !*text) {
        surface->width = 0;
        surface->height = 0;
        return true;
    }

    space_size = ctx.face->size->metrics.x_ppem / SPACE_WH_REL;

    wide = str_to_wide(text, NULL);
    if (!wide) {
        return false;
    }

    base_offset = allocate_surface(wide, surface);
    if (base_offset == SIZE_MAX) {
        free(wide);
        return false;
    }

    // draw glyphs
    it = wide;
    while (*it) {
        if (*it == L' ') {
            x += space_size;
        } else if (FT_Load_Char(ctx.face, *it, FT_LOAD_RENDER) == 0) {
            const FT_GlyphSlot glyph = ctx.face->glyph;
            const FT_Bitmap* bmp = &glyph->bitmap;
            const size_t off_y = base_offset - glyph->bitmap_top;
            size_t size;

            // calc line width, floating point math doesn't match bmp width
            if (x + bmp->width < surface->width) {
                size = bmp->width;
            } else {
                size = surface->width - x;
            }

            // put glyph's bitmap on the surface
            for (size_t y = 0; y < bmp->rows; ++y) {
                const size_t offset = (y + off_y) * surface->width + x;
                uint8_t* dst = &surface->data[offset + glyph->bitmap_left];
                memcpy(dst, &bmp->buffer[y * bmp->pitch], size);
            }

            x += glyph->advance.x / POINT_FACTOR;
        }
        ++it;
    }

    free(wide);

    return true;
}

void font_print(struct pixmap* wnd, ssize_t x, ssize_t y,
                const struct text_surface* text)
{
    if (ARGB_GET_A(ctx.background)) {
        pixmap_blend(wnd, x - BACKGROUND_PADDING, y,
                     text->width + BACKGROUND_PADDING * 2, text->height,
                     ctx.background);
    }

    if (ARGB_GET_A(ctx.shadow)) {
        ssize_t shadow_offset = text->height / 16;
        if (shadow_offset < 1) {
            shadow_offset = 1;
        }
        pixmap_apply_mask(wnd, x + shadow_offset, y + shadow_offset, text->data,
                          text->width, text->height, ctx.shadow);
    }

    pixmap_apply_mask(wnd, x, y, text->data, text->width, text->height,
                      ctx.color);
}

// SPDX-License-Identifier: MIT
// Font renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.h"

#include "config.h"
#include "str.h"

// font realted
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define POINT_FACTOR 64.0 // default points per pixel for 26.6 format
#define SPACE_WH_REL 2.0

/** Font context. */
struct font {
    FT_Library lib; ///< Font lib instance
    FT_Face face;   ///< Font face instance
    char* name;     ///< Font face name
    size_t size;    ///< Font size (pt)
    argb_t color;   ///< Font color
    argb_t shadow;  ///< Font shadow color
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
 * @return true if operation completed successfully
 */
static bool allocate_surface(const wchar_t* text, struct text_surface* surface)
{
    const size_t space_size = ctx.face->size->metrics.x_ppem / SPACE_WH_REL;
    const size_t height = ctx.face->size->metrics.height / POINT_FACTOR;
    size_t width = 0;
    uint8_t* data = NULL;
    size_t data_size;

    // get total width
    while (*text) {
        if (*text == L' ') {
            width += space_size;
        } else if (FT_Load_Char(ctx.face, *text, FT_LOAD_RENDER) == 0) {
            width += ctx.face->glyph->advance.x / POINT_FACTOR;
        }
        ++text;
    }

    // allocate surface buffer
    data_size = width * height;
    if (data_size) {
        data = realloc(surface->data, data_size);
        if (data) {
            surface->width = width;
            surface->height = height;
            surface->data = data;
            memset(surface->data, 0, data_size);
        }
    }

    return !!data;
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, "name") == 0) {
        str_dup(value, &ctx.name);
        status = cfgst_ok;
    } else if (strcmp(key, "size") == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num > 0 && num < 1024) {
            ctx.size = num;
            status = cfgst_ok;
        }
    } else if (strcmp(key, "color") == 0) {
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

void font_create(void)
{
    // set defaults
    str_dup("monospace", &ctx.name);
    ctx.size = 14;
    ctx.color = ARGB(0xff, 0xcc, 0xcc, 0xcc);
    ctx.shadow = ARGB(0x80, 0, 0, 0);

    // register configuration loader
    config_add_loader("font", load_config);
}

void font_init(void)
{
    char file[256];
    const FT_F26Dot6 size = ctx.size * POINT_FACTOR;

    if (!search_font_file(ctx.name, file, sizeof(file)) ||
        FT_Init_FreeType(&ctx.lib) != 0 ||
        FT_New_Face(ctx.lib, file, 0, &ctx.face) != 0) {
        fprintf(stderr, "Unable to load font %s\n", ctx.name);
        return;
    }

    FT_Set_Char_Size(ctx.face, size, 0, 96, 0);
}

void font_destroy(void)
{
    if (ctx.face) {
        FT_Done_Face(ctx.face);
    }
    if (ctx.lib) {
        FT_Done_FreeType(ctx.lib);
    }
    free(ctx.name);
}

bool font_render(const char* text, struct text_surface* surface)
{
    size_t space_size;
    ssize_t base_offset;
    wchar_t* wide;
    wchar_t* it;
    size_t x = 0;

    if (!ctx.face) {
        return false;
    }

    space_size = ctx.face->size->metrics.x_ppem / SPACE_WH_REL;
    base_offset =
        (ctx.face->ascender * (ctx.face->size->metrics.y_scale / 65536.0)) /
        POINT_FACTOR;

    wide = str_to_wide(text, NULL);
    if (!wide) {
        return false;
    }
    if (!allocate_surface(wide, surface)) {
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
            const ssize_t off_y = base_offset - glyph->bitmap_top;
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

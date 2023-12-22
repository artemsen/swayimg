// SPDX-License-Identifier: MIT
// Text renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.h"

#include "config.h"

#include <wchar.h>

// font realted
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define SPACE_WH_REL 2
#define GLYPH_GW_REL 4

/** Font context. */
struct font {
    FT_Library font_lib; ///< Font lib instance
    FT_Face font_face;   ///< Font face
    wchar_t* wide;       ///< Buffer for wide char text
    size_t wide_sz;      ///< Size of wide buffer in bytes
};

/**
 * Get path to the font file by its name.
 * @param name font name
 * @param font_file ouput buffer for file path
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
 * Convert utf-8 string to wide char format.
 * @param ctx font context
 * @param text source string to encode
 * @param len length of the input string, 0 for auto
 * @return pointer to wide string
 */
static const wchar_t* to_wide(struct font* ctx, const char* text, size_t len)
{
    size_t ansi_sz = len ? len : strlen(text);
    const size_t wide_sz = (ansi_sz + 1 /*last null*/) * sizeof(wchar_t);
    wchar_t cpt = 0;
    wchar_t* wide;

    if (wide_sz < ctx->wide_sz) {
        wide = ctx->wide;
    } else {
        wide = malloc(wide_sz);
        if (wide) {
            free(ctx->wide);
            ctx->wide = wide;
            ctx->wide_sz = wide_sz;
        } else {
            return NULL;
        }
    }

    while (*text && ansi_sz--) {
        const uint8_t ch = *text;
        if (ch <= 0x7f)
            cpt = ch;
        else if (ch <= 0xbf)
            cpt = (cpt << 6) | (ch & 0x3f);
        else if (ch <= 0xdf)
            cpt = ch & 0x1f;
        else if (ch <= 0xef)
            cpt = ch & 0x0f;
        else
            cpt = ch & 0x07;
        ++text;
        if (((*text & 0xc0) != 0x80) && (cpt <= 0x10ffff)) {
            *wide = cpt;
            ++wide;
        }
    }
    *wide = 0;

    return ctx->wide;
}

/**
 * Get width of current glyph plus right gap.
 * @param ctx font context
 * @return width in pixels
 */
static inline size_t glyph_width(const struct font* ctx)
{
    const FT_GlyphSlot glyph = ctx->font_face->glyph;
    const FT_Bitmap* bitmap = &glyph->bitmap;
    return bitmap->width + config.font_size / GLYPH_GW_REL;
}

/**
 * Draw glyph on window buffer.
 * @param ctx font context
 * @param wnd_buf window buffer
 * @param wnd_size window buffer size
 * @param pos top-left coordinates of the glyph
 * @return false if whole glyph out of window
 */
static bool draw_glyph(const struct font* ctx, argb_t* wnd_buf,
                       const struct size* wnd_size, const struct point* pos)
{
    const FT_GlyphSlot glyph = ctx->font_face->glyph;
    const FT_Bitmap* bitmap = &glyph->bitmap;
    const size_t fheight = font_height(ctx);

    for (size_t y = 0; y < bitmap->rows; ++y) {
        argb_t* wnd_line;
        const uint8_t* glyph_line;
        const size_t wnd_y = pos->y + y + fheight - glyph->bitmap_top;

        if (wnd_y >= wnd_size->height && y == 0) {
            return false;
        }
        wnd_line = &wnd_buf[wnd_y * wnd_size->width];
        glyph_line = &bitmap->buffer[y * bitmap->width];

        for (size_t x = 0; x < bitmap->width; ++x) {
            const uint8_t alpha = glyph_line[x];
            const size_t wnd_x = pos->x + glyph->bitmap_left + x;
            if (wnd_x < wnd_size->width && alpha) {
                argb_t* wnd_pixel = &wnd_line[wnd_x];
                const argb_t bg = *wnd_pixel;
                const argb_t fg = config.font_color;
                *wnd_pixel = ARGB_ALPHA_BLEND(alpha, 0xff, bg, fg);
            }
        }
    }

    return true;
}

struct font* font_init(void)
{
    struct font* ctx;
    char font_file[256];

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    if (search_font_file(config.font_face, font_file, sizeof(font_file)) &&
        FT_Init_FreeType(&ctx->font_lib) == 0 &&
        FT_New_Face(ctx->font_lib, font_file, 0, &ctx->font_face) == 0) {
        font_scale(ctx, 1);
    }

    return ctx;
}

void font_free(struct font* ctx)
{
    if (ctx) {
        if (ctx->font_face) {
            FT_Done_Face(ctx->font_face);
        }
        if (ctx->font_lib) {
            FT_Done_FreeType(ctx->font_lib);
        }
        free(ctx->wide);
        free(ctx);
    }
}

void font_scale(struct font* ctx, size_t scale)
{
    if (ctx->font_face) {
        const FT_F26Dot6 size = config.font_size * scale * 64;
        FT_Set_Char_Size(ctx->font_face, size, 0, 96, 0);
    }
}

size_t font_height(const struct font* ctx)
{
    return ctx->font_face
        ? ctx->font_face->size->metrics.y_ppem + config.font_size / 4
        : 0;
}

size_t font_text_width(struct font* ctx, const char* text, size_t len)
{
    size_t width = 0;
    const wchar_t* wtext = to_wide(ctx, text, len);
    const size_t wlen = wtext ? wcslen(wtext) : 0;

    for (size_t i = 0; i < wlen; ++i) {
        if (wtext[i] == L' ') {
            width += font_height(ctx) / SPACE_WH_REL;
        } else if (FT_Load_Char(ctx->font_face, wtext[i], FT_LOAD_DEFAULT) ==
                   0) {
            width += glyph_width(ctx);
        }
    }

    return width;
}

size_t font_print(struct font* ctx, argb_t* wnd_buf,
                  const struct size* wnd_size, const struct point* pos,
                  const char* text, size_t len)
{
    struct point pen = *pos;
    const wchar_t* wtext = to_wide(ctx, text, len);
    const size_t wlen = wtext ? wcslen(wtext) : 0;

    for (size_t i = 0; i < wlen; ++i) {
        if (wtext[i] == L' ') {
            pen.x += font_height(ctx) / SPACE_WH_REL;
        } else if (FT_Load_Char(ctx->font_face, wtext[i], FT_LOAD_RENDER) ==
                   0) {
            if (!draw_glyph(ctx, wnd_buf, wnd_size, &pen)) {
                break;
            }
            pen.x += glyph_width(ctx);
        }
    }

    return pen.x - pos->x;
}

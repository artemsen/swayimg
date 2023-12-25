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

// Section name in the config file
#define CONFIG_SECTION "font"

// Defaults
#define DEFALT_FONT  "monospace"
#define DEFALT_COLOR 0x00cccccc
#define DEFALT_SIZE  14
#define DEFALT_SCALE 1

/** Font context. */
struct font {
    FT_Library lib; ///< Font lib instance
    FT_Face face;   ///< Font face instance
    char* name;     ///< Font face name
    argb_t color;   ///< Font color
    size_t size;    ///< Base font size (pt)
    size_t scale;   ///< Scale factor (HiDPI)
};
static struct font ctx;

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
 * Load font.
 * @return false if font wasn't initialized
 */
static bool lazy_load(void)
{
    bool rc = ctx.face != NULL;

    if (!rc) {
        char file[256];
        rc = search_font_file(ctx.name, file, sizeof(file)) &&
            FT_Init_FreeType(&ctx.lib) == 0 &&
            FT_New_Face(ctx.lib, file, 0, &ctx.face) == 0;
    }
    if (rc) {
        const FT_F26Dot6 size = ctx.size * ctx.scale * 64;
        FT_Set_Char_Size(ctx.face, size, 0, 96, 0);
    }

    return rc;
}

/**
 * Convert utf-8 string to wide char format.
 * @param text source string to encode
 * @param len length of the input string, 0 for auto
 * @return pointer to wide string, called must free it
 */
static wchar_t* to_wide(const char* text, size_t len)
{
    size_t ansi_sz = len ? len : strlen(text);
    const size_t wide_sz = (ansi_sz + 1 /*last null*/) * sizeof(wchar_t);
    wchar_t* wide;
    wchar_t* ptr;

    wide = malloc(wide_sz);
    if (!wide) {
        return NULL;
    }
    ptr = wide;

    while (*text && ansi_sz--) {
        wchar_t cpt = 0;
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
            *ptr = cpt;
            ++ptr;
        }
    }
    *ptr = 0; // last null

    return wide;
}

/**
 * Draw glyph on window buffer.
 * @param wnd_buf window buffer
 * @param wnd_size window buffer size
 * @param x,y top-left coordinates of the glyph
 */
static void draw_glyph(argb_t* wnd_buf, const struct size* wnd_size, ssize_t x1,
                       ssize_t y1)
{
    const FT_GlyphSlot glyph = ctx.face->glyph;
    const FT_Bitmap* bitmap = &glyph->bitmap;
    const size_t fheight = font_height();

    for (size_t y = 0; y < bitmap->rows; ++y) {
        argb_t* wnd_line;
        const uint8_t* glyph_line;
        const size_t wnd_y = y1 + y + fheight - glyph->bitmap_top;

        if (wnd_y >= wnd_size->height && y == 0) {
            return; // out of window
        }
        wnd_line = &wnd_buf[wnd_y * wnd_size->width];
        glyph_line = &bitmap->buffer[y * bitmap->width];

        for (size_t x = 0; x < bitmap->width; ++x) {
            const uint8_t alpha = glyph_line[x];
            const size_t wnd_x = x1 + glyph->bitmap_left + x;
            if (wnd_x < wnd_size->width && alpha) {
                argb_t* wnd_pixel = &wnd_line[wnd_x];
                const argb_t bg = *wnd_pixel;
                const argb_t fg = ctx.color;
                *wnd_pixel = ARGB_ALPHA_BLEND(alpha, 0xff, bg, fg);
            }
        }
    }
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static bool load_config(const char* key, const char* value)
{
    if (strcmp(key, "name") == 0) {
        const size_t sz = strlen(value) + 1;
        char* name = realloc(ctx.name, sz);
        if (ctx.name) {
            ctx.name = name;
            memcpy(ctx.name, value, sz);
        }
        return true;
    } else if (strcmp(key, "size") == 0) {
        ssize_t num;
        if (!config_parse_num(value, &num, 0) || num <= 0 || num > 1024) {
            return false;
        }
        ctx.size = num;
        return true;
    } else if (strcmp(key, "color") == 0) {
        argb_t color;
        if (!config_parse_color(value, &color)) {
            return false;
        }
        ctx.color = color;
        return true;
    }
    return false;
}

void font_init(void)
{
    // set defaults
    const char* default_font = DEFALT_FONT;
    const size_t sz = strlen(default_font) + 1;
    ctx.name = malloc(sz);
    if (ctx.name) {
        memcpy(ctx.name, default_font, sz);
    }
    ctx.color = DEFALT_COLOR;
    ctx.size = DEFALT_SIZE;
    ctx.scale = DEFALT_SCALE;

    // register configuration loader
    config_add_section(CONFIG_SECTION, load_config);
}

void font_free(void)
{
    if (ctx.face) {
        FT_Done_Face(ctx.face);
    }
    if (ctx.lib) {
        FT_Done_FreeType(ctx.lib);
    }
    free(ctx.name);
}

void font_set_scale(size_t scale)
{
    ctx.scale = scale;
}

size_t font_height(void)
{
    return lazy_load() ? ctx.face->size->metrics.y_ppem + ctx.size / 4 : 0;
}

size_t font_print(argb_t* wnd_buf, const struct size* wnd_size,
                  const struct point* pos, const char* text, size_t len)
{
    wchar_t* wide;
    size_t wlen;
    size_t width = 0;

    if (!lazy_load()) {
        return 0;
    }

    wide = to_wide(text, len);
    if (!wide) {
        return 0;
    }
    wlen = wcslen(wide);

    for (size_t i = 0; i < wlen; ++i) {
        if (wide[i] == L' ') {
            width += font_height() / SPACE_WH_REL;
        } else if (FT_Load_Char(ctx.face, wide[i], FT_LOAD_RENDER) == 0) {
            const size_t glyph_width =
                ctx.face->glyph->bitmap.width + ctx.size / GLYPH_GW_REL;
            if (wnd_buf && wnd_size && pos) {
                draw_glyph(wnd_buf, wnd_size, pos->x + width, pos->y);
            }
            width += glyph_width;
        }
    }

    free(wide);

    return width;
}

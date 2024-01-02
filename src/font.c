// SPDX-License-Identifier: MIT
// Text renderer.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "font.h"

#include "config.h"
#include "str.h"

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

        if (wnd_y >= wnd_size->height) {
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
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void font_init(void)
{
    // set defaults
    str_dup(DEFALT_FONT, &ctx.name);
    ctx.color = DEFALT_COLOR;
    ctx.size = DEFALT_SIZE;
    ctx.scale = DEFALT_SCALE;

    // register configuration loader
    config_add_loader(CONFIG_SECTION, load_config);
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
                  const struct point* pos, const wchar_t* text)
{
    size_t width = 0;

    if (!text || !lazy_load()) {
        return 0;
    }

    while (*text) {
        if (*text == L' ') {
            width += font_height() / SPACE_WH_REL;
        } else if (FT_Load_Char(ctx.face, *text, FT_LOAD_RENDER) == 0) {
            const size_t glyph_width =
                ctx.face->glyph->bitmap.width + ctx.size / GLYPH_GW_REL;
            if (wnd_buf && wnd_size && pos) {
                draw_glyph(wnd_buf, wnd_size, pos->x + width, pos->y);
            }
            width += glyph_width;
        }
        ++text;
    }

    return width;
}

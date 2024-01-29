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

#define NO_SHADOW 0xff000000

// Section name in the config file
#define CONFIG_SECTION "font"

// Defaults
#define DEFALT_FONT   "monospace"
#define DEFALT_COLOR  0x00cccccc
#define DEFALT_SHADOW 0x00000000
#define DEFALT_SIZE   14
#define DEFALT_SCALE  1

/** Font context. */
struct font {
    FT_Library lib; ///< Font lib instance
    FT_Face face;   ///< Font face instance
    char* name;     ///< Font face name
    argb_t color;   ///< Font color
    argb_t shadow;  ///< Font shadow color
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
        if (rc) {
            const FT_F26Dot6 size = ctx.size * ctx.scale * 64;
            FT_Set_Char_Size(ctx.face, size, 0, 96, 0);
        }
    }

    return rc;
}

/**
 * Draw current glyph on pixmap.
 * @param pm destination pixmap
 * @param x,y top-left coordinates of the glyph
 * @param color color of the font
 */
static void draw_glyph(struct pixmap* pm, size_t x, size_t y, argb_t color)
{
    const FT_GlyphSlot glyph = ctx.face->glyph;
    const FT_Bitmap* bmp = &glyph->bitmap;

    y += font_height() - glyph->bitmap_top;
    x += glyph->bitmap_left;

    pixmap_apply_mask(pm, x, y, bmp->buffer, bmp->width, bmp->rows, color);
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
            ctx.shadow = NO_SHADOW;
            status = cfgst_ok;
        } else if (config_to_color(value, &ctx.shadow)) {
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

size_t font_print(struct pixmap* pm, size_t x, size_t y, const wchar_t* text)
{
    size_t width = 0;
    const size_t tracking = ctx.size / GLYPH_GW_REL;
    const size_t space_width = font_height() / SPACE_WH_REL;
    const size_t shadow_shift = ctx.size / 10;

    if (!text || !lazy_load()) {
        return 0;
    }

    while (*text) {
        if (*text == L' ') {
            width += space_width;
        } else if (FT_Load_Char(ctx.face, *text, FT_LOAD_RENDER) == 0) {
            const size_t glyph_width = ctx.face->glyph->bitmap.width + tracking;
            if (pm) {
                const size_t letter_x = x + width;
                if (ctx.shadow != NO_SHADOW) {
                    draw_glyph(pm, letter_x + shadow_shift, y + shadow_shift,
                               ctx.shadow);
                }
                draw_glyph(pm, letter_x, y, ctx.color);
            }
            width += glyph_width;
        }
        ++text;
    }

    return width;
}

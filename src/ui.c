// SPDX-License-Identifier: MIT
// User interface: Window management, keyboard input, etc.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "ui.h"

#include "buildcfg.h"
#include "compositor.h"
#include "uiface.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/** Global UI context. */
static void* ui_ctx;
/** Global UI handlers. */
static struct ui ui_handlers;

/**
 * Initialize Wayland UI.
 * @param cfg config instance
 * @param img first image instance
 * @param handlers UI handlers
 * @return UI context or NULL on errors
 */
static void* init_wayland(const struct config* cfg, const struct image* img,
                          struct ui* handlers)
{
    const struct config* general = config_section(cfg, CFG_GENERAL);
    struct wndrect wnd = { .x = SSIZE_MAX,
                           .y = SSIZE_MAX,
                           .width = UI_WINDOW_DEFAULT_WIDTH,
                           .height = UI_WINDOW_DEFAULT_HEIGHT };
    char* app_id = NULL;
    bool decoration;
    const char* value;
    void* ctx;

    // initial window position
    value = config_get(general, CFG_GNRL_POSITION);
    if (strcmp(value, CFG_AUTO) != 0) {
        struct str_slice slices[2];
        ssize_t x, y;
        if (str_split(value, ',', slices, 2) == 2 &&
            str_to_num(slices[0].value, slices[0].len, &x, 0) &&
            str_to_num(slices[1].value, slices[1].len, &y, 0)) {
            wnd.x = x;
            wnd.y = y;
        } else {
            config_error_val(general->name, CFG_GNRL_POSITION);
        }
    }

    // initial window size
    value = config_get(general, CFG_GNRL_SIZE);
    if (strcmp(value, CFG_FULLSCREEN) == 0) {
        wnd.width = UI_WINDOW_FULLSCREEN;
        wnd.height = UI_WINDOW_FULLSCREEN;
    } else if (strcmp(value, CFG_FROM_IMAGE) == 0) {
        struct imgframe* frame = arr_nth(img->data->frames, 0);
        wnd.width = frame->pm.width;
        wnd.height = frame->pm.height;
    } else {
        ssize_t width, height;
        struct str_slice slices[2];
        if (str_split(value, ',', slices, 2) == 2 &&
            str_to_num(slices[0].value, slices[0].len, &width, 0) &&
            str_to_num(slices[1].value, slices[1].len, &height, 0) &&
            width > UI_WINDOW_MIN && width < UI_WINDOW_MAX &&
            height > UI_WINDOW_MIN && height < UI_WINDOW_MAX) {
            wnd.width = width;
            wnd.height = height;
        } else {
            config_error_val(general->name, CFG_GNRL_SIZE);
        }
    }

    // app id (class name)
    value = config_get(general, CFG_GNRL_APP_ID);
    if (!*value) {
        config_error_val(CFG_GENERAL, CFG_GNRL_APP_ID);
        value = config_get_default(CFG_GENERAL, CFG_GNRL_APP_ID);
    }
    str_dup(value, &app_id);

    // window decoration (title/borders/...)
    decoration = config_get_bool(general, CFG_GNRL_DECOR);

    if (wnd.width != UI_WINDOW_FULLSCREEN &&
        wnd.height != UI_WINDOW_FULLSCREEN) {
        // setup window position and size
#ifdef HAVE_COMPOSITOR
        if (config_get_bool(general, CFG_GNRL_OVERLAY)) {
            compositor_get_focus(&wnd);
        }
#endif // HAVE_COMPOSITOR

        // limit window size
        if (wnd.width < UI_WINDOW_MIN || wnd.height < UI_WINDOW_MIN ||
            wnd.width > UI_WINDOW_MAX || wnd.height > UI_WINDOW_MAX) {
            wnd.width = UI_WINDOW_DEFAULT_WIDTH;
            wnd.height = UI_WINDOW_DEFAULT_HEIGHT;
        }

#ifdef HAVE_COMPOSITOR
        if (wnd.x != SSIZE_MAX && wnd.y != SSIZE_MAX) {
            compositor_overlay(&wnd, &app_id);
        }
#endif // HAVE_COMPOSITOR
    }

    ctx = ui_init_wl(app_id, wnd.width, wnd.height, decoration, handlers);

    free(app_id);
    return ctx;
}

bool ui_init(const struct config* cfg, const struct image* img)
{
    ui_ctx = init_wayland(cfg, img, &ui_handlers);
    return !!ui_ctx;
}

void ui_destroy(void)
{
    if (ui_ctx) {
        ui_handlers.free(ui_ctx);
    }
    ui_ctx = NULL;
}

void ui_event_prepare(void)
{
    if (ui_handlers.event_prep) {
        ui_handlers.event_prep(ui_ctx);
    }
}

void ui_event_done(void)
{
    if (ui_handlers.event_done) {
        ui_handlers.event_done(ui_ctx);
    }
}

struct pixmap* ui_draw_begin(void)
{
    return ui_handlers.draw_begin(ui_ctx);
}

void ui_draw_commit(void)
{
    ui_handlers.draw_commit(ui_ctx);
}

void ui_set_title(const char* name)
{
    if (ui_handlers.set_title) {
        ui_handlers.set_title(ui_ctx, name);
    }
}

void ui_set_cursor(enum ui_cursor shape)
{
    if (ui_handlers.set_cursor) {
        ui_handlers.set_cursor(ui_ctx, shape);
    }
}

void ui_set_ctype(enum ui_ctype ctype)
{
    if (ui_handlers.set_ctype) {
        ui_handlers.set_ctype(ui_ctx, ctype);
    }
}

size_t ui_get_width(void)
{
    return ui_handlers.get_width(ui_ctx);
}

size_t ui_get_height(void)
{
    return ui_handlers.get_height(ui_ctx);
}

void ui_toggle_fullscreen(void)
{
    if (ui_handlers.toggle_fullscreen) {
        ui_handlers.toggle_fullscreen(ui_ctx);
    }
}

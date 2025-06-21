// SPDX-License-Identifier: MIT
// Viewport: displaying part of an image on the surface of a window.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "viewport.h"

#include "application.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

// Convert Id to special color value
#define ID_TO_ARGB(n) ARGB(0, 0xee, 0xba, 0xbe + (n))

// Window background modes
#define BKGMODE_AUTO        ID_TO_ARGB(0)
#define BKGMODE_AUTO_NAME   "auto"
#define BKGMODE_EXTEND      ID_TO_ARGB(1)
#define BKGMODE_EXTEND_NAME "extend"
#define BKGMODE_MIRROR      ID_TO_ARGB(2)
#define BKGMODE_MIRROR_NAME "mirror"

// Background grid parameters
#define GRID_NAME   "grid"
#define GRID_BKGID  ID_TO_ARGB(0)
#define GRID_STEP   10
#define GRID_COLOR1 ARGB(0xff, 0x33, 0x33, 0x33)
#define GRID_COLOR2 ARGB(0xff, 0x4c, 0x4c, 0x4c)

// Scale thresholds
#define MIN_SCALE 10    // pixels
#define MAX_SCALE 100.0 // factor

// clang-format off
static const char* scale_names[] = {
    [vp_scale_fit_optimal] = "optimal",
    [vp_scale_fit_window] = "fit",
    [vp_scale_fit_width] = "width",
    [vp_scale_fit_height] = "height",
    [vp_scale_fill_window] = "fill",
    [vp_scale_real_size] = "real",
    [vp_scale_keep_zoom] = "keep",
};
// clang-format on

// clang-format off
static const char* position_names[] = {
    [vp_pos_free] = "free",
    [vp_pos_center] = "center",
    [vp_pos_top] = "top",
    [vp_pos_bottom] = "bottom",
    [vp_pos_left] = "left",
    [vp_pos_right] = "right",
    [vp_pos_tl] = "top_left",
    [vp_pos_tr] = "top_right",
    [vp_pos_bl] = "bottom_left",
    [vp_pos_br] = "bottom_right",
};
// clang-format on

/**
 * Fix up image position.
 * @param vp viewport context
 * @param force flag to force update position
 */
static void fixup_position(struct viewport* vp, bool force)
{
    const struct pixmap* pm = viewport_pixmap(vp);
    const size_t img_width = vp->scale * pm->width;
    const size_t img_height = vp->scale * pm->height;

    if (force || (img_width <= vp->width && vp->def_pos != vp_pos_free)) {
        switch (vp->def_pos) {
            case vp_pos_free:
                vp->x = vp->width / 2 - img_width / 2;
                break;
            case vp_pos_top:
                vp->x = vp->width / 2 - img_width / 2;
                break;
            case vp_pos_center:
                vp->x = vp->width / 2 - img_width / 2;
                break;
            case vp_pos_bottom:
                vp->x = vp->width / 2 - img_width / 2;
                break;
            case vp_pos_left:
                vp->x = 0;
                break;
            case vp_pos_right:
                vp->x = vp->width - img_width;
                break;
            case vp_pos_tl:
                vp->x = 0;
                break;
            case vp_pos_tr:
                vp->x = vp->width - img_width;
                break;
            case vp_pos_bl:
                vp->x = 0;
                break;
            case vp_pos_br:
                vp->x = vp->width - img_width;
                break;
        }
    }
    if (force || (img_height <= vp->height && vp->def_pos != vp_pos_free)) {
        switch (vp->def_pos) {
            case vp_pos_free:
                vp->y = vp->height / 2 - img_height / 2;
                break;
            case vp_pos_top:
                vp->y = 0;
                break;
            case vp_pos_center:
                vp->y = vp->height / 2 - img_height / 2;
                break;
            case vp_pos_bottom:
                vp->y = vp->height - img_height;
                break;
            case vp_pos_left:
                vp->y = vp->height / 2 - img_height / 2;
                break;
            case vp_pos_right:
                vp->y = vp->height / 2 - img_height / 2;
                break;
            case vp_pos_tl:
                vp->y = 0;
                break;
            case vp_pos_tr:
                vp->y = 0;
                break;
            case vp_pos_bl:
                vp->y = vp->height - img_height;
                break;
            case vp_pos_br:
                vp->y = vp->height - img_height;
                break;
        }
    }

    if (vp->def_pos != vp_pos_free) {
        // bind to window border
        if (vp->x > 0 && vp->x + img_width > vp->width) {
            vp->x = 0;
        }
        if (vp->y > 0 && vp->y + img_height > vp->height) {
            vp->y = 0;
        }
        if (vp->x < 0 && vp->x + img_width < vp->width) {
            vp->x = vp->width - img_width;
        }
        if (vp->y < 0 && vp->y + img_height < vp->height) {
            vp->y = vp->height - img_height;
        }
    }

    // don't let canvas to be far out of window
    if (vp->x + (ssize_t)img_width < 0) {
        vp->x = -img_width;
    }
    if (vp->x > (ssize_t)vp->width) {
        vp->x = vp->width;
    }
    if (vp->y + (ssize_t)img_height < 0) {
        vp->y = -img_height;
    }
    if (vp->y > (ssize_t)vp->height) {
        vp->y = vp->height;
    }
}

/**
 * Set fixed scale for current image.
 * @param vp viewport context
 * @param scale fixed scale mode to set
 */
static void scale_fixed(struct viewport* vp, enum vp_scale scale)
{
    const struct pixmap* pm = viewport_pixmap(vp);
    const double ratio_w = (double)vp->width / pm->width;
    const double ratio_h = (double)vp->height / pm->height;
    double factor = 1.0;

    switch (scale) {
        case vp_scale_keep_zoom:
        case vp_scale_fit_optimal:
            factor = min(ratio_w, ratio_h);
            if (factor > 1.0) {
                factor = 1.0;
            }
            break;
        case vp_scale_fit_window:
            factor = min(ratio_w, ratio_h);
            break;
        case vp_scale_fit_width:
            factor = ratio_w;
            break;
        case vp_scale_fit_height:
            factor = ratio_h;
            break;
        case vp_scale_fill_window:
            factor = max(ratio_w, ratio_h);
            break;
        case vp_scale_real_size:
            factor = 1.0; // 100 %
            break;
    }

    viewport_scale_abs(vp, factor);
}

/** Animation timer handler. */
static void on_animation_timer(void* data)
{
    struct viewport* vp = data;
    viewport_frame(vp, true);
    vp->animation_cb();
    viewport_anim_ctl(vp, vp_actl_start); // restart timer
}

void viewport_init(struct viewport* vp, const struct config* section)
{
    const char* value;

    vp->aa_en = true;
    vp->aa = aa_mks13;
    if (!aa_from_name(config_get(section, CFG_VIEW_AA), &vp->aa)) {
        const char* def = config_get_default(section->name, CFG_VIEW_AA);
        aa_from_name(def, &vp->aa);
        config_error_val(section->name, CFG_VIEW_AA);
    }

    // window background
    value = config_get(section, CFG_VIEW_WINDOW);
    if (strcmp(value, BKGMODE_AUTO_NAME) == 0) {
        vp->bkg_window = BKGMODE_AUTO;
    } else if (strcmp(value, BKGMODE_EXTEND_NAME) == 0) {
        vp->bkg_window = BKGMODE_EXTEND;
    } else if (strcmp(value, BKGMODE_MIRROR_NAME) == 0) {
        vp->bkg_window = BKGMODE_MIRROR;
    } else {
        vp->bkg_window = config_get_color(section, CFG_VIEW_WINDOW);
    }

    // background for transparent images
    value = config_get(section, CFG_VIEW_TRANSP);
    if (strcmp(value, GRID_NAME) == 0) {
        vp->bkg_transp = GRID_BKGID;
    } else {
        vp->bkg_transp = config_get_color(section, CFG_VIEW_TRANSP);
    }

    // default position and scale
    vp->def_pos = config_get_oneof(section, CFG_VIEW_POSITION, position_names,
                                   ARRAY_SIZE(position_names));
    vp->def_scale = config_get_oneof(section, CFG_VIEW_SCALE, scale_names,
                                     ARRAY_SIZE(scale_names));

    // setup animation timer
    vp->animation_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (vp->animation_fd != -1) {
        app_watch(vp->animation_fd, on_animation_timer, vp);
    }
}

void viewport_free(struct viewport* vp)
{
    if (vp->animation_fd != -1) {
        close(vp->animation_fd);
    }
}

void viewport_reset(struct viewport* vp, struct image* img)
{
    const struct pixmap* pm = viewport_pixmap(vp);
    const size_t prev_w = pm ? pm->width : 0;
    const size_t prev_h = pm ? pm->height : 0;

    vp->image = img;
    vp->frame = 0;

    pm = viewport_pixmap(vp);
    if (vp->def_scale != vp_scale_keep_zoom) {
        scale_fixed(vp, vp->def_scale);
        fixup_position(vp, true);
    } else {
        if (vp->scale == 0) {
            scale_fixed(vp, vp_scale_fit_optimal);
            fixup_position(vp, true);
        } else {
            const ssize_t diff_w = prev_w - pm->width;
            const ssize_t diff_h = prev_h - pm->height;
            vp->x += floor(vp->scale * diff_w) / 2.0;
            vp->y += floor(vp->scale * diff_h) / 2.0;
            fixup_position(vp, false);
        }
    }

    viewport_anim_ctl(vp, vp_actl_start); // restart animation
}

void viewport_resize(struct viewport* vp, size_t width, size_t height)
{
    vp->width = width;
    vp->height = height;
    if (vp->image) {
        scale_fixed(vp, vp->def_scale);
        fixup_position(vp, false);
    }
}

void viewport_frame(struct viewport* vp, bool forward)
{
    const size_t total_frames = vp->image->data->frames->size;
    if (forward) {
        if (++vp->frame >= total_frames) {
            vp->frame = 0;
        }
    } else {
        if (vp->frame == 0) {
            vp->frame = total_frames - 1;
        } else {
            --vp->frame;
        }
    }
}

void viewport_move(struct viewport* vp, enum vp_move dir, size_t px)
{
    switch (dir) {
        case vp_move_up:
            vp->y -= px;
            break;
        case vp_move_down:
            vp->y += px;
            break;
        case vp_move_left:
            vp->x -= px;
            break;
        case vp_move_right:
            vp->x += px;
            break;
    }
    fixup_position(vp, false);
}

void viewport_rotate(struct viewport* vp)
{
    const struct pixmap* pm = viewport_pixmap(vp);
    const ssize_t diff = (ssize_t)pm->width - pm->height;
    const ssize_t shift = (vp->scale * diff) / 2;

    vp->x -= shift;
    vp->y += shift;

    fixup_position(vp, false);
}

bool viewport_scale_def(struct viewport* vp, const char* scale)
{
    const ssize_t index = str_index(scale_names, scale, 0);
    const bool rc = (index >= 0);
    if (rc) {
        vp->def_scale = index;
        scale_fixed(vp, vp->def_scale);
        fixup_position(vp, true);
    }
    return rc;
}

const char* viewport_scale_switch(struct viewport* vp)
{
    enum vp_scale scale = vp->def_scale;
    if (++scale > vp_scale_keep_zoom) {
        scale = vp_scale_fit_optimal;
    }
    vp->def_scale = scale;
    scale_fixed(vp, scale);
    fixup_position(vp, true);
    return scale_names[scale];
}

void viewport_scale_abs(struct viewport* vp, double scale)
{
    const struct pixmap* pm = viewport_pixmap(vp);

    // save center
    const double wnd_half_w = (double)vp->width / 2.0;
    const double wnd_half_h = (double)vp->height / 2.0;
    const double center_x = wnd_half_w / vp->scale - vp->x / vp->scale;
    const double center_y = wnd_half_h / vp->scale - vp->y / vp->scale;

    // check scale limits
    if (scale > MAX_SCALE) {
        scale = MAX_SCALE;
    } else {
        const double scale_w = (double)MIN_SCALE / pm->width;
        const double scale_h = (double)MIN_SCALE / pm->height;
        const double scale_min = max(scale_w, scale_h);
        if (scale < scale_min) {
            scale = scale_min;
        }
    }

    vp->scale = scale;

    // restore center
    vp->x = wnd_half_w - center_x * vp->scale;
    vp->y = wnd_half_h - center_y * vp->scale;

    fixup_position(vp, false);
}

void viewport_anim_ctl(struct viewport* vp, enum vp_actl op)
{
    if (vp->animation_fd != -1) {
        struct itimerspec ts = { 0 };
        if (op == vp_actl_start) {
            const struct imgframe* frame =
                arr_nth(vp->image->data->frames, vp->frame);
            if (vp->image->data->frames->size > 1 && frame->duration) {
                ts.it_value.tv_sec = frame->duration / 1000;
                ts.it_value.tv_nsec = (frame->duration % 1000) * 1000000;
            }
        }
        timerfd_settime(vp->animation_fd, 0, &ts, NULL);
    }
}

bool viewport_anim_stat(const struct viewport* vp)
{
    struct itimerspec ts;
    return vp->animation_fd != -1 &&
        timerfd_gettime(vp->animation_fd, &ts) == 0 &&
        (ts.it_value.tv_sec || ts.it_value.tv_nsec);
}

const struct pixmap* viewport_pixmap(const struct viewport* vp)
{
    const struct pixmap* pm = NULL;
    if (vp->image) {
        assert(vp->frame < vp->image->data->frames->size);
        const struct imgframe* frame =
            arr_nth(vp->image->data->frames, vp->frame);
        pm = &frame->pm;
    }
    return pm;
}

void viewport_draw(const struct viewport* vp, struct pixmap* wnd)
{
    const struct pixmap* pm = viewport_pixmap(vp);
    const size_t width = vp->scale * pm->width;
    const size_t height = vp->scale * pm->height;

    // clear image background
    if (pm->format == pixmap_argb) {
        if (vp->bkg_transp == GRID_BKGID) {
            pixmap_grid(wnd, vp->x, vp->y, width, height, GRID_STEP,
                        GRID_COLOR1, GRID_COLOR2);
        } else {
            pixmap_fill(wnd, vp->x, vp->y, width, height, vp->bkg_transp);
        }
    }

    // put image on window surface
    image_render(vp->image, vp->frame, vp->aa_en ? vp->aa : aa_nearest,
                 vp->scale, true, vp->x, vp->y, wnd);

    // set window background
    switch (vp->bkg_window) {
        case BKGMODE_AUTO:
            if (width > height) {
                pixmap_bkg_mirror(wnd, vp->x, vp->y, width, height);
            } else {
                pixmap_bkg_extend(wnd, vp->x, vp->y, width, height);
            }
            break;
        case BKGMODE_EXTEND:
            pixmap_bkg_extend(wnd, vp->x, vp->y, width, height);
            break;
        case BKGMODE_MIRROR:
            pixmap_bkg_mirror(wnd, vp->x, vp->y, width, height);
            break;
        default:
            pixmap_inverse_fill(wnd, vp->x, vp->y, width, height,
                                vp->bkg_window);
    }
}

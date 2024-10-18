// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "pixmap.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

/** Scale filter parameters. */
struct scale_param {
    const struct pixmap* src; ///< Source pixmap
    struct pixmap* dst;       ///< Destination pixmap
    ssize_t x;                ///< Left offset on destination pixmap
    ssize_t y;                ///< Top offset on destination pixmap
    float scale;              ///< Scale factor
    bool alpha;               ///< Flag to use alpha blending
};

/**
 * Scale handler.
 * @param sp pointer to scaling params
 * @param start index of the first line to draw
 * @param step number of lines to skip on each iteration
 */
typedef void (*scale_fn)(struct scale_param* sp, size_t start, size_t step);

/** Scale filter thread task. */
struct scale_task {
    pthread_t tid;          ///< Thread id
    struct scale_param* sp; ///< Scaling parameters
    size_t start;           ///< Index of the first line to draw
    size_t step;            ///< Number of lines to skip on each iteration
    scale_fn fn;            ///< Scale function
};

/** Background thread scaler handler. */
static void* scale_thread(void* data)
{
    const struct scale_task* task = data;
    task->fn(task->sp, task->start, task->step);
    return NULL;
}

/**
 * Alpha blending.
 * @param img color of image's pixel
 * @param wnd pointer to window buffer to put pixel
 */
static inline void alpha_blend(argb_t src, argb_t* dst)
{
    const uint8_t ai = ARGB_GET_A(src);

    if (ai != 0xff) {
        const argb_t wp = *dst;
        const uint8_t aw = ARGB_GET_A(wp);
        const uint8_t target_alpha = max(ai, aw);
        const argb_t inv = 255 - ai;
        src = ARGB_SET_A(target_alpha) |
            ARGB_SET_R((ai * ARGB_GET_R(src) + inv * ARGB_GET_R(wp)) / 255) |
            ARGB_SET_G((ai * ARGB_GET_G(src) + inv * ARGB_GET_G(wp)) / 255) |
            ARGB_SET_B((ai * ARGB_GET_B(src) + inv * ARGB_GET_B(wp)) / 255);
    }

    *dst = src;
}

/** Nearest scale filter, see `scale_fn` for details. */
static void scale_nearest(struct scale_param* sp, size_t start, size_t step)
{
    const ssize_t left = max(0, sp->x);
    const ssize_t top = max(0, sp->y);
    const ssize_t right =
        min((ssize_t)sp->dst->width, sp->x + sp->scale * sp->src->width);
    const ssize_t bottom =
        min((ssize_t)sp->dst->height, sp->y + sp->scale * sp->src->height);

    const ssize_t delta_x = left - sp->x;
    const ssize_t delta_y = top - sp->y;

    for (ssize_t dst_y = top + start; dst_y < bottom; dst_y += step) {
        const size_t src_y = (float)(dst_y - top + delta_y) / sp->scale;
        const argb_t* src_line = &sp->src->data[src_y * sp->src->width];
        argb_t* dst_line = &sp->dst->data[dst_y * sp->dst->width];

        for (ssize_t dst_x = left; dst_x < right; ++dst_x) {
            const size_t src_x = (float)(dst_x - left + delta_x) / sp->scale;
            const argb_t color = src_line[src_x];

            if (sp->alpha) {
                alpha_blend(color, &dst_line[dst_x]);
            } else {
                dst_line[dst_x] = ARGB_SET_A(0xff) | color;
            }
        }
    }
}

/** Average scale filter, see `scale_fn` for details. */
static void scale_average(struct scale_param* sp, size_t start, size_t step)
{
    const ssize_t left = max(0, sp->x);
    const ssize_t top = max(0, sp->y);
    const ssize_t right =
        min((ssize_t)sp->dst->width, sp->x + sp->scale * sp->src->width);
    const ssize_t bottom =
        min((ssize_t)sp->dst->height, sp->y + sp->scale * sp->src->height);

    const ssize_t iscale = (1.0 / sp->scale) / 2;
    const ssize_t delta_x = left - sp->x;
    const ssize_t delta_y = top - sp->y;

    for (ssize_t dst_y = top + start; dst_y < bottom; dst_y += step) {
        ssize_t src_y = (double)(dst_y - top + delta_y) / sp->scale;
        const ssize_t src_y0 = max(0, src_y - iscale);
        const ssize_t src_y1 = min((ssize_t)sp->src->height, src_y + iscale);
        argb_t* dst_line = &sp->dst->data[dst_y * sp->dst->width];

        for (ssize_t dst_x = left; dst_x < right; ++dst_x) {
            ssize_t src_x = (double)(dst_x - left + delta_x) / sp->scale;
            const ssize_t src_x0 = max(0, src_x - iscale);
            const ssize_t src_x1 = min((ssize_t)sp->src->width, src_x + iscale);

            size_t a = 0, r = 0, g = 0, b = 0;
            size_t count = 0;

            for (src_y = src_y0; src_y <= src_y1; ++src_y) {
                for (src_x = src_x0; src_x <= src_x1; ++src_x) {
                    const argb_t color =
                        sp->src->data[src_y * sp->src->width + src_x];
                    a += ARGB_GET_A(color);
                    r += ARGB_GET_R(color);
                    g += ARGB_GET_G(color);
                    b += ARGB_GET_B(color);
                    ++count;
                }
            }
            if (count) {
                a /= count;
                r /= count;
                g /= count;
                b /= count;
            }

            const argb_t color = ARGB(a, r, g, b);

            if (sp->alpha) {
                alpha_blend(color, &dst_line[dst_x]);
            } else {
                dst_line[dst_x] = ARGB_SET_A(0xff) | color;
            }
        }
    }
}

/** Bicubic scale filter, see `scale_fn` for details. */
static void scale_bicubic(struct scale_param* sp, size_t start, size_t step)
{
    const ssize_t left = max(0, sp->x);
    const ssize_t top = max(0, sp->y);
    const ssize_t right =
        min((ssize_t)sp->dst->width, sp->x + sp->scale * sp->src->width);
    const ssize_t bottom =
        min((ssize_t)sp->dst->height, sp->y + sp->scale * sp->src->height);

    const ssize_t delta_x = left - sp->x;
    const ssize_t delta_y = top - sp->y;

    size_t state_zero_x = 1;
    size_t state_zero_y = 1;
    float state[4][4][4] = { 0 }; // color channel, y, x

    for (ssize_t dst_y = top + start; dst_y < bottom; dst_y += step) {
        argb_t* dst_line = &sp->dst->data[dst_y * sp->dst->width];
        const double scaled_y = (double)(dst_y - top + delta_y) / sp->scale;
        const size_t fixed_y = (size_t)scaled_y;
        const float diff_y = scaled_y - fixed_y;
        const float diff_y2 = diff_y * diff_y;
        const float diff_y3 = diff_y * diff_y2;

        for (ssize_t dst_x = left; dst_x < right; ++dst_x) {
            const double scaled_x =
                (double)(dst_x - left + delta_x) / sp->scale;
            const size_t fixed_x = (size_t)scaled_x;
            const float diff_x = scaled_x - fixed_x;
            const float diff_x2 = diff_x * diff_x;
            const float diff_x3 = diff_x * diff_x2;
            argb_t fg = 0;

            // update cached state
            if (state_zero_x != fixed_x || state_zero_y != fixed_y) {
                float pixels[4][4][4]; // color channel, y, x
                state_zero_x = fixed_x;
                state_zero_y = fixed_y;
                for (size_t pc = 0; pc < 4; ++pc) {
                    // get colors for the current area
                    for (size_t py = 0; py < 4; ++py) {
                        size_t iy = fixed_y + py;
                        if (iy > 0) {
                            --iy;
                            if (iy >= sp->src->height) {
                                iy = sp->src->height - 1;
                            }
                        }
                        for (size_t px = 0; px < 4; ++px) {
                            size_t ix = fixed_x + px;
                            if (ix > 0) {
                                --ix;
                                if (ix >= sp->src->width) {
                                    ix = sp->src->width - 1;
                                }
                            }
                            const argb_t pixel =
                                sp->src->data[iy * sp->src->width + ix];
                            pixels[pc][py][px] = (pixel >> (pc * 8)) & 0xff;
                        }
                    }
                    // recalc state cache for the current area
                    // clang-format off
                    state[pc][0][0] = pixels[pc][1][1];
                    state[pc][0][1] = -0.5 * pixels[pc][1][0] + 0.5  * pixels[pc][1][2];
                    state[pc][0][2] =        pixels[pc][1][0] - 2.5  * pixels[pc][1][1] + 2.0  * pixels[pc][1][2] - 0.5  * pixels[pc][1][3];
                    state[pc][0][3] = -0.5 * pixels[pc][1][0] + 1.5  * pixels[pc][1][1] - 1.5  * pixels[pc][1][2] + 0.5  * pixels[pc][1][3];
                    state[pc][1][0] = -0.5 * pixels[pc][0][1] + 0.5  * pixels[pc][2][1];
                    state[pc][1][1] = 0.25 * pixels[pc][0][0] - 0.25 * pixels[pc][0][2] -
                                      0.25 * pixels[pc][2][0] + 0.25 * pixels[pc][2][2];
                    state[pc][1][2] = -0.5 * pixels[pc][0][0] + 1.25 * pixels[pc][0][1] -        pixels[pc][0][2] + 0.25 * pixels[pc][0][3] +
                                       0.5 * pixels[pc][2][0] - 1.25 * pixels[pc][2][1] +        pixels[pc][2][2] - 0.25 * pixels[pc][2][3];
                    state[pc][1][3] = 0.25 * pixels[pc][0][0] - 0.75 * pixels[pc][0][1] + 0.75 * pixels[pc][0][2] - 0.25 * pixels[pc][0][3] -
                                      0.25 * pixels[pc][2][0] + 0.75 * pixels[pc][2][1] - 0.75 * pixels[pc][2][2] + 0.25 * pixels[pc][2][3];
                    state[pc][2][0] =        pixels[pc][0][1] - 2.5  * pixels[pc][1][1] + 2.0  * pixels[pc][2][1] - 0.5  * pixels[pc][3][1];
                    state[pc][2][1] = -0.5 * pixels[pc][0][0] + 0.5  * pixels[pc][0][2] + 1.25 * pixels[pc][1][0] - 1.25 * pixels[pc][1][2] -
                                             pixels[pc][2][0] +        pixels[pc][2][2] + 0.25 * pixels[pc][3][0] - 0.25 * pixels[pc][3][2];
                    state[pc][2][2] =        pixels[pc][0][0] - 2.5  * pixels[pc][0][1] + 2.0  * pixels[pc][0][2] - 0.5  * pixels[pc][0][3] -
                                       2.5 * pixels[pc][1][0] + 6.25 * pixels[pc][1][1] - 5.0  * pixels[pc][1][2] + 1.25 * pixels[pc][1][3] +
                                       2.0 * pixels[pc][2][0] - 5.0  * pixels[pc][2][1] + 4.0  * pixels[pc][2][2] -        pixels[pc][2][3] -
                                       0.5 * pixels[pc][3][0] + 1.25 * pixels[pc][3][1] -        pixels[pc][3][2] + 0.25 * pixels[pc][3][3];
                    state[pc][2][3] = -0.5 * pixels[pc][0][0] + 1.5  * pixels[pc][0][1] - 1.5  * pixels[pc][0][2] + 0.5  * pixels[pc][0][3] +
                                      1.25 * pixels[pc][1][0] - 3.75 * pixels[pc][1][1] + 3.75 * pixels[pc][1][2] - 1.25 * pixels[pc][1][3] -
                                             pixels[pc][2][0] + 3.0  * pixels[pc][2][1] - 3.0  * pixels[pc][2][2] +        pixels[pc][2][3] +
                                      0.25 * pixels[pc][3][0] - 0.75 * pixels[pc][3][1] + 0.75 * pixels[pc][3][2] - 0.25 * pixels[pc][3][3];
                    state[pc][3][0] = -0.5 * pixels[pc][0][1] + 1.5  * pixels[pc][1][1] - 1.5  * pixels[pc][2][1] + 0.5  * pixels[pc][3][1];
                    state[pc][3][1] = 0.25 * pixels[pc][0][0] - 0.25 * pixels[pc][0][2] -
                                      0.75 * pixels[pc][1][0] + 0.75 * pixels[pc][1][2] +
                                      0.75 * pixels[pc][2][0] - 0.75 * pixels[pc][2][2] -
                                      0.25 * pixels[pc][3][0] + 0.25 * pixels[pc][3][2];
                    state[pc][3][2] = -0.5 * pixels[pc][0][0] + 1.25 * pixels[pc][0][1] -        pixels[pc][0][2] + 0.25 * pixels[pc][0][3] +
                                       1.5 * pixels[pc][1][0] - 3.75 * pixels[pc][1][1] + 3.0  * pixels[pc][1][2] - 0.75 * pixels[pc][1][3] -
                                       1.5 * pixels[pc][2][0] + 3.75 * pixels[pc][2][1] - 3.0  * pixels[pc][2][2] + 0.75 * pixels[pc][2][3] +
                                       0.5 * pixels[pc][3][0] - 1.25 * pixels[pc][3][1] +        pixels[pc][3][2] - 0.25 * pixels[pc][3][3];
                    state[pc][3][3] = 0.25 * pixels[pc][0][0] - 0.75 * pixels[pc][0][1] + 0.75 * pixels[pc][0][2] - 0.25 * pixels[pc][0][3] -
                                      0.75 * pixels[pc][1][0] + 2.25 * pixels[pc][1][1] - 2.25 * pixels[pc][1][2] + 0.75 * pixels[pc][1][3] +
                                      0.75 * pixels[pc][2][0] - 2.25 * pixels[pc][2][1] + 2.25 * pixels[pc][2][2] - 0.75 * pixels[pc][2][3] -
                                      0.25 * pixels[pc][3][0] + 0.75 * pixels[pc][3][1] - 0.75 * pixels[pc][3][2] + 0.25 * pixels[pc][3][3];
                    // clang-format on
                }
            }

            // set pixel
            for (size_t pc = 0; pc < 4; ++pc) {
                // clang-format off
                const float inter =
                    (state[pc][0][0] + state[pc][0][1] * diff_x + state[pc][0][2] * diff_x2 + state[pc][0][3] * diff_x3) +
                    (state[pc][1][0] + state[pc][1][1] * diff_x + state[pc][1][2] * diff_x2 + state[pc][1][3] * diff_x3) * diff_y +
                    (state[pc][2][0] + state[pc][2][1] * diff_x + state[pc][2][2] * diff_x2 + state[pc][2][3] * diff_x3) * diff_y2 +
                    (state[pc][3][0] + state[pc][3][1] * diff_x + state[pc][3][2] * diff_x2 + state[pc][3][3] * diff_x3) * diff_y3;
                // clang-format on
                const uint8_t color = max(min(inter, 255), 0);
                fg |= (color << (pc * 8));
            }

            if (sp->alpha) {
                alpha_blend(fg, &dst_line[dst_x]);
            } else {
                dst_line[dst_x] = ARGB_SET_A(0xff) | fg;
            }
        }
    }
}

bool pixmap_create(struct pixmap* pm, size_t width, size_t height)
{
    argb_t* data = calloc(1, height * width * sizeof(argb_t));
    if (data) {
        pm->width = width;
        pm->height = height;
        pm->data = data;
    }
    return !!data;
}

void pixmap_free(struct pixmap* pm)
{
    free(pm->data);
}

bool pixmap_save(struct pixmap* pm, const char* path)
{
    FILE* fp;
    unsigned i;

    if (!(fp = fopen(path, "wb"))) {
        return false;
    }

    // TODO: add alpha channel
    fprintf(fp, "P6\n%zu %zu\n255\n", pm->width, pm->height);
    for (i = 0; i < pm->width * pm->height; ++i) {
        uint8_t color[4] = { (((pm->data[i] >> (8 * 2)) & 0xff)),
                             (((pm->data[i] >> (8 * 1)) & 0xff)),
                             (((pm->data[i] >> (8 * 0)) & 0xff)) };
        fwrite(color, 3, 1, fp);
    }

    fclose(fp);
    return true;
}

bool pixmap_load(struct pixmap* pm, const char* path)
{
    FILE* fp;
    unsigned i;

    if (!(fp = fopen(path, "rb"))) {
        return false;
    }

    char header[3];
    if (fscanf(fp, "%2s\n%zu %zu\n255\n", header, &pm->width, &pm->height) !=
        3) {
        goto fail;
    }

    if (strcmp(header, "P6")) {
        goto fail;
    }

    /* NOTE: It might be that we want the pixmap to have exact width and height
     * and return false if it doesn't match */
    if (!pixmap_create(pm, pm->width, pm->height)) {
        goto fail;
    }

    for (i = 0; i < pm->width * pm->height; ++i) {
        uint8_t color[3];
        if (fread(color, 3, 1, fp) != 1) {
            pixmap_free(pm);
            goto fail;
        }
        pm->data[i] = ARGB(0xff, color[0], color[1], color[2]);
    }

    fclose(fp);
    return true;

fail:
    fclose(fp);
    return false;
}

void pixmap_fill(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, argb_t color)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)pm->width, (ssize_t)width + x);
    const ssize_t bottom = min((ssize_t)pm->height, (ssize_t)height + y);
    const ssize_t fill_width = right - left;
    const ssize_t fill_height = bottom - top;

    const size_t template_sz = fill_width * sizeof(argb_t);
    argb_t* template = &pm->data[top * pm->width + left];

    if (right < 0 || bottom < 0 || fill_width <= 0 || fill_height <= 0) {
        return;
    }

    // compose and copy template line
    for (x = 0; x < fill_width; ++x) {
        template[x] = color;
    }
    for (y = top + 1; y < bottom; ++y) {
        memcpy(&pm->data[y * pm->width + left], template, template_sz);
    }
}

void pixmap_inverse_fill(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                         size_t height, argb_t color)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)pm->width, (ssize_t)width + x);
    const ssize_t bottom = min((ssize_t)pm->height, (ssize_t)height + y);

    if (left > 0) {
        pixmap_fill(pm, 0, top, left, bottom - top, color);
    }
    if (right < (ssize_t)pm->width) {
        pixmap_fill(pm, right, top, pm->width - right, bottom - top, color);
    }
    if (top > 0) {
        pixmap_fill(pm, 0, 0, pm->width, top, color);
    }
    if (bottom < (ssize_t)pm->height) {
        pixmap_fill(pm, 0, bottom, pm->width, pm->height - bottom, color);
    }
}

void pixmap_blend(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  size_t height, argb_t color)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)pm->width, x + (ssize_t)width);
    const ssize_t bottom = min((ssize_t)pm->height, y + (ssize_t)height);

    for (y = top; y < bottom; ++y) {
        argb_t* line = &pm->data[y * pm->width];
        for (x = left; x < right; ++x) {
            alpha_blend(color, &line[x]);
        }
    }
}

void pixmap_hline(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  argb_t color)
{
    if (y >= 0 && y < (ssize_t)pm->height) {
        const ssize_t begin = max(0, x);
        const ssize_t end = min((ssize_t)pm->width, x + (ssize_t)width);
        const size_t offset = y * pm->width;
        for (ssize_t i = begin; i < end; ++i) {
            alpha_blend(color, &pm->data[offset + i]);
        }
    }
}

void pixmap_vline(struct pixmap* pm, ssize_t x, ssize_t y, size_t height,
                  argb_t color)
{
    if (x >= 0 && x < (ssize_t)pm->width) {
        const ssize_t begin = max(0, y);
        const ssize_t end = min((ssize_t)pm->height, y + (ssize_t)height);
        for (ssize_t i = begin; i < end; ++i) {
            alpha_blend(color, &pm->data[i * pm->width + x]);
        }
    }
}

void pixmap_rect(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, argb_t color)
{

    pixmap_hline(pm, x, y, width, color);
    pixmap_hline(pm, x, y + height - 1, width, color);
    pixmap_vline(pm, x, y + 1, height - 1, color);
    pixmap_vline(pm, x + width - 1, y + 1, height - 1, color);
}

void pixmap_grid(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, size_t tail_sz, argb_t color1, argb_t color2)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)pm->width, (ssize_t)width + x);
    const ssize_t bottom = min((ssize_t)pm->height, (ssize_t)height + y);
    const ssize_t grid_width = right - left;
    const ssize_t grid_height = bottom - top;

    const size_t template_sz = grid_width * sizeof(argb_t);
    argb_t* templates[] = { &pm->data[top * pm->width + left],
                            &pm->data[(top + tail_sz) * pm->width + left] };

    if (right < 0 || bottom < 0 || grid_width <= 0 || grid_height <= 0) {
        return;
    }

    for (y = 0; y < grid_height; ++y) {
        const size_t shift = (y / tail_sz) % 2;
        argb_t* line = &pm->data[(y + top) * pm->width + left];
        if (line != templates[0] && line != templates[1]) {
            // put template line
            memcpy(line, templates[shift], template_sz);
        } else {
            // compose template line
            for (x = 0; x < grid_width; ++x) {
                const size_t tail = x / tail_sz;
                line[x] = (tail % 2) ^ shift ? color1 : color2;
            }
        }
    }
}

void pixmap_apply_mask(struct pixmap* pm, ssize_t x, ssize_t y,
                       const uint8_t* mask, size_t width, size_t height,
                       argb_t color)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)pm->width, x + (ssize_t)width);
    const ssize_t bottom = min((ssize_t)pm->height, y + (ssize_t)height);
    const ssize_t dst_width = right - left;
    const ssize_t delta_x = left - x;
    const ssize_t delta_y = top - y;

    for (ssize_t dst_y = top; dst_y < bottom; ++dst_y) {
        const size_t src_y = dst_y - top + delta_y;
        const uint8_t* mask_line = &mask[src_y * width + delta_x];
        argb_t* dst_line = &pm->data[dst_y * pm->width + left];

        for (x = 0; x < dst_width; ++x) {
            const uint8_t alpha_mask = mask_line[x];
            if (alpha_mask != 0) {
                const uint8_t alpha_color = ARGB_GET_A(color);
                const uint8_t alpha = (alpha_mask * alpha_color) / 255;
                const argb_t clr = ARGB_SET_A(alpha) | (color & 0x00ffffff);
                alpha_blend(clr, &dst_line[x]);
            }
        }
    }
}

void pixmap_copy(const struct pixmap* src, struct pixmap* dst, ssize_t x,
                 ssize_t y, bool alpha)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)dst->width, x + (ssize_t)src->width);
    const ssize_t bottom = min((ssize_t)dst->height, y + (ssize_t)src->height);
    const ssize_t dst_width = right - left;
    const ssize_t delta_x = left - x;
    const ssize_t delta_y = top - y;
    const size_t line_sz = dst_width * sizeof(argb_t);

    for (ssize_t dst_y = top; dst_y < bottom; ++dst_y) {
        const size_t src_y = dst_y - top + delta_y;
        const argb_t* src_line = &src->data[src_y * src->width + delta_x];
        argb_t* dst_line = &dst->data[dst_y * dst->width + left];

        if (alpha) {
            for (x = 0; x < dst_width; ++x) {
                alpha_blend(src_line[x], &dst_line[x]);
            }
        } else {
            memcpy(dst_line, src_line, line_sz);
        }
    }
}

void pixmap_scale(enum pixmap_scale scaler, const struct pixmap* src,
                  struct pixmap* dst, ssize_t x, ssize_t y, float scale,
                  bool alpha)
{
    // get numper of active CPUs
#ifdef __FreeBSD__
    uint32_t cpus = 0;
    size_t cpus_len = sizeof(cpus);
    sysctlbyname("hw.ncpu", &cpus, &cpus_len, 0, 0);
#else
    const long cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif

    // background threads
    const size_t threads_num = min(16, max(cpus, 1)) - 1;
    struct scale_task* tasks = NULL;

    // scaling parameters
    struct scale_param sp = {
        .src = src,
        .dst = dst,
        .x = x,
        .y = y,
        .scale = scale,
        .alpha = alpha,
    };
    scale_fn scaler_fn;

    switch (scaler) {
        case pixmap_nearest:
            scaler_fn = scale_nearest;
            break;
        case pixmap_bicubic:
            scaler_fn = scale_bicubic;
            break;
        case pixmap_average:
            scaler_fn = scale_average;
            break;
        default:
            return;
    }

    // create task for each CPU core
    if (threads_num) {
        tasks = malloc(threads_num * sizeof(struct scale_task));
        if (!tasks) {
            return;
        }
    }

    // start background threads
    for (size_t i = 0; i < threads_num; ++i) {
        struct scale_task* task = &tasks[i];
        task->tid = 0;
        task->sp = &sp;
        task->start = i + 1; // skip first line
        task->step = threads_num + 1;
        task->fn = scaler_fn;
        pthread_create(&task->tid, NULL, scale_thread, task);
    }

    // execute the first task in the current thread
    scaler_fn(&sp, 0, threads_num + 1);

    // wait for all threads to complete
    for (size_t i = 0; i < threads_num; ++i) {
        pthread_join(tasks[i].tid, NULL);
    }

    free(tasks);
}

void pixmap_flip_vertical(struct pixmap* pm)
{
    void* buffer;
    const size_t stride = pm->width * sizeof(argb_t);

    buffer = malloc(stride);
    if (buffer) {
        for (size_t y = 0; y < pm->height / 2; ++y) {
            argb_t* src = &pm->data[y * pm->width];
            argb_t* dst = &pm->data[(pm->height - y - 1) * pm->width];
            memcpy(buffer, dst, stride);
            memcpy(dst, src, stride);
            memcpy(src, buffer, stride);
        }
        free(buffer);
    }
}

void pixmap_flip_horizontal(struct pixmap* pm)
{
    for (size_t y = 0; y < pm->height; ++y) {
        argb_t* line = &pm->data[y * pm->width];
        for (size_t x = 0; x < pm->width / 2; ++x) {
            argb_t* left = &line[x];
            argb_t* right = &line[pm->width - x - 1];
            const argb_t swap = *left;
            *left = *right;
            *right = swap;
        }
    }
}

void pixmap_rotate(struct pixmap* pm, size_t angle)
{
    const size_t pixels = pm->width * pm->height;

    if (angle == 180) {
        for (size_t i = 0; i < pixels / 2; ++i) {
            argb_t* color1 = &pm->data[i];
            argb_t* color2 = &pm->data[pixels - i - 1];
            const argb_t swap = *color1;
            *color1 = *color2;
            *color2 = swap;
        }
    } else if (angle == 90 || angle == 270) {
        argb_t* data = malloc(pm->height * pm->width * sizeof(argb_t));
        if (data) {
            const size_t width = pm->height;
            const size_t height = pm->width;
            for (size_t y = 0; y < pm->height; ++y) {
                for (size_t x = 0; x < pm->width; ++x) {
                    size_t pos;
                    if (angle == 90) {
                        pos = x * width + (width - y - 1);
                    } else {
                        pos = (height - x - 1) * width + y;
                    }
                    data[pos] = pm->data[y * pm->width + x];
                }
            }
            free(pm->data);
            pm->width = width;
            pm->height = height;
            pm->data = data;
        }
    }
}

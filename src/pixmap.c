// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "pixmap.h"

#include <stdlib.h>
#include <string.h>

/**
 * Alpha blending.
 * @param img color of image's pixel
 * @param wnd pointer to window buffer to put puxel
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

void pixmap_apply_mask(struct pixmap* dst, size_t x, size_t y,
                       const uint8_t* mask, size_t width, size_t height,
                       argb_t color)
{
    size_t mask_width;
    size_t mask_height;

    if (width == 0 || x >= dst->width || height == 0 || y >= dst->height) {
        return;
    }

    mask_width = min(width, dst->width - x);
    mask_height = min(height, dst->height - y);

    for (size_t mask_y = 0; mask_y < mask_height; ++mask_y) {
        argb_t* dst_line = &dst->data[(y + mask_y) * dst->width + x];
        const uint8_t* mask_line = &mask[mask_y * width];

        for (size_t mask_x = 0; mask_x < mask_width; ++mask_x) {
            const uint8_t alpha_mask = mask_line[mask_x];
            if (alpha_mask != 0) {
                const uint8_t alpha_color = ARGB_GET_A(color);
                const uint8_t alpha = (alpha_mask * alpha_color) / 255;
                const argb_t clr = ARGB_SET_A(alpha) | (color & 0x00ffffff);
                alpha_blend(clr, &dst_line[mask_x]);
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

void pixmap_scale_nearest(const struct pixmap* src, struct pixmap* dst,
                          ssize_t x, ssize_t y, float scale, bool alpha)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)dst->width, x + scale * src->width);
    const ssize_t bottom = min((ssize_t)dst->height, y + scale * src->height);

    const ssize_t delta_x = left - x;
    const ssize_t delta_y = top - y;

    for (ssize_t dst_y = top; dst_y < bottom; ++dst_y) {
        const size_t src_y = (float)(dst_y - top + delta_y) / scale;
        const argb_t* src_line = &src->data[src_y * src->width];
        argb_t* dst_line = &dst->data[dst_y * dst->width];

        for (ssize_t dst_x = left; dst_x < right; ++dst_x) {
            const size_t src_x = (float)(dst_x - left + delta_x) / scale;
            const argb_t color = src_line[src_x];

            if (alpha) {
                alpha_blend(color, &dst_line[dst_x]);
            } else {
                dst_line[dst_x] = ARGB_SET_A(0xff) | color;
            }
        }
    }
}

void pixmap_scale_bicubic(const struct pixmap* src, struct pixmap* dst,
                          ssize_t x, ssize_t y, float scale, bool alpha)
{
    const ssize_t left = max(0, x);
    const ssize_t top = max(0, y);
    const ssize_t right = min((ssize_t)dst->width, x + scale * src->width);
    const ssize_t bottom = min((ssize_t)dst->height, y + scale * src->height);

    const ssize_t delta_x = left - x;
    const ssize_t delta_y = top - y;

    size_t state_zero_x = 1;
    size_t state_zero_y = 1;
    float state[4][4][4]; // color channel, y, x

    for (ssize_t dst_y = top; dst_y < bottom; ++dst_y) {
        argb_t* dst_line = &dst->data[dst_y * dst->width];
        const double scaled_y = (double)(dst_y - top + delta_y) / scale;
        const size_t fixed_y = (size_t)scaled_y;
        const float diff_y = scaled_y - fixed_y;
        const float diff_y2 = diff_y * diff_y;
        const float diff_y3 = diff_y * diff_y2;

        for (ssize_t dst_x = left; dst_x < right; ++dst_x) {
            const double scaled_x = (double)(dst_x - left + delta_x) / scale;
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
                            if (iy >= src->height) {
                                iy = src->height - 1;
                            }
                        }
                        for (size_t px = 0; px < 4; ++px) {
                            size_t ix = fixed_x + px;
                            if (ix > 0) {
                                --ix;
                                if (ix >= src->width) {
                                    ix = src->width - 1;
                                }
                            }
                            const argb_t pixel =
                                src->data[iy * src->width + ix];
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

            if (alpha) {
                alpha_blend(fg, &dst_line[dst_x]);
            } else {
                dst_line[dst_x] = ARGB_SET_A(0xff) | fg;
            }
        }
    }
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

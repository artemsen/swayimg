// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "pixmap.h"

#include <stdlib.h>
#include <string.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

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
        const argb_t inv = 256 - ai;
        src = ARGB_SET_A(target_alpha) |
            ARGB_SET_R((ai * ARGB_GET_R(src) + inv * ARGB_GET_R(wp)) >> 8) |
            ARGB_SET_G((ai * ARGB_GET_G(src) + inv * ARGB_GET_G(wp)) >> 8) |
            ARGB_SET_B((ai * ARGB_GET_B(src) + inv * ARGB_GET_B(wp)) >> 8);
    }

    *dst = src;
}

/**
 * Put one pixmap on another: nearest filter, fast but ugly.
 */
static void put_nearest(struct pixmap* dst, ssize_t dst_x, ssize_t dst_y,
                        const struct pixmap* src, ssize_t src_x, ssize_t src_y,
                        float src_scale, bool alpha)
{
    const size_t max_dst_x = min(dst->width, src_x + src_scale * src->width);
    const size_t max_dst_y = min(dst->height, src_y + src_scale * src->height);

    const size_t dst_width = max_dst_x - dst_x;
    const size_t dst_height = max_dst_y - dst_y;

    const size_t delta_x = dst_x - src_x;
    const size_t delta_y = dst_y - src_y;

    for (size_t y = 0; y < dst_height; ++y) {
        const size_t scaled_src_y = (float)(y + delta_y) / src_scale;
        const argb_t* src_line = &src->data[scaled_src_y * src->width];
        argb_t* dst_line = &dst->data[(y + dst_y) * dst->width + dst_x];

        for (size_t x = 0; x < dst_width; ++x) {
            const size_t src_x = (float)(x + delta_x) / src_scale;
            const argb_t color = src_line[src_x];
            if (alpha) {
                alpha_blend(color, &dst_line[x]);
            } else {
                dst_line[x] = ARGB_SET_A(0xff) | color;
            }
        }
    }
}

/**
 * Put one pixmap on another: bicubic filter, nice but slow.
 */
static void put_bicubic(struct pixmap* dst, ssize_t dst_x, ssize_t dst_y,
                        const struct pixmap* src, ssize_t src_x, ssize_t src_y,
                        float src_scale, bool alpha)
{
    size_t state_zero_x = 1;
    size_t state_zero_y = 1;
    float state[4][4][4]; // color channel, y, x

    const size_t max_dst_x = min(dst->width, src_x + src_scale * src->width);
    const size_t max_dst_y = min(dst->height, src_y + src_scale * src->height);
    const size_t dst_width = max_dst_x - dst_x;
    const size_t dst_height = max_dst_y - dst_y;
    const size_t delta_x = dst_x - src_x;
    const size_t delta_y = dst_y - src_y;

    for (size_t y = 0; y < dst_height; ++y) {
        argb_t* dst_line = &dst->data[(dst_y + y) * dst->width + dst_x];
        const float scaled_y = (float)(y + delta_y) / src_scale - 0.5;
        const size_t fixed_y = (size_t)scaled_y;
        const float diff_y = scaled_y - fixed_y;
        const float diff_y2 = diff_y * diff_y;
        const float diff_y3 = diff_y * diff_y2;

        for (size_t x = 0; x < dst_width; ++x) {
            const float scaled_x = (float)(x + delta_x) / src_scale - 0.5;
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
                alpha_blend(fg, &dst_line[x]);
            } else {
                dst_line[x] = ARGB_SET_A(0xff) | fg;
            }
        }
    }
}

bool pixmap_create(struct pixmap* pm, size_t width, size_t height)
{
    pm->width = width;
    pm->height = height;
    pm->data = calloc(1, height * width * sizeof(argb_t));

    return !!(pm->data);
}

void pixmap_free(struct pixmap* pm)
{
    free(pm->data);
}

void pixmap_fill(struct pixmap* pm, size_t x, size_t y, size_t width,
                 size_t height, argb_t color)
{
    const size_t max_y = y + height;
    const size_t template_sz = width * sizeof(argb_t);
    argb_t* template = &pm->data[y * pm->width + x];

    // compose template line
    for (size_t i = 0; i < width; ++i) {
        template[i] = color;
    }

    // put template line
    for (size_t i = y + 1; i < max_y; ++i) {
        memcpy(&pm->data[i * pm->width + x], template, template_sz);
    }
}

void pixmap_grid(struct pixmap* pm, size_t x, size_t y, size_t width,
                 size_t height, size_t tail_sz, argb_t color1, argb_t color2)
{
    const size_t template_sz = width * sizeof(argb_t);
    argb_t* templates[] = { &pm->data[y * pm->width + x],
                            &pm->data[(y + tail_sz) * pm->width + x] };

    for (size_t i = 0; i < height; ++i) {
        const size_t shift = (i / tail_sz) % 2;
        argb_t* line = &pm->data[(y + i) * pm->width + x];
        if (line != templates[0] && line != templates[1]) {
            // put template line
            memcpy(line, templates[shift], template_sz);
        } else {
            // compose template line
            for (size_t j = 0; j < width; ++j) {
                const size_t tail = j / tail_sz;
                line[j] = (tail % 2) ^ shift ? color1 : color2;
            }
        }
    }
}

void pixmap_apply_mask(struct pixmap* dst, size_t x, size_t y,
                       const uint8_t* mask, size_t width, size_t height,
                       argb_t color)
{
    for (size_t mask_y = 0; mask_y < height; ++mask_y) {
        argb_t* dst_line = &dst->data[(y + mask_y) * dst->width + x];
        const uint8_t* mask_line = &mask[mask_y * width];

        if (y + mask_y >= dst->height) {
            break; // out of map
        }

        for (size_t mask_x = 0; mask_x < width; ++mask_x) {
            uint8_t alpha;

            if (x + mask_x >= dst->width) {
                break; // out of map
            }

            alpha = mask_line[mask_x];
            if (alpha) {
                argb_t* pixel = &dst_line[mask_x];
                const argb_t bg = *pixel;
                const argb_t fg = color;
                *pixel = ARGB_ALPHA_BLEND(alpha, 0xff, bg, fg);
            }
        }
    }
}

void pixmap_copy(struct pixmap* dst, size_t dst_x, size_t dst_y,
                 const struct pixmap* src, size_t src_width, size_t src_height)
{
    if (src_width >= dst_x + dst->width) {
        src_width = dst->width - dst_x;
    }
    if (src_height >= dst_y + dst->height) {
        src_height = dst->height - dst_y;
    }

    const size_t len = src_width * sizeof(argb_t);

    for (size_t y = 0; y < src_height; ++y) {
        argb_t* dst_ptr = &dst->data[(y + dst_y) * dst->width + dst_x];
        const argb_t* src_ptr = &src->data[y * src->width];
        memcpy(dst_ptr, src_ptr, len);
    }
}

void pixmap_put(struct pixmap* dst, ssize_t dst_x, ssize_t dst_y,
                const struct pixmap* src, ssize_t src_x, ssize_t src_y,
                float src_scale, bool alpha, bool antialiasing)
{
    if (antialiasing) {
        put_bicubic(dst, dst_x, dst_y, src, src_x, src_y, src_scale, alpha);
    } else {
        put_nearest(dst, dst_x, dst_y, src, src_x, src_y, src_scale, alpha);
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

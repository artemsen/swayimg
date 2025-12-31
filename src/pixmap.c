// SPDX-License-Identifier: MIT
// Pixel map.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "pixmap.h"

#include "tpool.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Background blur parameters
#define BLUR_SIZE  3
#define BLUR_SIGMA 16

/** Pixmap slice. */
struct pm_slice {
    struct pixmap* pm;    ///< Parent pixmap
    size_t x, y;          ///< Top left coordinates on parent pixmap
    size_t width, height; ///< Size of the slice
};

/** Parameters for background threads. */
struct bkg_params {
    const struct pm_slice* image; ///< Image area
    struct pm_slice fill;         ///< Work area to fill
};

/** Color accumulator. */
struct cacc {
    double r, g, b;
};

/**
 * Initialize color accumulator.
 * @param cacc color accumulator instance
 * @param color RGB color to set
 * @param factor color factor
 */
static inline void cacc_set(struct cacc* cacc, argb_t color, double factor)
{
    cacc->r = ARGB_GET_R(color) * factor;
    cacc->g = ARGB_GET_G(color) * factor;
    cacc->b = ARGB_GET_B(color) * factor;
}

/**
 * Add color to accumulator.
 * @param cacc color accumulator instance
 * @param color RGB color to add
 */
static inline void cacc_add(struct cacc* cacc, argb_t color)
{
    cacc->r += ARGB_GET_R(color);
    cacc->g += ARGB_GET_G(color);
    cacc->b += ARGB_GET_B(color);
}

/**
 * Subtract color from accumulator.
 * @param cacc color accumulator instance
 * @param color RGB color to subtract
 */
static inline void cacc_sub(struct cacc* cacc, argb_t color)
{
    cacc->r -= ARGB_GET_R(color);
    cacc->g -= ARGB_GET_G(color);
    cacc->b -= ARGB_GET_B(color);
}

/**
 * Create ARGB color from accumulator.
 * @param cacc color accumulator instance
 * @param weight weight of the components
 * @return ARGB color
 */
static inline argb_t cacc_argb(const struct cacc* cacc, double weight)
{
    ssize_t r = cacc->r * weight;
    ssize_t g = cacc->g * weight;
    ssize_t b = cacc->b * weight;
    r = max(0, r);
    g = max(0, g);
    b = max(0, b);
    r = min(ARGB_MAX_COLOR, r);
    g = min(ARGB_MAX_COLOR, g);
    b = min(ARGB_MAX_COLOR, b);
    return ARGB(ARGB_MAX_COLOR, r, g, b);
}

/**
 * Get pointer to the pixel at specified coordinates.
 * @param slice pixmap slice
 * @param x,y coordinates of target pixel
 * @return pointer to ARGB pixel
 */
static inline argb_t* slice_ptr(const struct pm_slice* slice, size_t x,
                                size_t y)
{
    assert(x < slice->width);
    assert(y < slice->height);
    const size_t offset = (slice->y + y) * slice->pm->width + slice->x + x;
    return &slice->pm->data[offset];
}

/**
 * Blur pixmap slice horizontally.
 * @param slice pixmap slice
 * @param radius blur radius
 */
static void blur_h(struct pm_slice* slice, size_t radius)
{
    const size_t radius_plus = radius + 1;
    const double weight = 1.0 / (radius + radius_plus);

    for (size_t y = 0; y < slice->height; ++y) {
        const argb_t px_first = *slice_ptr(slice, 0, y);
        const argb_t px_last = *slice_ptr(slice, slice->width - 1, y);
        struct cacc acc;

        cacc_set(&acc, px_first, radius_plus);
        for (size_t x = 0; x < radius && x < slice->width; ++x) {
            cacc_add(&acc, *slice_ptr(slice, x, y));
        }

        for (size_t x = 0; x <= radius && x + radius < slice->width; ++x) {
            cacc_add(&acc, *slice_ptr(slice, x + radius, y));
            cacc_sub(&acc, px_first);
            *slice_ptr(slice, x, y) = cacc_argb(&acc, weight);
        }

        for (size_t x = radius_plus; x + radius < slice->width; ++x) {
            cacc_add(&acc, *slice_ptr(slice, x + radius, y));
            cacc_sub(&acc, *slice_ptr(slice, x - radius_plus, y));
            *slice_ptr(slice, x, y) = cacc_argb(&acc, weight);
        }

        for (size_t x = slice->width - radius;
             x < slice->width && x >= radius_plus; ++x) {
            cacc_add(&acc, px_last);
            cacc_sub(&acc, *slice_ptr(slice, x - radius_plus, y));
            *slice_ptr(slice, x, y) = cacc_argb(&acc, weight);
        }
    }
}

/**
 * Blur pixmap slice vertically.
 * @param slice pixmap slice
 * @param radius blur radius
 */
void blur_v(struct pm_slice* slice, size_t radius)
{
    const size_t radius_plus = radius + 1;
    const double weight = 1.0 / (radius + radius_plus);

    for (size_t x = 0; x < slice->width; ++x) {
        const argb_t px_first = *slice_ptr(slice, x, 0);
        const argb_t px_last = *slice_ptr(slice, x, slice->height - 1);
        struct cacc acc;

        cacc_set(&acc, px_first, radius_plus);
        for (size_t y = 0; y < radius && y < slice->height; ++y) {
            cacc_add(&acc, *slice_ptr(slice, x, y));
        }

        for (size_t y = 0; y <= radius && y + radius < slice->height; ++y) {
            cacc_add(&acc, *slice_ptr(slice, x, y + radius));
            cacc_sub(&acc, px_first);
            *slice_ptr(slice, x, y) = cacc_argb(&acc, weight);
        }

        for (size_t y = radius_plus; y + radius < slice->height; ++y) {
            cacc_add(&acc, *slice_ptr(slice, x, y + radius));
            cacc_sub(&acc, *slice_ptr(slice, x, y - radius_plus));
            *slice_ptr(slice, x, y) = cacc_argb(&acc, weight);
        }

        for (size_t y = slice->height - radius;
             y < slice->height && y >= radius_plus; ++y) {
            cacc_add(&acc, px_last);
            cacc_sub(&acc, *slice_ptr(slice, x, y - radius_plus));
            *slice_ptr(slice, x, y) = cacc_argb(&acc, weight);
        }
    }
}

/**
 * Apply Gaussian blur to pixmap slice.
 * @param slice pixmap slice
 */
static void blur(struct pm_slice* slice)
{
    const double sigma12 = 12 * BLUR_SIGMA * BLUR_SIGMA;
    size_t weight_min, weight_max;
    size_t weight_tran;
    static size_t blur_box[BLUR_SIZE] = { 0 };

    if (!blur_box[0]) {
        // create Gaussian blur box
        weight_min = sqrt(sigma12 / BLUR_SIZE + 1);
        if (weight_min % 2 == 0) {
            --weight_min;
        }
        weight_max = weight_min + 2;
        weight_tran = (sigma12 - BLUR_SIZE * weight_min * weight_min -
                       4.0 * BLUR_SIZE * weight_min - 3.0 * BLUR_SIZE) /
            (-4.0 * weight_min - 4.0);
        for (size_t i = 0; i < BLUR_SIZE; ++i) {
            blur_box[i] = i < weight_tran ? weight_min : weight_max;
        }
    }

    // multi-pass blur
    for (size_t i = 0; i < BLUR_SIZE; ++i) {
        const size_t radius = (blur_box[i] - 1) / 2;
        blur_h(slice, radius);
        blur_v(slice, radius);
    }
}

/** Working thread callback: extend/blur background. */
static void bkg_extend(void* data)
{
    struct bkg_params* bkg = data;
    const struct pixmap* parent = bkg->image->pm;

    const double scale_w = (double)parent->width / bkg->image->width;
    const double scale_h = (double)parent->height / bkg->image->height;
    const double scale = max(scale_w, scale_h);

    const size_t diff_w = (scale * bkg->image->width - parent->width) / 2;
    const size_t diff_h = (scale * bkg->image->height - parent->height) / 2;
    const size_t diff_x = diff_w + bkg->fill.x;
    const size_t diff_y = diff_h + bkg->fill.y;

    for (size_t y = 0; y < bkg->fill.height; ++y) {
        const size_t img_y = (diff_y + y) / scale;
        for (size_t x = 0; x < bkg->fill.width; ++x) {
            const size_t img_x = (diff_x + x) / scale;
            const argb_t px = *slice_ptr(bkg->image, img_x, img_y);
            *slice_ptr(&bkg->fill, x, y) = px;
        }
    }

    blur(&bkg->fill);
}

/** Working thread callback: mirror/blur background. */
static void bkg_mirror(void* data)
{
    struct bkg_params* bkg = data;

    const bool right = bkg->fill.x == bkg->image->x + bkg->image->width;
    const bool top = bkg->fill.y + bkg->fill.height == bkg->image->y;
    const bool bottom = bkg->fill.y == bkg->image->y + bkg->image->height;

    for (size_t y = 0; y < bkg->fill.height; ++y) {
        size_t img_y;
        size_t offset;
        bool flip;

        // get vertical image coordinates
        if (top) {
            offset = bkg->image->height - (bkg->image->y % bkg->image->height);
            flip = ((offset + y) / bkg->image->height) % 2 ==
                (bkg->image->y / bkg->image->height) % 2;
        } else if (bottom) {
            offset = 0;
            flip = ((offset + y) / bkg->image->height) % 2 == 0;
        } else {
            offset = 0;
            flip = false;
        }
        img_y = (y + offset) % bkg->image->height;
        if (flip) {
            img_y = bkg->image->height - img_y - 1;
        }

        for (size_t x = 0; x < bkg->fill.width; ++x) {
            argb_t color;
            size_t img_x;

            // get horzontal image coordinates
            if (right) {
                offset = 0;
                flip = ((offset + x) / bkg->image->width) % 2 == 0;
            } else {
                offset =
                    bkg->image->width - (bkg->image->x % bkg->image->width);
                flip = ((offset + x) / bkg->image->width) % 2 ==
                    (bkg->image->x / bkg->image->width) % 2;
            }

            img_x = (x + offset) % bkg->image->width;
            if (flip) {
                img_x = bkg->image->width - img_x - 1;
            }

            // copy pixel
            color = *slice_ptr(bkg->image, img_x, img_y);
            *slice_ptr(&bkg->fill, x, y) = color;
        }
    }

    blur(&bkg->fill);
}

/**
 * Create background.
 * @param pm pixmap context
 * @param x,y top left coordinates of existing image on pixmap surface
 * @param width,height size of existing image on pixmap surface
 * @param wfn filter to apply
 */
static void bkg_create(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                       size_t height, tpool_worker wfn)
{
    const struct pm_slice img = {
        .pm = pm,
        .x = max(0, x),
        .y = max(0, y),
        .width = min((ssize_t)pm->width, (ssize_t)width + x) - max(0, x),
        .height = min((ssize_t)pm->height, (ssize_t)height + y) - max(0, y),
    };

    struct bkg_params tasks[4]; // left/right/top/bottom
    size_t tid = 0;

    if (img.width == 0 || img.height == 0) {
        return;
    }

    if (img.x > 0) {
        tasks[tid].image = &img;
        tasks[tid].fill.pm = img.pm;
        tasks[tid].fill.x = 0;
        tasks[tid].fill.y = img.y;
        tasks[tid].fill.width = img.x;
        tasks[tid].fill.height = img.height;
        tpool_add_task(wfn, NULL, &tasks[tid++]);
    }
    if (img.x + img.width < img.pm->width) {
        tasks[tid].image = &img;
        tasks[tid].fill.pm = img.pm;
        tasks[tid].fill.x = img.x + img.width;
        tasks[tid].fill.y = img.y;
        tasks[tid].fill.width = img.pm->width - tasks[tid].fill.x;
        tasks[tid].fill.height = img.height;
        tpool_add_task(wfn, NULL, &tasks[tid++]);
    }
    if (img.y > 0) {
        tasks[tid].image = &img;
        tasks[tid].fill.pm = img.pm;
        tasks[tid].fill.x = 0;
        tasks[tid].fill.y = 0;
        tasks[tid].fill.width = img.pm->width;
        tasks[tid].fill.height = img.y;
        tpool_add_task(wfn, NULL, &tasks[tid++]);
    }
    if (img.y + img.height < img.pm->height) {
        tasks[tid].image = &img;
        tasks[tid].fill.pm = img.pm;
        tasks[tid].fill.x = 0;
        tasks[tid].fill.y = img.y + img.height;
        tasks[tid].fill.width = img.pm->width;
        tasks[tid].fill.height = img.pm->height - tasks[tid].fill.y;
        tpool_add_task(wfn, NULL, &tasks[tid++]);
    }

    tpool_wait();
}

bool pixmap_create(struct pixmap* pm, enum pixmap_format format, size_t width,
                   size_t height)
{
    argb_t* data = calloc(1, height * width * sizeof(argb_t));
    if (data) {
        pm->format = format;
        pm->width = width;
        pm->height = height;
        pm->data = data;
    }
    return data;
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
            pixmap_alpha_blend(color, &line[x]);
        }
    }
}

void pixmap_hline(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                  size_t thickness, argb_t color)
{
    for (size_t j = 0; j < thickness; ++j) {
        const ssize_t yy = y + j;
        if (yy >= 0 && yy < (ssize_t)pm->height) {
            const ssize_t begin = max(0, x);
            const ssize_t end = min((ssize_t)pm->width, x + (ssize_t)width);
            const size_t offset = yy * pm->width;
            for (ssize_t i = begin; i < end; ++i) {
                pixmap_alpha_blend(color, &pm->data[offset + i]);
            }
        }
    }
}

void pixmap_vline(struct pixmap* pm, ssize_t x, ssize_t y, size_t height,
                  size_t thickness, argb_t color)
{
    for (size_t j = 0; j < thickness; ++j) {
        const ssize_t xx = x + j;
        if (xx >= 0 && xx < (ssize_t)pm->width) {
            const ssize_t begin = max(0, y);
            const ssize_t end = min((ssize_t)pm->height, y + (ssize_t)height);
            for (ssize_t i = begin; i < end; ++i) {
                pixmap_alpha_blend(color, &pm->data[i * pm->width + xx]);
            }
        }
    }
}

void pixmap_rect(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                 size_t height, size_t thickness, argb_t color)
{
    pixmap_hline(pm, x, y, width, thickness, color);
    pixmap_hline(pm, x, y + height - thickness, width, thickness, color);
    pixmap_vline(pm, x, y + thickness, height - thickness * 2, thickness,
                 color);
    pixmap_vline(pm, x + width - thickness, y + thickness,
                 height - thickness * 2, thickness, color);
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
                const uint8_t alpha =
                    (alpha_mask * alpha_color) / ARGB_MAX_COLOR;
                const argb_t clr = ARGB_SET_A(alpha) | (color & 0x00ffffff);
                pixmap_alpha_blend(clr, &dst_line[x]);
            }
        }
    }
}

void pixmap_copy(const struct pixmap* src, struct pixmap* dst, ssize_t x,
                 ssize_t y)
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

        if (src->format == pixmap_argb) {
            for (x = 0; x < dst_width; ++x) {
                pixmap_alpha_blend(src_line[x], &dst_line[x]);
            }
        } else {
            memcpy(dst_line, src_line, line_sz);
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
        argb_t* data = malloc(pm->height * pm->width * sizeof(*data));
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

void pixmap_bkg_extend(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                       size_t height)
{
    bkg_create(pm, x, y, width, height, bkg_extend);
}

void pixmap_bkg_mirror(struct pixmap* pm, ssize_t x, ssize_t y, size_t width,
                       size_t height)
{
    bkg_create(pm, x, y, width, height, bkg_mirror);
}

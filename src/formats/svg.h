// SPDX-License-Identifier: MIT
// SVG format decoder.

#pragma once

// SVG decoder implementation
enum image_status decode_svg(struct image* img, const uint8_t* data,
                             size_t size);

/**
 * Adjust the render size of SVG images
 * @param scale target scale factor (* RENDER_SIZE_BASE)
 */
void adjust_svg_render_size(double scale);

/**
 * Reset the render size to RENDER_SIZE_BASE
 */
void reset_svg_render_size(void);

/**
 * Decode an SVG with a partial viewport.
 * @param img the source image
 * @param dst destination pixmap
 * @param x,y destination left top coordinates
 * @param scale scale of source pixmap
 */
enum image_status decode_svg_partial(struct image* img,
                                     const struct pixmap* dst, ssize_t x,
                                     ssize_t y, double scale);

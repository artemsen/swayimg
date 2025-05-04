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

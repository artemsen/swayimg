// SPDX-License-Identifier: MIT
// Alpha blending.
// Copyright (C) 2024 Abe Wieland <abe.wieland@gmail.com>

#pragma once

#include "pixmap.h"

/**
 * Alpha blending.
 * @param src top pixel
 * @param dst bottom pixel
 */
static inline void alpha_blend(argb_t src, argb_t* dst)
{
    const uint8_t a1 = ARGB_GET_A(src);
    if (a1 == 255) {
        *dst = src;
    } else if (a1 != 0) {
        // if all quantities are in [0, 1] range, the formulas are:
        // a_out = a_top + (1 - a_top) * a_bot
        // c_out = a_top * c_top + (1 - a_top) * a_bot * c_bot
        // this integer math does the same, avoiding some division
        const argb_t dp = *dst;
        const uint32_t c1 = a1 * 255;
        const uint32_t c2 = (255 - a1) * ARGB_GET_A(dp);
        // guaranteed to be non-zero because a1 is nonzero
        const uint32_t alpha = c1 + c2;
        *dst = ARGB(alpha / 255,
                    (ARGB_GET_R(src) * c1 + ARGB_GET_R(dp) * c2) / alpha,
                    (ARGB_GET_G(src) * c1 + ARGB_GET_G(dp) * c2) / alpha,
                    (ARGB_GET_B(src) * c1 + ARGB_GET_B(dp) * c2) / alpha);
    }
}

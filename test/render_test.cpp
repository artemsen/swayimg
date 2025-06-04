// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Abe Wieland <abe.wieland@gmail.com>

extern "C" {
#include "render.h"
}

#include "pixmap_test.h"

class RenderTest : public PixmapTest {
protected:
    void ScaleCopy(enum aa_mode scaler, const struct pixmap& src, size_t w,
                   size_t h, float scale, ssize_t x, ssize_t y)
    {
        struct pixmap full, dst1, dst2;
        pixmap_create(&full, pixmap_argb, src.width * scale,
                      src.height * scale);
        pixmap_create(&dst1, pixmap_argb, w, h);
        pixmap_create(&dst2, pixmap_argb, w, h);
        software_render(&src, &full, 0, 0, scale, scaler, false);
        pixmap_copy(&full, &dst1, x, y);
        software_render(&src, &dst2, x, y, scale, scaler, false);
        Compare(dst2, dst1.data);
        pixmap_free(&full);
        pixmap_free(&dst1);
        pixmap_free(&dst2);
    }
};

TEST_F(RenderTest, ScaleCopyUp)
{
    // clang-format off
    argb_t src[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x11, 0x12, 0x13,
        0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33,
    };
    // clang-format on

    const struct pixmap pm = { pixmap_argb, 4, 4, src };
    ScaleCopy(aa_bilinear, pm, 2, 2, 2.0, 0, 0);
}

TEST_F(RenderTest, ScaleCopyUpNeg)
{
    // clang-format off
    argb_t src[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x11, 0x12, 0x13,
        0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33,
    };
    // clang-format on

    const struct pixmap pm = { pixmap_argb, 4, 4, src };
    ScaleCopy(aa_bilinear, pm, 2, 2, 2.0, -1, -1);
}

TEST_F(RenderTest, ScaleCopyUpPos)
{
    // clang-format off
    argb_t src[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x11, 0x12, 0x13,
        0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33,
    };
    // clang-format on

    const struct pixmap pm = { pixmap_argb, 4, 4, src };
    ScaleCopy(aa_bilinear, pm, 2, 2, 2.0, 1, 1);
}

TEST_F(RenderTest, ScaleCopyDown)
{
    // clang-format off
    argb_t src[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x11, 0x12, 0x13,
        0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33,
    };
    // clang-format on

    const struct pixmap pm = { pixmap_argb, 4, 4, src };
    ScaleCopy(aa_bilinear, pm, 2, 2, 0.5, 0, 0);
}

TEST_F(RenderTest, ScaleCopyDownNeg)
{
    // clang-format off
    argb_t src[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x11, 0x12, 0x13,
        0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33,
    };
    // clang-format on

    const struct pixmap pm = { pixmap_argb, 4, 4, src };
    ScaleCopy(aa_bilinear, pm, 2, 2, 0.5, -1, -1);
}

TEST_F(RenderTest, ScaleCopyDownPos)
{
    // clang-format off
    argb_t src[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x11, 0x12, 0x13,
        0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33,
    };
    // clang-format on

    const struct pixmap pm = { pixmap_argb, 4, 4, src };
    ScaleCopy(aa_bilinear, pm, 2, 2, 0.5, 1, 1);
}

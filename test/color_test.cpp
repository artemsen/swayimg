// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "color.hpp"

#include <gtest/gtest.h>

TEST(ColorTest, ARGB)
{
    const argb_t color0;
    EXPECT_EQ(color0, 0U);
    EXPECT_FALSE(color0);

    const argb_t color1(0xaabbccdd);
    EXPECT_EQ(color1.a, 0xaa);
    EXPECT_EQ(color1.r, 0xbb);
    EXPECT_EQ(color1.g, 0xcc);
    EXPECT_EQ(color1.b, 0xdd);
    EXPECT_TRUE(color1);

    const argb_t color2(0xaa, 0xbb, 0xcc, 0xdd);
    EXPECT_EQ(color2.a, 0xaa);
    EXPECT_EQ(color2.r, 0xbb);
    EXPECT_EQ(color2.g, 0xcc);
    EXPECT_EQ(color2.b, 0xdd);
    EXPECT_TRUE(color2);
}

TEST(ArgbTest, ABGRBlending)
{
    argb_t bg;
    argb_t fg;

    // fully transparent foreground: result should be background
    fg = argb_t(0x00, 0xff, 0xff, 0xff);
    bg = argb_t(0xff, 0x00, 0x00, 0x00);
    bg.blend(fg);
    EXPECT_EQ(bg.a, 0xff);
    EXPECT_EQ(bg.r, 0);
    EXPECT_EQ(bg.g, 0);
    EXPECT_EQ(bg.b, 0);

    // fully opaque foreground: result should be foreground
    fg = argb_t(0xff, 0xff, 0xff, 0xff);
    bg = argb_t(0xff, 0x00, 0x00, 0x00);
    bg.blend(fg);
    EXPECT_EQ(bg.a, 0xff);
    EXPECT_EQ(bg.r, 0xff);
    EXPECT_EQ(bg.g, 0xff);
    EXPECT_EQ(bg.b, 0xff);

    // 50% blending (alpha 127/128)
    fg = argb_t(0x80, 0x00, 0x00, 0x00);
    bg = argb_t(0xff, 0xff, 0xff, 0xff);
    bg.blend(fg);
    EXPECT_NEAR(bg.r, 0x7f, 1);
    EXPECT_EQ(bg.a, 0xff);
}

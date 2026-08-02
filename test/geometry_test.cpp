// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "geometry.hpp"

#include <gtest/gtest.h>

TEST(PointTest, Validation)
{
    Point pt;
    EXPECT_FALSE(pt);

    pt.x = 10;
    EXPECT_FALSE(pt);

    pt.y = 20;
    EXPECT_TRUE(pt);
}

TEST(PointTest, Add)
{
    const Point a = { .x = 10, .y = 20 };
    const Point b = { .x = 5, .y = -3 };
    const Point c = a + b;
    EXPECT_EQ(c.x, 15);
    EXPECT_EQ(c.y, 17);
}

TEST(PointTest, Subtract)
{
    const Point a = { .x = 10, .y = 20 };
    const Point b = { .x = 5, .y = 8 };
    const Point c = a - b;
    EXPECT_EQ(c.x, 5);
    EXPECT_EQ(c.y, 12);
}

TEST(SizeTest, Validation)
{
    Size sz;
    EXPECT_FALSE(sz);

    sz.width = 10;
    EXPECT_FALSE(sz);

    sz.height = 20;
    EXPECT_TRUE(sz);
}

TEST(SizeTest, Scale)
{
    const Size sz = { .width = 50, .height = 100 };
    const Size scaled = sz * 2.0;
    EXPECT_EQ(scaled.width, 100UL);
    EXPECT_EQ(scaled.height, 200UL);
}

TEST(SizeTest, ScaleZero)
{
    const Size sz = { .width = 50, .height = 100 };
    const Size scaled = sz * 0.0;
    EXPECT_FALSE(scaled);
}

TEST(RectangleTest, Validation)
{
    Rectangle rect;
    EXPECT_FALSE(rect);

    rect.x = 10;
    rect.y = 20;
    EXPECT_FALSE(rect);

    rect.width = 100;
    rect.height = 200;
    EXPECT_TRUE(rect);
}

TEST(RectangleTest, DefaultValues)
{
    const Rectangle rect;
    EXPECT_EQ(rect.x, Rectangle::npos);
    EXPECT_EQ(rect.y, Rectangle::npos);
    EXPECT_EQ(rect.width, 0UL);
    EXPECT_EQ(rect.height, 0UL);
    EXPECT_FALSE(rect);
    EXPECT_FALSE(rect.position_valid());
    EXPECT_FALSE(rect.size_valid());
}

TEST(RectangleTest, ConstructFromPointSize)
{
    const Rectangle rect(Point { .x = 5, .y = 10 },
                         Size { .width = 20, .height = 30 });
    EXPECT_EQ(rect.x, 5);
    EXPECT_EQ(rect.y, 10);
    EXPECT_EQ(rect.width, 20UL);
    EXPECT_EQ(rect.height, 30UL);
}

TEST(RectangleTest, CutoutCenter)
{
    // window 100x100, cutout center 40x40
    const Rectangle rect(0, 0, 100, 100);
    const Rectangle cut(30, 30, 40, 40);
    auto [top, bottom, left, right] = rect.cutout(cut);

    // top: full width, above cut
    EXPECT_TRUE(top);
    EXPECT_EQ(top.x, 0);
    EXPECT_EQ(top.y, 0);
    EXPECT_EQ(top.width, 100UL);
    EXPECT_EQ(top.height, 30UL);

    // bottom: full width, below cut
    EXPECT_TRUE(bottom);
    EXPECT_EQ(bottom.x, 0);
    EXPECT_EQ(bottom.y, 70);
    EXPECT_EQ(bottom.width, 100UL);
    EXPECT_EQ(bottom.height, 30UL);

    // left: left of cut, cut height
    EXPECT_TRUE(left);
    EXPECT_EQ(left.x, 0);
    EXPECT_EQ(left.y, 30);
    EXPECT_EQ(left.width, 30UL);
    EXPECT_EQ(left.height, 40UL);

    // right: right of cut, cut height
    EXPECT_TRUE(right);
    EXPECT_EQ(right.x, 70);
    EXPECT_EQ(right.y, 30);
    EXPECT_EQ(right.width, 30UL);
    EXPECT_EQ(right.height, 40UL);
}

TEST(RectangleTest, CutoutAtOrigin)
{
    const Rectangle rect(0, 0, 100, 100);
    const Rectangle cut(0, 0, 50, 50);
    auto [top, bottom, left, right] = rect.cutout(cut);

    // no top (cut starts at top)
    EXPECT_FALSE(top);

    // no left (cut starts at left)
    EXPECT_FALSE(left);

    // bottom area below cut
    EXPECT_TRUE(bottom);
    EXPECT_EQ(bottom.x, 0);
    EXPECT_EQ(bottom.y, 50);
    EXPECT_EQ(bottom.width, 100UL);
    EXPECT_EQ(bottom.height, 50UL);

    // right area to the right of cut
    EXPECT_TRUE(right);
    EXPECT_EQ(right.x, 50);
    EXPECT_EQ(right.y, 0);
    EXPECT_EQ(right.width, 50UL);
    EXPECT_EQ(right.height, 50UL);
}

TEST(RectangleTest, CutoutFullSize)
{
    const Rectangle rect(0, 0, 100, 100);
    const Rectangle cut(0, 0, 100, 100);
    auto [top, bottom, left, right] = rect.cutout(cut);

    EXPECT_FALSE(top);
    EXPECT_FALSE(bottom);
    EXPECT_FALSE(left);
    EXPECT_FALSE(right);
}

TEST(RectangleTest, Intersection)
{
    // partial overlap
    const Rectangle partial =
        Rectangle { -2, -3, 10, 11 }.intersect({ 5, 6, 9, 10 });
    EXPECT_EQ(partial.x, 5);
    EXPECT_EQ(partial.y, 6);
    EXPECT_EQ(partial.width, 3UL);
    EXPECT_EQ(partial.height, 2UL);

    // no overlap (completely outside)
    const Rectangle out =
        Rectangle { 0, 0, 10, 10 }.intersect({ 20, 20, 5, 5 });
    EXPECT_FALSE(out);

    // one contains another
    const Rectangle contain =
        Rectangle { 2, 3, 4, 5 }.intersect({ 0, 0, 10, 10 });
    EXPECT_EQ(contain.x, 2);
    EXPECT_EQ(contain.y, 3);
    EXPECT_EQ(contain.width, 4UL);
    EXPECT_EQ(contain.height, 5UL);

    // edge touch (no actual area overlap)
    const Rectangle edge =
        Rectangle { 0, 0, 10, 10 }.intersect({ 10, 0, 5, 5 });
    EXPECT_FALSE(edge);
}

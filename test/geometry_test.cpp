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

TEST(SizeTest, Validation)
{
    Size sz;
    EXPECT_FALSE(sz);

    sz.width = 10;
    EXPECT_FALSE(sz);

    sz.height = 20;
    EXPECT_TRUE(sz);
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

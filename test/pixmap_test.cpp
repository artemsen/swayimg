// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "pixmap.hpp"

#include <gtest/gtest.h>

#include <format>

::testing::AssertionResult CheckPixmap(const Pixmap& pm,
                                       const std::vector<argb_t>& expect)
{
    if (pm.width() * pm.height() != expect.size()) {
        return ::testing::AssertionFailure()
            << "Wrong pixmap size: " << expect.size() << " expected "
            << pm.width() * pm.height();
    }
    for (size_t y = 0; y < pm.height(); ++y) {
        for (size_t x = 0; x < pm.width(); ++x) {
            if (pm.at(x, y) != expect[y * pm.width() + x]) {
                return ::testing::AssertionFailure()
                    << std::format(
                           "{}",
                           *reinterpret_cast<const uint32_t*>(pm.ptr(x, y)))
                    << " != "
                    << std::format("{}",
                                   *reinterpret_cast<const uint32_t*>(
                                       &expect[y * pm.width() + x]))
                    << " at x:" << x << ",y:" << y;
            }
        }
    }

    return ::testing::AssertionSuccess();
}

#define EXPECT_PMEQ(pm, ex) \
    EXPECT_TRUE(CheckPixmap(pm, ex)) << "Pixmap:\n" << to_string(pm)

static std::string to_string(const Pixmap& pm)
{
    std::string res;
    for (size_t y = 0; y < pm.height(); ++y) {
        for (size_t x = 0; x < pm.width(); ++x) {
            res += std::format(
                " {}", *reinterpret_cast<const uint32_t*>((pm.ptr(x, y))));
        }
        res += '\n';
    }
    return res;
}

TEST(PixmapTest, Create)
{
    Pixmap pm;
    pm.create(Pixmap::ARGB, 10, 5);
    EXPECT_EQ(pm.width(), 10UL);
    EXPECT_EQ(pm.height(), 5UL);
    EXPECT_EQ(pm.stride(), 40UL);
    EXPECT_TRUE(pm);

    EXPECT_TRUE(pm.ptr(0, 0));
    EXPECT_TRUE(pm.ptr(9, 4));
    EXPECT_EQ(pm.at(0, 0), 0);
    EXPECT_EQ(pm.at(9, 4), 0);
}

TEST(PixmapTest, Attach)
{
    std::vector<argb_t> data = { 1, 2, 3, 4 };
    Pixmap pm;
    pm.attach(Pixmap::ARGB, 2, 2, data.data());
    EXPECT_EQ(pm.width(), 2UL);
    EXPECT_EQ(pm.height(), 2UL);
    EXPECT_EQ(pm.stride(), 8UL);
    EXPECT_PMEQ(pm, data);
}

TEST(PixmapTest, Free)
{
    Pixmap pm;
    pm.create(Pixmap::ARGB, 10, 10);
    pm.free();
    EXPECT_EQ(pm.width(), 0UL);
    EXPECT_EQ(pm.height(), 0UL);
    EXPECT_EQ(pm.stride(), 0UL);
    EXPECT_FALSE(pm);
}

TEST(PixmapTest, Fill)
{
    Pixmap pm;
    pm.create(Pixmap::ARGB, 4, 4);
    pm.fill({ 1, 1, 10, 10 }, 1);

    // clang-format off
    const std::vector<argb_t> expect = {
        0, 0, 0, 0,
        0, 1, 1, 1,
        0, 1, 1, 1,
        0, 1, 1, 1,
    };
    // clang-format on

    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, FlipHorizontal)
{
    // clang-format off
    std::vector<argb_t> source = {
        1, 0, 0, 2,
        0, 1, 0, 0,
        0, 0, 1, 0,
        3, 0, 0, 1,
    };
    const std::vector<argb_t> expect = {
        2, 0, 0, 1,
        0, 0, 1, 0,
        0, 1, 0, 0,
        1, 0, 0, 3,
    };
    // clang-format on

    Pixmap pm;
    pm.attach(Pixmap::ARGB, 4, 4, source.data());
    pm.flip_horizontal();
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, FlipVertical)
{
    // clang-format off
    std::vector<argb_t> source = {
        1, 0, 0, 2,
        0, 1, 0, 0,
        0, 0, 1, 0,
        3, 0, 0, 1,
    };
    const std::vector<argb_t> expect = {
        3, 0, 0, 1,
        0, 0, 1, 0,
        0, 1, 0, 0,
        1, 0, 0, 2,
    };
    // clang-format on

    Pixmap pm;
    pm.attach(Pixmap::ARGB, 4, 4, source.data());
    pm.flip_vertical();
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Rotate90)
{
    // clang-format off
    std::vector<argb_t> source = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 0, 1,
    };
    const std::vector<argb_t> expect = {
        8, 4, 0,
        9, 5, 1,
        0, 6, 2,
        1, 7, 3,
    };
    // clang-format on

    Pixmap pm;
    pm.attach(Pixmap::ARGB, 4, 3, source.data());
    pm.rotate(90);
    EXPECT_EQ(pm.width(), 3UL);
    EXPECT_EQ(pm.height(), 4UL);
    EXPECT_EQ(pm.stride(), 12UL);
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Rotate180)
{
    // clang-format off
    std::vector<argb_t> source = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 0, 1,
    };
    const std::vector<argb_t> expect = {
        1, 0, 9, 8,
        7, 6, 5, 4,
        3, 2, 1, 0,
    };
    // clang-format on

    Pixmap pm;
    pm.attach(Pixmap::ARGB, 4, 3, source.data());
    pm.rotate(180);
    EXPECT_EQ(pm.width(), 4UL);
    EXPECT_EQ(pm.height(), 3UL);
    EXPECT_EQ(pm.stride(), 16UL);
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Rotate270)
{
    // clang-format off
    std::vector<argb_t> source = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 0, 1,
    };
    const std::vector<argb_t> expect = {
        3, 7, 1,
        2, 6, 0,
        1, 5, 9,
        0, 4, 8,
    };
    // clang-format on

    Pixmap pm;
    pm.attach(Pixmap::ARGB, 4, 3, source.data());
    pm.rotate(270);
    EXPECT_EQ(pm.width(), 3UL);
    EXPECT_EQ(pm.height(), 4UL);
    EXPECT_EQ(pm.stride(), 12UL);
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Grid)
{
    // clang-format off
    const std::vector<argb_t> expect = {
        0, 0, 0, 0,
        0, 2, 1, 2,
        0, 1, 2, 1,
        0, 2, 1, 2,
    };
    // clang-format on

    Pixmap pm;
    pm.create(Pixmap::ARGB, 4, 4);
    pm.grid({ 1, 1, 10, 10 }, 1, 1, 2);
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Rectangle)
{
    // clang-format off
    const std::vector<argb_t> expect = {
        0, 0, 0, 0,
        0, 1, 1, 1,
        0, 1, 0, 0,
        0, 1, 0, 0,
    };
    // clang-format on

    Pixmap pm;
    pm.create(Pixmap::ARGB, 4, 4);
    pm.rectangle({ 1, 1, 10, 10 }, 1, 1);
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Filter)
{
    // clang-format off
    const std::vector<argb_t> expect = {
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    };
    // clang-format on

    Pixmap pm;
    pm.create(Pixmap::ARGB, 4, 4);
    pm.foreach([](argb_t& c) {
        c = 1;
    });
    EXPECT_PMEQ(pm, expect);
}

TEST(PixmapTest, Copy)
{
    // clang-format off
    const std::vector<argb_t> expect = {
        0, 0, 0, 0,
        0, 1, 1, 1,
        0, 1, 1, 1,
        0, 1, 1, 1,
    };
    // clang-format on

    Pixmap fg;
    fg.create(Pixmap::ARGB, 4, 4);
    fg.fill({ 0, 0, 4, 4 }, 1);

    Pixmap bg;
    bg.create(Pixmap::ARGB, 4, 4);
    bg.copy(fg, 1, 1);

    EXPECT_PMEQ(bg, expect);
}

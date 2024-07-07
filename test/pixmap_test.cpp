// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "pixmap.h"
}

#include <gtest/gtest.h>

class Pixmap : public ::testing::Test {
protected:
    void TearDown() override { pixmap_free(&pm); }
    struct pixmap pm;
};

TEST_F(Pixmap, Create)
{
    ASSERT_TRUE(pixmap_create(&pm, 123, 456));
    ASSERT_NE(pm.data, nullptr);
    ASSERT_EQ(pm.width, 123);
    ASSERT_EQ(pm.height, 456);
}

TEST_F(Pixmap, Fill)
{
    ASSERT_TRUE(pixmap_create(&pm, 4, 4));
    pixmap_fill(&pm, 1, 1, 1, 1, 0x12345678);
}

TEST_F(Pixmap, InverseFill)
{
    ASSERT_TRUE(pixmap_create(&pm, 4, 4));
    pixmap_inverse_fill(&pm, 1, 1, 1, 1, 0x12345678);
}

TEST_F(Pixmap, Grid)
{
    ASSERT_TRUE(pixmap_create(&pm, 4, 4));
    pixmap_grid(&pm, 0, 0, 1, 1, 1, 0x12345678, 0x87654321);
}

TEST_F(Pixmap, Mask)
{
    ASSERT_TRUE(pixmap_create(&pm, 4, 4));
    const uint8_t mask[] = { 0, 1, 2, 3, 4 };
    pixmap_apply_mask(&pm, 0, 0, mask, 2, 2, 0x12345678);
}

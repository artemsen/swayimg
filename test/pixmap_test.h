// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

extern "C" {
#include "pixmap.h"
}

#include <gtest/gtest.h>

class PixmapTest : public ::testing::Test {
protected:
    void Compare(const struct pixmap& pm, const argb_t* expect) const
    {
        for (size_t y = 0; y < pm.height; ++y) {
            for (size_t x = 0; x < pm.width; ++x) {
                char expected[32], real[32];
                snprintf(expected, sizeof(expected), "y=%ld,x=%ld,c=%08x", y, x,
                         expect[y * pm.height + x]);
                snprintf(real, sizeof(real), "y=%ld,x=%ld,c=%08x", y, x,
                         pm.data[y * pm.height + x]);
                EXPECT_STREQ(expected, real);
            }
        }
    }
};

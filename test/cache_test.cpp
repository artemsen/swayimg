// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "cache.h"
#include "formats/loader.h"
#include "list.h"
}

#include <gtest/gtest.h>

class Cache : public ::testing::Test {
protected:
    void SetUp() override
    {
        for (size_t i = 0; i < sizeof(image) / sizeof(image[0]); ++i) {
            const std::string name = "image" + std::to_string(i);
            image[i] = image_create(name.c_str());
            image_list =
                static_cast<struct image*>(list_append(image_list, image[i]));
        }
    }

    void TearDown() override
    {
        cache_free(cache);
        list_for_each(image_list, struct image, it) {
            image_free(it, IMGFREE_ALL);
        }
    }

    struct cache* cache;
    struct image* image[5];
    struct image* image_list = nullptr;
};

TEST_F(Cache, Init)
{
    ASSERT_FALSE(cache_init(0));
    ASSERT_EQ(cache_capacity(nullptr), static_cast<size_t>(0));

    cache = cache_init(3);
    ASSERT_TRUE(cache);
    EXPECT_EQ(cache_capacity(cache), static_cast<size_t>(3));
}

TEST_F(Cache, Trim)
{
    cache = cache_init(5);
    ASSERT_TRUE(cache);

    for (auto i : image) {
        image_alloc_frame(i, 1, 1);
        ASSERT_TRUE(cache_put(cache, i));
    }

    cache_trim(cache, 3);

    EXPECT_FALSE(image_has_frames(image[0]));
    EXPECT_FALSE(image_has_frames(image[1]));
    EXPECT_TRUE(image_has_frames(image[2]));
    EXPECT_TRUE(image_has_frames(image[3]));
    EXPECT_TRUE(image_has_frames(image[4]));
}

TEST_F(Cache, Put)
{
    cache = cache_init(3);
    ASSERT_TRUE(cache);

    for (auto i : image) {
        image_alloc_frame(i, 1, 1);
        ASSERT_TRUE(cache_put(cache, i));
    }

    EXPECT_FALSE(image_has_frames(image[0]));
    EXPECT_FALSE(image_has_frames(image[1]));
    EXPECT_TRUE(image_has_frames(image[2]));
    EXPECT_TRUE(image_has_frames(image[3]));
    EXPECT_TRUE(image_has_frames(image[4]));
}

TEST_F(Cache, Out)
{
    cache = cache_init(3);
    ASSERT_TRUE(cache);

    EXPECT_TRUE(cache_put(cache, image[0]));

    EXPECT_TRUE(cache_out(cache, image[0]));
    EXPECT_FALSE(cache_out(cache, image[1]));
}

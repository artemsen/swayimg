// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "cache.h"
#include "formats/loader.h"
#include "imglist.h"
}

#include "config_test.h"

class Cache : public ConfigTest {
protected:
    void SetUp() override
    {
        config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
        imglist_init(config);
        for (size_t i = 0; i < sizeof(image) / sizeof(image[0]); ++i) {
            const std::string name = "exec://image" + std::to_string(i);
            image[i] = imglist_add(name.c_str());
        }
    }

    void TearDown() override
    {
        cache_free(cache);
        imglist_destroy();
    }

    struct cache* cache = nullptr;
    struct image* image[5];
};

TEST_F(Cache, Init)
{
    ASSERT_FALSE(cache_init(0));
    ASSERT_EQ(cache_capacity(nullptr), static_cast<size_t>(0));

    cache = cache_init(3);
    ASSERT_TRUE(cache);
    EXPECT_EQ(cache_capacity(cache), static_cast<size_t>(3));
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

TEST_F(Cache, Out)
{
    cache = cache_init(3);
    ASSERT_TRUE(cache);

    image_alloc_frame(image[0], 1, 1);
    EXPECT_TRUE(cache_put(cache, image[0]));

    EXPECT_TRUE(cache_out(cache, image[0]));
    EXPECT_FALSE(cache_out(cache, image[1]));
}

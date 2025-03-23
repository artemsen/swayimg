// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "cache.h"
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
            image_deref(it);
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
    ASSERT_EQ(cache_capacity(cache), static_cast<size_t>(3));
}

TEST_F(Cache, Put)
{
    cache = cache_init(3);
    ASSERT_TRUE(cache);

    ASSERT_EQ(image[0]->ref_count, static_cast<size_t>(1));

    image_addref(image[0]);
    ASSERT_TRUE(cache_put(cache, image[0]));
    image_addref(image[1]);
    ASSERT_TRUE(cache_put(cache, image[1]));
    image_addref(image[2]);
    ASSERT_TRUE(cache_put(cache, image[2]));

    image[0]->format = static_cast<char*>(malloc(1));
    ASSERT_TRUE(image[0]->format);
    ASSERT_EQ(image[0]->ref_count, static_cast<size_t>(2));

    image_addref(image[3]);
    ASSERT_TRUE(cache_put(cache, image[3]));
    image_addref(image[4]);
    ASSERT_TRUE(cache_put(cache, image[4]));

    ASSERT_EQ(image[0]->ref_count, static_cast<size_t>(1));
    ASSERT_FALSE(image[0]->format);
}

TEST_F(Cache, Out)
{
    cache = cache_init(3);
    ASSERT_TRUE(cache);

    image_addref(image[0]);
    ASSERT_TRUE(cache_put(cache, image[0]));
    ASSERT_EQ(image[0]->ref_count, static_cast<size_t>(2));

    ASSERT_TRUE(cache_out(cache, image[0]));
    ASSERT_EQ(image[0]->ref_count, static_cast<size_t>(2));
    image_deref(image[0]);

    ASSERT_FALSE(cache_out(cache, image[1]));
}

TEST_F(Cache, Trim)
{
    cache = cache_init(5);
    ASSERT_TRUE(cache);

    for (auto i : image) {
        image_addref(i);
        ASSERT_TRUE(cache_put(cache, i));
    }

    cache_trim(cache, 3);

    ASSERT_EQ(image[0]->ref_count, static_cast<size_t>(1));
    ASSERT_EQ(image[1]->ref_count, static_cast<size_t>(1));
    ASSERT_EQ(image[2]->ref_count, static_cast<size_t>(2));
    ASSERT_EQ(image[3]->ref_count, static_cast<size_t>(2));
    ASSERT_EQ(image[4]->ref_count, static_cast<size_t>(2));
}

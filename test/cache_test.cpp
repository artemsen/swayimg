// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "cache.h"
}

#include <gtest/gtest.h>

class Cache : public ::testing::Test {
protected:
    static constexpr size_t capacity = 3;
    void SetUp() override { cache_init(&cache, capacity); }
    void TearDown() override { cache_free(&cache); }
    struct cache_queue cache;
};

TEST_F(Cache, Create)
{
    ASSERT_EQ(cache.capacity, 3);
    EXPECT_EQ(cache.queue[0].image, nullptr);
    EXPECT_EQ(cache.queue[1].image, nullptr);
    EXPECT_EQ(cache.queue[2].image, nullptr);
}

TEST_F(Cache, Reset)
{
    for (size_t i = 0; i < capacity; ++i) {
        cache_put(&cache, image_create(), i);
    }
    EXPECT_NE(cache.queue[0].image, nullptr);
    EXPECT_NE(cache.queue[1].image, nullptr);
    EXPECT_NE(cache.queue[2].image, nullptr);

    cache_reset(&cache);

    EXPECT_EQ(cache.queue[0].image, nullptr);
    EXPECT_EQ(cache.queue[1].image, nullptr);
    EXPECT_EQ(cache.queue[2].image, nullptr);
}

TEST_F(Cache, IsFull)
{
    for (size_t i = 0; i < capacity; ++i) {
        EXPECT_FALSE(cache_full(&cache));
        cache_put(&cache, image_create(), i);
    }
    EXPECT_TRUE(cache_full(&cache));
}

TEST_F(Cache, Put)
{
    for (size_t i = 0; i < 3; ++i) {
        struct image* image = image_create();
        cache_put(&cache, image, i);
        EXPECT_EQ(cache.queue[i].image, image);
        EXPECT_EQ(cache.queue[i].index, i);
    }

    struct image* image = image_create();
    cache_put(&cache, image, 42);

    EXPECT_EQ(cache.queue[2].image, image);
    EXPECT_EQ(cache.queue[2].index, 42);
    EXPECT_NE(cache.queue[1].image, nullptr);
    EXPECT_EQ(cache.queue[1].index, 2);
    EXPECT_NE(cache.queue[0].image, nullptr);
    EXPECT_EQ(cache.queue[0].index, 1);
}

TEST_F(Cache, Get)
{
    for (size_t i = 0; i < 3; ++i) {
        cache_put(&cache, image_create(), i);
    }

    struct image* image = cache_get(&cache, 1);
    EXPECT_NE(image, nullptr);
    image_free(image);

    EXPECT_EQ(cache.queue[2].image, nullptr);
    EXPECT_NE(cache.queue[1].image, nullptr);
    EXPECT_EQ(cache.queue[1].index, 2);
    EXPECT_NE(cache.queue[0].image, nullptr);
    EXPECT_EQ(cache.queue[0].index, 0);

    EXPECT_EQ(cache_get(&cache, 1), nullptr);
}

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
        imglist_load(images, imgnum);
    }

    void TearDown() override
    {
        cache_free(cache);
        imglist_destroy();
    }

    struct cache* cache = nullptr;

    static constexpr size_t imgnum = 5;
    static constexpr const char* const images[imgnum] = {
        "exec://1", "exec://2", "exec://3", "exec://4", "exec://5",
    };
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

    struct image* img = imglist_first();
    ASSERT_TRUE(img);

    for (size_t i = 0; i < imgnum; ++i) {
        img->data = static_cast<struct imgdata*>(calloc(1, sizeof(*img->data)));
        ASSERT_TRUE(image_alloc_frame(img->data, pixmap_argb, 1, 1));
        ASSERT_TRUE(cache_put(cache, img));
        img = imglist_next(img);
    }

    img = imglist_first();
    EXPECT_FALSE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_FALSE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_TRUE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_TRUE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_TRUE(image_has_frames(img));
}

TEST_F(Cache, Trim)
{
    cache = cache_init(5);
    ASSERT_TRUE(cache);

    struct image* img = imglist_first();
    ASSERT_TRUE(img);

    for (size_t i = 0; i < imgnum; ++i) {
        img->data = static_cast<struct imgdata*>(calloc(1, sizeof(*img->data)));
        ASSERT_TRUE(image_alloc_frame(img->data, pixmap_argb, 1, 1));
        ASSERT_TRUE(cache_put(cache, img));
        img = imglist_next(img);
    }

    cache_trim(cache, 3);

    img = imglist_first();
    EXPECT_FALSE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_FALSE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_TRUE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_TRUE(image_has_frames(img));
    img = imglist_next(img);
    EXPECT_TRUE(image_has_frames(img));
}

TEST_F(Cache, Out)
{
    cache = cache_init(3);
    ASSERT_TRUE(cache);

    struct image* img = imglist_first();
    ASSERT_TRUE(img);
    img->data = static_cast<struct imgdata*>(calloc(1, sizeof(*img->data)));
    ASSERT_TRUE(image_alloc_frame(img->data, pixmap_argb, 1, 1));

    EXPECT_TRUE(cache_put(cache, img));

    EXPECT_TRUE(cache_out(cache, img));
    EXPECT_FALSE(cache_out(cache, imglist_last()));
}

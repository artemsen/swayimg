// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "imglist.h"
}

#include "config_test.h"

class ImageList : public ConfigTest {
protected:
    void TearDown() override { imglist_destroy(); }
};

TEST_F(ImageList, Load)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);
    ASSERT_EQ(imglist_size(), static_cast<size_t>(0));

    const char* const img[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(img, sizeof(img) / sizeof(img[0])));
    ASSERT_EQ(imglist_size(), static_cast<size_t>(3));

    EXPECT_STREQ(imglist_first()->source, img[0]);
    EXPECT_STREQ(imglist_last()->source, img[2]);
}

TEST_F(ImageList, Duplicate)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://1",
        "exec://2",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    ASSERT_EQ(imglist_size(), static_cast<size_t>(2));
    EXPECT_STREQ(imglist_first()->source, "exec://1");
    EXPECT_STREQ(imglist_last()->source, "exec://2");
}

TEST_F(ImageList, SortAlpha)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_REVERSE, CFG_NO);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://3",
        "exec://1",
        "exec://2",
        "exec://4",
    };
    struct image* img =
        imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0]));
    ASSERT_TRUE(img);
    EXPECT_STREQ(img->source, "exec://3");

    img = imglist_first();
    for (size_t i = 1; i <= 4; ++i) {
        const std::string src = "exec://" + std::to_string(i);
        ASSERT_TRUE(img);
        EXPECT_STREQ(img->source, src.c_str());
        img = static_cast<struct image*>(list_next(img));
    }

    EXPECT_FALSE(img);
}

TEST_F(ImageList, SortAlphaReverse)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_REVERSE, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://3",
        "exec://1",
        "exec://2",
        "exec://4",
    };
    struct image* img =
        imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0]));
    ASSERT_TRUE(img);
    EXPECT_STREQ(img->source, "exec://3");

    img = imglist_first();
    for (size_t i = 4; i >= 1; --i) {
        const std::string src = "exec://" + std::to_string(i);
        ASSERT_TRUE(img);
        EXPECT_STREQ(img->source, src.c_str());
        img = static_cast<struct image*>(list_next(img));
    }

    EXPECT_FALSE(img);
}

TEST_F(ImageList, SortNumeric)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "numeric");
    config_set(config, CFG_LIST, CFG_LIST_REVERSE, CFG_NO);
    imglist_init(config);

    // clang-format off
    const char* const imglist[] = {
        "exec://3",
        "exec://a1",
        "exec://10a10",
        "exec://1",
        "exec://20",
        "exec://b0",
        "exec://10a1",
    };
    const char* const etalon[] = {
        "exec://1",
        "exec://3",
        "exec://10a1",
        "exec://10a10",
        "exec://20",
        "exec://a1",
        "exec://b0",
    };
    // clang-format on

    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img = imglist_first();
    for (auto e : etalon) {
        ASSERT_TRUE(img);
        ASSERT_STREQ(img->source, e);
        img = imglist_next(img);
    }
}

TEST_F(ImageList, SortNumericReverse)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "numeric");
    config_set(config, CFG_LIST, CFG_LIST_REVERSE, CFG_YES);
    imglist_init(config);

    // clang-format off
    const char* const imglist[] = {
        "exec://3",
        "exec://10a10",
        "exec://1",
        "exec://20",
        "exec://10a1",
    };
    const char* const etalon[] = {
        "exec://20",
        "exec://10a10",
        "exec://10a1",
        "exec://3",
        "exec://1",
    };
    // clang-format on

    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img = imglist_first();
    for (auto e : etalon) {
        ASSERT_TRUE(img);
        ASSERT_STREQ(img->source, e);
        img = imglist_next(img);
    }
}

TEST_F(ImageList, Find)
{
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    const struct image* img = imglist_find("exec://1");
    ASSERT_TRUE(img);
    EXPECT_STREQ(img->source, "exec://1");

    EXPECT_TRUE(imglist_find("exec://2"));
    EXPECT_FALSE(imglist_find("not_exist"));
}

TEST_F(ImageList, Remove)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    imglist_remove(imglist_first());
    EXPECT_EQ(imglist_size(), static_cast<size_t>(2));

    imglist_remove(imglist_last());
    EXPECT_EQ(imglist_size(), static_cast<size_t>(1));

    imglist_remove(imglist_first());
    EXPECT_EQ(imglist_size(), static_cast<size_t>(0));
}

TEST_F(ImageList, Next)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    EXPECT_EQ(imglist_next(imglist_find("exec://1")), imglist_find("exec://2"));
    EXPECT_EQ(imglist_next(imglist_find("exec://2")), nullptr);
}

TEST_F(ImageList, Prev)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    EXPECT_EQ(imglist_prev(imglist_find("exec://2")), imglist_find("exec://1"));
    EXPECT_EQ(imglist_prev(imglist_find("exec://1")), nullptr);
}

TEST_F(ImageList, NextFile)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };

    EXPECT_EQ(imglist_next_file(img[0]), img[1]);
    EXPECT_EQ(imglist_next_file(img[1]), img[2]);
    EXPECT_EQ(imglist_next_file(img[2]), nullptr);
}

TEST_F(ImageList, NextFileLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };

    EXPECT_EQ(imglist_next_file(img[0]), img[1]);
    EXPECT_EQ(imglist_next_file(img[1]), img[2]);
    EXPECT_EQ(imglist_next_file(img[2]), img[0]);
}

TEST_F(ImageList, NextFileLoopSelf)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = { "exec://1" };
    struct image* img =
        imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0]));
    ASSERT_TRUE(img);

    EXPECT_FALSE(imglist_next_file(img));
}

TEST_F(ImageList, PrevFile)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };
    EXPECT_EQ(imglist_prev_file(img[0]), nullptr);
    EXPECT_EQ(imglist_prev_file(img[2]), img[1]);
    EXPECT_EQ(imglist_prev_file(img[1]), img[0]);
}

TEST_F(ImageList, PrevFileLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };
    EXPECT_EQ(imglist_prev_file(img[0]), img[2]);
    EXPECT_EQ(imglist_prev_file(img[2]), img[1]);
    EXPECT_EQ(imglist_prev_file(img[1]), img[0]);
}

TEST_F(ImageList, PrevFileLoopSelf)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = { "exec://1" };
    struct image* img =
        imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0]));
    ASSERT_TRUE(img);

    ASSERT_EQ(imglist_prev_file(img), nullptr);
}

TEST_F(ImageList, NextDir)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://123/dir1/image1",
        "exec://123/dir1/image2",
        "exec://123/dir2/image3",
        "exec://123/dir2/image4",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://123/dir1/image1"),
        imglist_find("exec://123/dir1/image2"),
        imglist_find("exec://123/dir2/image3"),
        imglist_find("exec://123/dir2/image4"),
    };
    EXPECT_EQ(imglist_next_dir(img[0]), img[2]);
    EXPECT_EQ(imglist_next_dir(img[1]), img[2]);
    EXPECT_EQ(imglist_next_dir(img[2]), nullptr);
    EXPECT_EQ(imglist_next_dir(img[3]), nullptr);
}

TEST_F(ImageList, NextDirLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://123/dir1/image1",
        "exec://123/dir1/image2",
        "exec://123/dir2/image3",
        "exec://123/dir2/image4",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://123/dir1/image1"),
        imglist_find("exec://123/dir1/image2"),
        imglist_find("exec://123/dir2/image3"),
        imglist_find("exec://123/dir2/image4"),
    };
    EXPECT_EQ(imglist_next_dir(img[0]), img[2]);
    EXPECT_EQ(imglist_next_dir(img[1]), img[2]);
    EXPECT_EQ(imglist_next_dir(img[2]), img[0]);
    EXPECT_EQ(imglist_next_dir(img[3]), img[0]);
}

TEST_F(ImageList, PrevDir)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://123/dir1/image1",
        "exec://123/dir1/image2",
        "exec://123/dir2/image3",
        "exec://123/dir2/image4",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://123/dir1/image1"),
        imglist_find("exec://123/dir1/image2"),
        imglist_find("exec://123/dir2/image3"),
        imglist_find("exec://123/dir2/image4"),
    };
    EXPECT_EQ(imglist_prev_dir(img[0]), nullptr);
    EXPECT_EQ(imglist_prev_dir(img[1]), nullptr);
    EXPECT_EQ(imglist_prev_dir(img[2]), img[1]);
    EXPECT_EQ(imglist_prev_dir(img[3]), img[1]);
}

TEST_F(ImageList, PrevDirLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    const char* const imglist[] = {
        "exec://123/dir1/image1",
        "exec://123/dir1/image2",
        "exec://123/dir2/image3",
        "exec://123/dir2/image4",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://123/dir1/image1"),
        imglist_find("exec://123/dir1/image2"),
        imglist_find("exec://123/dir2/image3"),
        imglist_find("exec://123/dir2/image4"),
    };
    EXPECT_EQ(imglist_prev_dir(img[0]), img[3]);
    EXPECT_EQ(imglist_prev_dir(img[1]), img[3]);
    EXPECT_EQ(imglist_prev_dir(img[2]), img[1]);
    EXPECT_EQ(imglist_prev_dir(img[3]), img[1]);
}

TEST_F(ImageList, GetRandom)
{
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };

    for (auto i : img) {
        const struct image* ri = imglist_rand(i);
        ASSERT_TRUE(ri);
        EXPECT_NE(ri, i);
    }
}

TEST_F(ImageList, Jump)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };

    EXPECT_EQ(imglist_jump(img[0], 0), img[0]);

    EXPECT_EQ(imglist_jump(img[0], 1), img[1]);
    EXPECT_EQ(imglist_jump(img[0], 2), img[2]);
    EXPECT_EQ(imglist_jump(img[0], 10), nullptr);

    EXPECT_EQ(imglist_jump(img[2], -1), img[1]);
    EXPECT_EQ(imglist_jump(img[2], -2), img[0]);
    EXPECT_EQ(imglist_jump(img[2], -10), nullptr);
}

TEST_F(ImageList, Distance)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    const char* const imglist[] = {
        "exec://1",
        "exec://2",
        "exec://3",
    };
    ASSERT_TRUE(imglist_load(imglist, sizeof(imglist) / sizeof(imglist[0])));

    struct image* img[] = {
        imglist_find("exec://1"),
        imglist_find("exec://2"),
        imglist_find("exec://3"),
    };

    EXPECT_EQ(imglist_distance(img[0], img[0]), static_cast<ssize_t>(0));

    EXPECT_EQ(imglist_distance(img[0], img[1]), static_cast<ssize_t>(1));
    EXPECT_EQ(imglist_distance(img[0], img[2]), static_cast<ssize_t>(2));

    EXPECT_EQ(imglist_distance(img[2], img[0]), static_cast<ssize_t>(-2));
    EXPECT_EQ(imglist_distance(img[1], img[0]), static_cast<ssize_t>(-1));
}

TEST_F(ImageList, Lock)
{
    imglist_init(config);
    EXPECT_FALSE(imglist_is_locked());
    imglist_lock();
    EXPECT_TRUE(imglist_is_locked());
    imglist_unlock();
    EXPECT_FALSE(imglist_is_locked());
}

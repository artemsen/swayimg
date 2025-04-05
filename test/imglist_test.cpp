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

TEST_F(ImageList, Add)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);
    ASSERT_EQ(imglist_size(), static_cast<size_t>(0));

    struct image* img[] = {
        imglist_add("exec://1"),
        imglist_add("exec://2"),
        imglist_add("exec://3"),
    };
    ASSERT_EQ(imglist_size(), static_cast<size_t>(3));

    EXPECT_EQ(imglist_first(), img[0]);
    EXPECT_EQ(imglist_last(), img[2]);
}

TEST_F(ImageList, Duplicate)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);
    imglist_add("exec://1");
    imglist_add("exec://1");
    imglist_add("exec://2");
    ASSERT_EQ(imglist_size(), static_cast<size_t>(2));
    EXPECT_STREQ(imglist_first()->source, "exec://1");
    EXPECT_STREQ(imglist_last()->source, "exec://2");
}

TEST_F(ImageList, SortAlpha)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_REVERSE, CFG_NO);
    imglist_init(config);

    imglist_add("exec://3");
    imglist_add("exec://1");
    imglist_add("exec://2");
    imglist_add("exec://4");

    struct image* img = imglist_first();

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

    imglist_add("exec://3");
    imglist_add("exec://1");
    imglist_add("exec://2");
    imglist_add("exec://4");

    struct image* img = imglist_first();

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

    const struct image* img[7];
    img[1] = imglist_add("exec://3");
    img[5] = imglist_add("exec://a1");
    img[3] = imglist_add("exec://10a10");
    img[0] = imglist_add("exec://1");
    img[4] = imglist_add("exec://20");
    img[6] = imglist_add("exec://b0");
    img[2] = imglist_add("exec://10a1");

    struct image* next = imglist_first();

    for (auto i : img) {
        ASSERT_TRUE(next);
        EXPECT_STREQ(i->source, next->source);
        next = imglist_next(next);
    }
}

TEST_F(ImageList, SortNumericReverse)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "numeric");
    config_set(config, CFG_LIST, CFG_LIST_REVERSE, CFG_YES);
    imglist_init(config);

    const struct image* img[5];
    img[3] = imglist_add("exec://3");
    img[1] = imglist_add("exec://10a10");
    img[4] = imglist_add("exec://1");
    img[0] = imglist_add("exec://20");
    img[2] = imglist_add("exec://10a1");

    struct image* next = imglist_first();

    for (auto i : img) {
        ASSERT_TRUE(next);
        EXPECT_STREQ(i->source, next->source);
        next = imglist_next(next);
    }
}

TEST_F(ImageList, Find)
{
    imglist_init(config);
    imglist_add("exec://1");
    imglist_add("exec://2");

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

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    imglist_remove(img1);
    EXPECT_EQ(imglist_size(), static_cast<size_t>(2));

    imglist_remove(img3);
    EXPECT_EQ(imglist_size(), static_cast<size_t>(1));

    imglist_remove(img2);
    EXPECT_EQ(imglist_size(), static_cast<size_t>(0));
}

TEST_F(ImageList, Next)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");

    EXPECT_EQ(imglist_next(img1), img2);
    EXPECT_EQ(imglist_next(img2), nullptr);
}

TEST_F(ImageList, Prev)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");

    EXPECT_EQ(imglist_prev(img2), img1);
    EXPECT_EQ(imglist_prev(img1), nullptr);
}

TEST_F(ImageList, NextFile)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    EXPECT_EQ(imglist_next_file(img1), img2);
    EXPECT_EQ(imglist_next_file(img2), img3);
    EXPECT_EQ(imglist_next_file(img3), nullptr);
}

TEST_F(ImageList, NextFileLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    EXPECT_EQ(imglist_next_file(img1), img2);
    EXPECT_EQ(imglist_next_file(img2), img3);
    EXPECT_EQ(imglist_next_file(img3), img1);
}

TEST_F(ImageList, NextFileLoopSelf)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img = imglist_add("exec://1");

    EXPECT_EQ(imglist_next_file(img), nullptr);
}

TEST_F(ImageList, PrevFile)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    EXPECT_EQ(imglist_prev_file(img1), nullptr);
    EXPECT_EQ(imglist_prev_file(img3), img2);
    EXPECT_EQ(imglist_prev_file(img2), img1);
}

TEST_F(ImageList, PrevFileLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    EXPECT_EQ(imglist_prev_file(img1), img3);
    EXPECT_EQ(imglist_prev_file(img3), img2);
    EXPECT_EQ(imglist_prev_file(img2), img1);
}

TEST_F(ImageList, PrevFileLoopSelf)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img = imglist_add("exec://1");

    ASSERT_EQ(imglist_prev_file(img), nullptr);
}

TEST_F(ImageList, NextDir)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    struct image* img11 = imglist_add("exec://123/dir1/image1");
    struct image* img12 = imglist_add("exec://123/dir1/image2");
    struct image* img23 = imglist_add("exec://123/dir2/image3");
    struct image* img24 = imglist_add("exec://123/dir2/image4");

    EXPECT_EQ(imglist_next_dir(img11), img23);
    EXPECT_EQ(imglist_next_dir(img12), img23);
    EXPECT_EQ(imglist_next_dir(img23), nullptr);
    EXPECT_EQ(imglist_next_dir(img24), nullptr);
}

TEST_F(ImageList, NextDirLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img11 = imglist_add("exec://123/dir1/image1");
    struct image* img12 = imglist_add("exec://123/dir1/image2");
    struct image* img23 = imglist_add("exec://123/dir2/image3");
    struct image* img24 = imglist_add("exec://123/dir2/image4");

    EXPECT_EQ(imglist_next_dir(img11), img23);
    EXPECT_EQ(imglist_next_dir(img12), img23);
    EXPECT_EQ(imglist_next_dir(img23), img11);
    EXPECT_EQ(imglist_next_dir(img24), img11);
}

TEST_F(ImageList, PrevDir)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    struct image* img11 = imglist_add("exec://123/dir1/image1");
    struct image* img12 = imglist_add("exec://123/dir1/image2");
    struct image* img23 = imglist_add("exec://123/dir2/image3");
    struct image* img24 = imglist_add("exec://123/dir2/image4");

    EXPECT_EQ(imglist_prev_dir(img11), nullptr);
    EXPECT_EQ(imglist_prev_dir(img12), nullptr);
    EXPECT_EQ(imglist_prev_dir(img23), img12);
    EXPECT_EQ(imglist_prev_dir(img24), img12);
}

TEST_F(ImageList, PrevDirLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img11 = imglist_add("exec://123/dir1/image1");
    struct image* img12 = imglist_add("exec://123/dir1/image2");
    struct image* img23 = imglist_add("exec://123/dir2/image3");
    struct image* img24 = imglist_add("exec://123/dir2/image4");

    EXPECT_EQ(imglist_prev_dir(img11), img24);
    EXPECT_EQ(imglist_prev_dir(img12), img24);
    EXPECT_EQ(imglist_prev_dir(img23), img12);
    EXPECT_EQ(imglist_prev_dir(img24), img12);
}

TEST_F(ImageList, GetRandom)
{
    imglist_init(config);

    struct image* img[] = {
        imglist_add("exec://1"),
        imglist_add("exec://2"),
        imglist_add("exec://3"),
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

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    EXPECT_EQ(imglist_jump(img1, 0), img1);

    EXPECT_EQ(imglist_jump(img1, 1), img2);
    EXPECT_EQ(imglist_jump(img1, 2), img3);
    EXPECT_EQ(imglist_jump(img1, 10), nullptr);

    EXPECT_EQ(imglist_jump(img3, -1), img2);
    EXPECT_EQ(imglist_jump(img3, -2), img1);
    EXPECT_EQ(imglist_jump(img3, -10), nullptr);
}

TEST_F(ImageList, Distance)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    EXPECT_EQ(imglist_distance(img1, img1), static_cast<ssize_t>(0));

    EXPECT_EQ(imglist_distance(img1, img2), static_cast<ssize_t>(1));
    EXPECT_EQ(imglist_distance(img1, img3), static_cast<ssize_t>(2));

    EXPECT_EQ(imglist_distance(img3, img1), static_cast<ssize_t>(-2));
    EXPECT_EQ(imglist_distance(img2, img1), static_cast<ssize_t>(-1));
}

TEST_F(ImageList, Lock)
{
    imglist_init(config);
    imglist_lock();
    imglist_unlock();
}

TEST_F(ImageList, Watch)
{
    imglist_init(config);
    imglist_watch(nullptr, nullptr);
}

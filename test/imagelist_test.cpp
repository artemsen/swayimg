// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "imagelist.h"
}

#include <gtest/gtest.h>

class ImageList : public ::testing::Test {
protected:
    void SetUp() override
    {
        unsetenv("XDG_CONFIG_HOME");
        unsetenv("XDG_CONFIG_DIRS");
        unsetenv("HOME");
        config = config_load();
        ASSERT_TRUE(config);
    }

    void TearDown() override
    {
        imglist_destroy();
        config_free(config);
    }

    struct Image {
        Image(struct image* image)
            : img(image)
        {
        }
        ~Image()
        {
            if (img) {
                image_deref(img);
            }
        }
        operator struct image *() { return img; }
        operator const struct image *() const { return img; }
        struct image* operator->() { return img; }
        struct image* img;
    };

    struct config* config;
};

TEST_F(ImageList, Add)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);
    ASSERT_EQ(imglist_size(), static_cast<size_t>(0));

    const struct image* img1 = imglist_add("exec://1");
    imglist_add("exec://2");
    const struct image* img3 = imglist_add("exec://3");
    ASSERT_EQ(imglist_size(), static_cast<size_t>(3));

    ASSERT_EQ(Image(imglist_first()), img1);
    ASSERT_EQ(Image(imglist_last()), img3);
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

    Image first(imglist_first());
    struct image* img = first;

    for (size_t i = 1; i <= 4; ++i) {
        const std::string src = "exec://" + std::to_string(i);
        ASSERT_TRUE(img);
        ASSERT_STREQ(img->source, src.c_str());
        img = static_cast<struct image*>(list_next(img));
    }

    ASSERT_FALSE(img);
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

    Image first(imglist_first());
    struct image* img = first;

    for (size_t i = 4; i >= 1; --i) {
        const std::string src = "exec://" + std::to_string(i);
        ASSERT_TRUE(img);
        ASSERT_STREQ(img->source, src.c_str());
        img = static_cast<struct image*>(list_next(img));
    }

    ASSERT_FALSE(img);
}

TEST_F(ImageList, Remove)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    imglist_remove(img1);
    ASSERT_EQ(imglist_size(), static_cast<size_t>(2));

    imglist_remove(img3);
    ASSERT_EQ(imglist_size(), static_cast<size_t>(1));

    imglist_remove(img2);
    ASSERT_EQ(imglist_size(), static_cast<size_t>(0));
}

TEST_F(ImageList, Next)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");

    ASSERT_EQ(Image(imglist_next(img1)), img2);
    ASSERT_EQ(Image(imglist_next(img2)), nullptr);
}

TEST_F(ImageList, Prev)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");

    ASSERT_EQ(Image(imglist_prev(img2)), img1);
    ASSERT_EQ(Image(imglist_prev(img1)), nullptr);
}

TEST_F(ImageList, NextFile)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    Image img = imglist_next_file(img1);
    ASSERT_EQ(img->ref_count, static_cast<size_t>(2));

    ASSERT_EQ(Image(imglist_next_file(img1)), img2);
    ASSERT_EQ(Image(imglist_next_file(img2)), img3);
    ASSERT_EQ(Image(imglist_next_file(img3)), nullptr);
}

TEST_F(ImageList, NextFileLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    Image img = imglist_next_file(img3);
    ASSERT_EQ(img->ref_count, static_cast<size_t>(2));

    ASSERT_EQ(Image(imglist_next_file(img1)), img2);
    ASSERT_EQ(Image(imglist_next_file(img2)), img3);
    ASSERT_EQ(Image(imglist_next_file(img3)), img1);
}

TEST_F(ImageList, NextFileLoopSelf)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img = imglist_add("exec://1");

    ASSERT_EQ(imglist_next_file(img), nullptr);
}

TEST_F(ImageList, PrevFile)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_NO);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    ASSERT_EQ(Image(imglist_prev_file(img1)), nullptr);
    ASSERT_EQ(Image(imglist_prev_file(img3)), img2);
    ASSERT_EQ(Image(imglist_prev_file(img2)), img1);
}

TEST_F(ImageList, PrevFileLoop)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");

    ASSERT_EQ(Image(imglist_prev_file(img1)), img3);
    ASSERT_EQ(Image(imglist_prev_file(img3)), img2);
    ASSERT_EQ(Image(imglist_prev_file(img2)), img1);
}

TEST_F(ImageList, PrevFileLoopSelf)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    config_set(config, CFG_LIST, CFG_LIST_LOOP, CFG_YES);
    imglist_init(config);

    struct image* img = imglist_add("exec://1");

    ASSERT_EQ(Image(imglist_prev_file(img)), nullptr);
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

    ASSERT_EQ(Image(imglist_next_dir(img11)), img23);
    ASSERT_EQ(Image(imglist_next_dir(img12)), img23);
    ASSERT_EQ(Image(imglist_next_dir(img23)), nullptr);
    ASSERT_EQ(Image(imglist_next_dir(img24)), nullptr);
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

    ASSERT_EQ(Image(imglist_next_dir(img11)), img23);
    ASSERT_EQ(Image(imglist_next_dir(img12)), img23);
    ASSERT_EQ(Image(imglist_next_dir(img23)), img11);
    ASSERT_EQ(Image(imglist_next_dir(img24)), img11);
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

    ASSERT_EQ(Image(imglist_prev_dir(img11)), nullptr);
    ASSERT_EQ(Image(imglist_prev_dir(img12)), nullptr);
    ASSERT_EQ(Image(imglist_prev_dir(img23)), img12);
    ASSERT_EQ(Image(imglist_prev_dir(img24)), img12);
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

    ASSERT_EQ(Image(imglist_prev_dir(img11)), img24);
    ASSERT_EQ(Image(imglist_prev_dir(img12)), img24);
    ASSERT_EQ(Image(imglist_prev_dir(img23)), img12);
    ASSERT_EQ(Image(imglist_prev_dir(img24)), img12);
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
        Image img = imglist_rand(i);
        ASSERT_TRUE(img);
        ASSERT_NE(img, i);
    }
}

TEST_F(ImageList, Forward)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");
    struct image* img4 = imglist_add("exec://4");

    ASSERT_EQ(Image(imglist_fwd(img1, 0)), img1);
    ASSERT_EQ(Image(imglist_fwd(img1, 1)), img2);
    ASSERT_EQ(Image(imglist_fwd(img1, 2)), img3);
    ASSERT_EQ(Image(imglist_fwd(img1, 1000)), img4);
    ASSERT_EQ(Image(imglist_fwd(img4, 1000)), img4);
}

TEST_F(ImageList, Backward)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");
    struct image* img4 = imglist_add("exec://4");

    ASSERT_EQ(Image(imglist_back(img4, 0)), img4);
    ASSERT_EQ(Image(imglist_back(img4, 1)), img3);
    ASSERT_EQ(Image(imglist_back(img4, 2)), img2);
    ASSERT_EQ(Image(imglist_back(img4, 1000)), img1);
}

TEST_F(ImageList, Distance)
{
    config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
    imglist_init(config);

    struct image* img1 = imglist_add("exec://1");
    struct image* img2 = imglist_add("exec://2");
    struct image* img3 = imglist_add("exec://3");
    struct image* img4 = imglist_add("exec://4");

    ASSERT_EQ(imglist_distance(img1, img1), static_cast<size_t>(0));
    ASSERT_EQ(imglist_distance(img1, img2), static_cast<size_t>(1));
    ASSERT_EQ(imglist_distance(img1, img3), static_cast<size_t>(2));
    ASSERT_EQ(imglist_distance(img1, img4), static_cast<size_t>(3));

    ASSERT_EQ(imglist_distance(img2, img1), static_cast<size_t>(1));
    ASSERT_EQ(imglist_distance(img3, img1), static_cast<size_t>(2));
    ASSERT_EQ(imglist_distance(img4, img1), static_cast<size_t>(3));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// TEST_F(ImageList, Skip)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_skip(2), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_skip(0), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_skip(1), static_cast<size_t>(IMGLIST_INVALID));
// }

// TEST_F(ImageList, NearestFwdNoLoop)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, true, false),
//               static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(0, true, false), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_nearest(1, true, false), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_nearest(2, true, false),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_nearest(42, true, false),
//               static_cast<size_t>(IMGLIST_INVALID));
//     image_list_skip(1);
//     ASSERT_EQ(image_list_nearest(0, true, false), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_nearest(1, true, false), static_cast<size_t>(2));
// }

// TEST_F(ImageList, NearestFwdLoop)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, true, true),
//               static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(0, true, true), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_nearest(1, true, true), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_nearest(2, true, true), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(42, true, true), static_cast<size_t>(0));
//     image_list_skip(0);
//     ASSERT_EQ(image_list_nearest(0, true, true), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_nearest(2, true, true), static_cast<size_t>(1));
// }

// TEST_F(ImageList, NearestBackNoLoop)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, false, false),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_nearest(0, false, false),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_nearest(1, false, false), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(2, false, false), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_nearest(42, false, false), static_cast<size_t>(2));
//     image_list_skip(1);
//     ASSERT_EQ(image_list_nearest(2, false, false), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(1, false, false), static_cast<size_t>(0));
// }

// TEST_F(ImageList, NearestBackLoop)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, false, true),
//               static_cast<size_t>(2));
//     ASSERT_EQ(image_list_nearest(0, false, true), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_nearest(1, false, true), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(2, false, true), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_nearest(42, false, true), static_cast<size_t>(2));
//     image_list_skip(1);
//     ASSERT_EQ(image_list_nearest(2, false, true), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_nearest(1, false, true), static_cast<size_t>(0));
// }

// TEST_F(ImageList, JumpFwd)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");

//     ASSERT_EQ(image_list_jump(IMGLIST_INVALID, 1, true),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_jump(42, 1, true),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_jump(0, 42, true), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_jump(0, 0, true), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_jump(0, 1, true), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_jump(0, 2, true), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_jump(0, 42, true), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_jump(2, 1, true), static_cast<size_t>(2));
//     image_list_skip(1);
//     ASSERT_EQ(image_list_jump(0, 1, true), static_cast<size_t>(2));
// }

// TEST_F(ImageList, JumpBack)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");

//     ASSERT_EQ(image_list_jump(IMGLIST_INVALID, 1, false),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_jump(42, 1, false),
//               static_cast<size_t>(IMGLIST_INVALID));
//     ASSERT_EQ(image_list_jump(2, 42, false), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_jump(2, 0, false), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_jump(2, 1, false), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_jump(2, 2, false), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_jump(2, 42, false), static_cast<size_t>(0));
//     ASSERT_EQ(image_list_jump(0, 1, false), static_cast<size_t>(0));
//     image_list_skip(1);
//     ASSERT_EQ(image_list_jump(2, 1, false), static_cast<size_t>(0));
// }

// TEST_F(ImageList, DistanceOld)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_distance(IMGLIST_INVALID, IMGLIST_INVALID),
//               static_cast<size_t>(2));
//     ASSERT_EQ(image_list_distance(0, IMGLIST_INVALID),
//     static_cast<size_t>(2)); ASSERT_EQ(image_list_distance(IMGLIST_INVALID,
//     2), static_cast<size_t>(2)); ASSERT_EQ(image_list_distance(0, 0),
//     static_cast<size_t>(0)); ASSERT_EQ(image_list_distance(0, 1),
//     static_cast<size_t>(1)); ASSERT_EQ(image_list_distance(0, 2),
//     static_cast<size_t>(2)); ASSERT_EQ(image_list_distance(2, 1),
//     static_cast<size_t>(1)); ASSERT_EQ(image_list_distance(2, 0),
//     static_cast<size_t>(2)); ASSERT_EQ(image_list_distance(2, 2),
//     static_cast<size_t>(0)); image_list_skip(1);
//     ASSERT_EQ(image_list_distance(0, 2), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_distance(2, 0), static_cast<size_t>(1));
// }

// TEST_F(ImageList, Get)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     const char* src = image_list_get(1);
//     ASSERT_STREQ(src, "exec://cmd2");
// }

// TEST_F(ImageList, GetFirst)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_first(), static_cast<size_t>(0));
// }

// TEST_F(ImageList, GetLast)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_last(), static_cast<size_t>(2));
// }

// TEST_F(ImageList, GetNextFile)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_next_file(0), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_next_file(1), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_next_file(2), static_cast<size_t>(0));
// }

// TEST_F(ImageList, GePrevFile)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1");
//     imglist_add("exec://cmd2");
//     imglist_add("exec://cmd3");
//     ASSERT_EQ(image_list_prev_file(0), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_prev_file(1), static_cast<size_t>(0));
// }

// TEST_F(ImageList, GetNextDir)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1/dir1/image1");
//     imglist_add("exec://cmd1/dir1/image2");
//     imglist_add("exec://cmd1/dir2/image3");
//     imglist_add("exec://cmd1/dir2/image4");
//     imglist_add("exec://cmd1/dir3/image5");
//     ASSERT_EQ(image_list_next_dir(0), static_cast<size_t>(2));
//     ASSERT_EQ(image_list_next_dir(2), static_cast<size_t>(4));
//     ASSERT_EQ(image_list_next_dir(4), static_cast<size_t>(0));
// }

// TEST_F(ImageList, GetPrevDir)
// {
//     imglist_init(config);
//     imglist_add("exec://cmd1/dir1/image1");
//     imglist_add("exec://cmd1/dir1/image2");
//     imglist_add("exec://cmd1/dir2/image3");
//     imglist_add("exec://cmd1/dir2/image4");
//     imglist_add("exec://cmd1/dir3/image5");
//     ASSERT_EQ(image_list_prev_dir(0), static_cast<size_t>(4));
//     ASSERT_EQ(image_list_prev_dir(2), static_cast<size_t>(1));
//     ASSERT_EQ(image_list_prev_dir(3), static_cast<size_t>(1));
// }

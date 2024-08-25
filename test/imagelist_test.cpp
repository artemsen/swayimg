// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "imagelist.h"
}

#include <gtest/gtest.h>

class ImageList : public ::testing::Test {
protected:
    void TearDown() override { image_list_destroy(); }
};

TEST_F(ImageList, Init)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    ASSERT_EQ(image_list_size(), static_cast<size_t>(0));
    ASSERT_EQ(image_list_init(nullptr, sources, 3), static_cast<size_t>(3));
    ASSERT_EQ(image_list_size(), static_cast<size_t>(3));
}

TEST_F(ImageList, Find)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    const size_t idx = image_list_find("exec://cmd2");
    ASSERT_EQ(idx, static_cast<size_t>(1));
}

TEST_F(ImageList, Skip)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_skip(2), static_cast<size_t>(1));
    ASSERT_EQ(image_list_skip(0), static_cast<size_t>(1));
    ASSERT_EQ(image_list_skip(1), static_cast<size_t>(IMGLIST_INVALID));
}

TEST_F(ImageList, NearestFwdNoLoop)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, true, false),
              static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(0, true, false), static_cast<size_t>(1));
    ASSERT_EQ(image_list_nearest(1, true, false), static_cast<size_t>(2));
    ASSERT_EQ(image_list_nearest(2, true, false),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_nearest(42, true, false),
              static_cast<size_t>(IMGLIST_INVALID));
    image_list_skip(1);
    ASSERT_EQ(image_list_nearest(0, true, false), static_cast<size_t>(2));
    ASSERT_EQ(image_list_nearest(1, true, false), static_cast<size_t>(2));
}

TEST_F(ImageList, NearestFwdLoop)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, true, true),
              static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(0, true, true), static_cast<size_t>(1));
    ASSERT_EQ(image_list_nearest(1, true, true), static_cast<size_t>(2));
    ASSERT_EQ(image_list_nearest(2, true, true), static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(42, true, true), static_cast<size_t>(0));
    image_list_skip(0);
    ASSERT_EQ(image_list_nearest(0, true, true), static_cast<size_t>(1));
    ASSERT_EQ(image_list_nearest(2, true, true), static_cast<size_t>(1));
}

TEST_F(ImageList, NearestBackNoLoop)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, false, false),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_nearest(0, false, false),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_nearest(1, false, false), static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(2, false, false), static_cast<size_t>(1));
    ASSERT_EQ(image_list_nearest(42, false, false), static_cast<size_t>(2));
    image_list_skip(1);
    ASSERT_EQ(image_list_nearest(2, false, false), static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(1, false, false), static_cast<size_t>(0));
}

TEST_F(ImageList, NearestBackLoop)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_nearest(IMGLIST_INVALID, false, true),
              static_cast<size_t>(2));
    ASSERT_EQ(image_list_nearest(0, false, true), static_cast<size_t>(2));
    ASSERT_EQ(image_list_nearest(1, false, true), static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(2, false, true), static_cast<size_t>(1));
    ASSERT_EQ(image_list_nearest(42, false, true), static_cast<size_t>(2));
    image_list_skip(1);
    ASSERT_EQ(image_list_nearest(2, false, true), static_cast<size_t>(0));
    ASSERT_EQ(image_list_nearest(1, false, true), static_cast<size_t>(0));
}

TEST_F(ImageList, JumpFwd)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);

    ASSERT_EQ(image_list_jump(IMGLIST_INVALID, 1, true),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_jump(42, 1, true),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_jump(0, 42, true), static_cast<size_t>(2));
    ASSERT_EQ(image_list_jump(0, 0, true), static_cast<size_t>(0));
    ASSERT_EQ(image_list_jump(0, 1, true), static_cast<size_t>(1));
    ASSERT_EQ(image_list_jump(0, 2, true), static_cast<size_t>(2));
    ASSERT_EQ(image_list_jump(0, 42, true), static_cast<size_t>(2));
    ASSERT_EQ(image_list_jump(2, 1, true), static_cast<size_t>(2));
    image_list_skip(1);
    ASSERT_EQ(image_list_jump(0, 1, true), static_cast<size_t>(2));
}

TEST_F(ImageList, JumpBack)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);

    ASSERT_EQ(image_list_jump(IMGLIST_INVALID, 1, false),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_jump(42, 1, false),
              static_cast<size_t>(IMGLIST_INVALID));
    ASSERT_EQ(image_list_jump(2, 42, false), static_cast<size_t>(0));
    ASSERT_EQ(image_list_jump(2, 0, false), static_cast<size_t>(2));
    ASSERT_EQ(image_list_jump(2, 1, false), static_cast<size_t>(1));
    ASSERT_EQ(image_list_jump(2, 2, false), static_cast<size_t>(0));
    ASSERT_EQ(image_list_jump(2, 42, false), static_cast<size_t>(0));
    ASSERT_EQ(image_list_jump(0, 1, false), static_cast<size_t>(0));
    image_list_skip(1);
    ASSERT_EQ(image_list_jump(2, 1, false), static_cast<size_t>(0));
}

TEST_F(ImageList, Distance)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_distance(IMGLIST_INVALID, IMGLIST_INVALID),
              static_cast<size_t>(2));
    ASSERT_EQ(image_list_distance(0, IMGLIST_INVALID), static_cast<size_t>(2));
    ASSERT_EQ(image_list_distance(IMGLIST_INVALID, 2), static_cast<size_t>(2));
    ASSERT_EQ(image_list_distance(0, 0), static_cast<size_t>(0));
    ASSERT_EQ(image_list_distance(0, 1), static_cast<size_t>(1));
    ASSERT_EQ(image_list_distance(0, 2), static_cast<size_t>(2));
    ASSERT_EQ(image_list_distance(2, 1), static_cast<size_t>(1));
    ASSERT_EQ(image_list_distance(2, 0), static_cast<size_t>(2));
    ASSERT_EQ(image_list_distance(2, 2), static_cast<size_t>(0));
    image_list_skip(1);
    ASSERT_EQ(image_list_distance(0, 2), static_cast<size_t>(1));
    ASSERT_EQ(image_list_distance(2, 0), static_cast<size_t>(1));
}

TEST_F(ImageList, Get)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    const char* src = image_list_get(1);
    ASSERT_STREQ(src, "exec://cmd2");
}

TEST_F(ImageList, GetFirst)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_first(), static_cast<size_t>(0));
}

TEST_F(ImageList, GetLast)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_last(), static_cast<size_t>(2));
}

TEST_F(ImageList, GetNextFile)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_next_file(0), static_cast<size_t>(1));
    ASSERT_EQ(image_list_next_file(1), static_cast<size_t>(2));
    ASSERT_EQ(image_list_next_file(2), static_cast<size_t>(0));
}

TEST_F(ImageList, GePrevFile)
{
    const char* sources[] = { "exec://cmd1", "exec://cmd2", "exec://cmd3" };
    image_list_init(nullptr, sources, 3);
    ASSERT_EQ(image_list_prev_file(0), static_cast<size_t>(2));
    ASSERT_EQ(image_list_prev_file(1), static_cast<size_t>(0));
}

TEST_F(ImageList, GetNextDir)
{
    const char* sources[] = {
        "exec://cmd1/dir1/image1", "exec://cmd1/dir1/image2",
        "exec://cmd1/dir2/image3", "exec://cmd1/dir2/image4",
        "exec://cmd1/dir3/image5",
    };
    image_list_init(nullptr, sources, 5);
    ASSERT_EQ(image_list_next_dir(0), static_cast<size_t>(2));
    ASSERT_EQ(image_list_next_dir(2), static_cast<size_t>(4));
    ASSERT_EQ(image_list_next_dir(4), static_cast<size_t>(0));
}

TEST_F(ImageList, GetPrevDir)
{
    const char* sources[] = {
        "exec://cmd1/dir1/image1", "exec://cmd1/dir1/image2",
        "exec://cmd1/dir2/image3", "exec://cmd1/dir2/image4",
        "exec://cmd1/dir3/image5",
    };
    image_list_init(nullptr, sources, 5);
    ASSERT_EQ(image_list_prev_dir(0), static_cast<size_t>(4));
    ASSERT_EQ(image_list_prev_dir(2), static_cast<size_t>(1));
    ASSERT_EQ(image_list_prev_dir(3), static_cast<size_t>(1));
}

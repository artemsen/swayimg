// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "image.h"
}

#include <gtest/gtest.h>

class Image : public ::testing::Test {
protected:
    void SetUp() override
    {
        image = image_alloc();
        ASSERT_TRUE(image);
    }
    void TearDown() override { image_free(image); }
    struct image* image;
};

TEST_F(Image, SetSourcePathLong)
{
    image_set_source(image, "/home/user/image.jpg");
    EXPECT_STREQ(image->source, "/home/user/image.jpg");
    EXPECT_STREQ(image->name, "image.jpg");
    EXPECT_STREQ(image->parent_dir, "user");
}

TEST_F(Image, SetSourcePathShort)
{
    image_set_source(image, "/image.jpg");
    EXPECT_STREQ(image->source, "/image.jpg");
    EXPECT_STREQ(image->name, "image.jpg");
    EXPECT_STREQ(image->parent_dir, "");
}

TEST_F(Image, SetSourcePathNameOnly)
{
    image_set_source(image, "image.jpg");
    EXPECT_STREQ(image->source, "image.jpg");
    EXPECT_STREQ(image->name, "image.jpg");
    EXPECT_STREQ(image->parent_dir, "");
}

TEST_F(Image, SetSourcePathRelative)
{
    image_set_source(image, "user/image.jpg");
    EXPECT_STREQ(image->source, "user/image.jpg");
    EXPECT_STREQ(image->name, "image.jpg");
    EXPECT_STREQ(image->parent_dir, "user");
}

TEST_F(Image, SetSourceStdin)
{
    image_set_source(image, LDRSRC_STDIN);
    EXPECT_STREQ(image->source, LDRSRC_STDIN);
    EXPECT_STREQ(image->name, LDRSRC_STDIN);
    EXPECT_STREQ(image->parent_dir, "");
}

TEST_F(Image, SetSourceExec)
{
    const char* src = LDRSRC_EXEC "cat image.txt";
    image_set_source(image, src);
    EXPECT_STREQ(image->source, src);
    EXPECT_STREQ(image->name, src);
    EXPECT_STREQ(image->parent_dir, "");
}

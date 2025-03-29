// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "image.h"
}

#include <gtest/gtest.h>
#include <unistd.h>

class Image : public ::testing::Test {
protected:
    void TearDown() override
    {
        if (image) {
            image_free(image);
        }
    }

    void Load(const char* file)
    {
        image = image_create(file);
        ASSERT_TRUE(image);
        ASSERT_EQ(image_load(image), ldr_success);
        EXPECT_NE(image->frames[0].pm.width, static_cast<size_t>(0));
        EXPECT_NE(image->frames[0].pm.height, static_cast<size_t>(0));
        EXPECT_NE(image->frames[0].pm.data[0], static_cast<argb_t>(0));
    }

    struct image* image = nullptr;
};

TEST_F(Image, Create)
{
    image = image_create("file123");
    ASSERT_TRUE(image);
    ASSERT_STREQ(image->source, "file123");
}

TEST_F(Image, Copy)
{
    image = image_create("file123");
    struct image* copy = image_copy(image);
    ASSERT_TRUE(copy);
    EXPECT_STREQ(image->source, "file123");
    image_free(copy);
}

TEST_F(Image, Unload)
{
    Load(TEST_DATA_DIR "/image.bmp");
    image_unload(image);
    EXPECT_EQ(image->num_frames, static_cast<size_t>(0));
}

TEST_F(Image, Transform)
{
    Load(TEST_DATA_DIR "/image.bmp");
    image_flip_vertical(image);
    image_flip_horizontal(image);
    image_rotate(image, 90);
}

TEST_F(Image, Thumbnail)
{
    Load(TEST_DATA_DIR "/image.bmp");

    image_thumb_create(image, 10, true, aa_nearest);
    EXPECT_TRUE(image->thumbnail.data);
    EXPECT_EQ(image->thumbnail.width, static_cast<size_t>(10));
    EXPECT_EQ(image->thumbnail.height, static_cast<size_t>(10));

    image_thumb_free(image);
    EXPECT_FALSE(image->thumbnail.data);
}

TEST_F(Image, LoadFromExec)
{
    image = image_create(LDRSRC_EXEC "cat " TEST_DATA_DIR "/image.bmp");
    ASSERT_TRUE(image);
    ASSERT_EQ(image_load(image), ldr_success);
}

#define TEST_LOADER(n)                    \
    TEST_F(Image, Load_##n)               \
    {                                     \
        Load(TEST_DATA_DIR "/image." #n); \
    }

TEST_LOADER(bmp);
TEST_LOADER(dcm);
TEST_LOADER(ff);
TEST_LOADER(pnm);
TEST_LOADER(qoi);
TEST_LOADER(tga);
#ifdef HAVE_LIBEXR
// TEST_LOADER(exr);
#endif
#ifdef HAVE_LIBGIF
TEST_LOADER(gif);
#endif
#ifdef HAVE_LIBHEIF
TEST_LOADER(heif);
#endif
#ifdef HAVE_LIBAVIF
TEST_LOADER(avif);
#endif
#ifdef HAVE_LIBJPEG
TEST_LOADER(jpg);
#endif
#ifdef HAVE_LIBJXL
TEST_LOADER(jxl);
#endif
#ifdef HAVE_LIBPNG
TEST_LOADER(png);
#endif
#ifdef HAVE_LIBRSVG
TEST_LOADER(svg);
#endif
#ifdef HAVE_LIBTIFF
TEST_LOADER(tiff);
#endif
#ifdef HAVE_LIBSIXEL
TEST_LOADER(six);
#endif
#ifdef HAVE_LIBWEBP
TEST_LOADER(webp);
#endif

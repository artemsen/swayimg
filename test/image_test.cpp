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
            image_deref(image);
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
    image = image_create("file");
    ASSERT_TRUE(image);
    ASSERT_EQ(image->ref_count, static_cast<size_t>(1));

    image_addref(image);
    ASSERT_EQ(image->ref_count, static_cast<size_t>(2));

    image_deref(image);
    ASSERT_EQ(image->ref_count, static_cast<size_t>(1));
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

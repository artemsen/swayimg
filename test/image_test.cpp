// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "buildcfg.h"
#include "image.h"

#ifdef HAVE_LIBRSVG
#include "formats/svg.h"
#endif
}

#include <gtest/gtest.h>
#include <unistd.h>

class Image : public ::testing::Test {
protected:
    void TearDown() override
    {
        if (image) {
            image_free(image, IMGFREE_ALL);
        }
    }

    void Load(const char* file)
    {
        image = image_create(file);
        ASSERT_TRUE(image);
        ASSERT_EQ(image_load(image), imgload_success);
        ASSERT_TRUE(image->name);
        ASSERT_TRUE(image->parent_dir);
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

TEST_F(Image, Update)
{
    Load(TEST_DATA_DIR "/image.bmp");
    image->index = 123;

    struct image* new_img = image_create(TEST_DATA_DIR "/image.bmp");
    ASSERT_TRUE(new_img);
    new_img->index = 321;

    image_update(new_img, image);

    EXPECT_FALSE(image->frames);
    EXPECT_TRUE(new_img->frames);
    EXPECT_EQ(new_img->index, static_cast<size_t>(321));

    image_free(new_img, IMGFREE_ALL);
}

TEST_F(Image, Free)
{
    Load(TEST_DATA_DIR "/image.bmp");

    EXPECT_FALSE(image_has_thumb(image));
    image_thumb_create(image, 1, true, aa_nearest);
    EXPECT_TRUE(image_has_thumb(image));
    image_free(image, IMGFREE_THUMB);
    EXPECT_FALSE(image_has_thumb(image));

    EXPECT_TRUE(image_has_frames(image));
    image_free(image, IMGFREE_FRAMES);
    EXPECT_FALSE(image_has_frames(image));

    EXPECT_FALSE(image->format);
}

TEST_F(Image, Transform)
{
    Load(TEST_DATA_DIR "/image.bmp");
    image_flip_vertical(image);
    image_flip_horizontal(image);
    image_rotate(image, 90);
    image_rotate(image, 180);
    image_rotate(image, 270);
}

TEST_F(Image, Thumbnail)
{
    image = image_create("file");
    ASSERT_FALSE(image_thumb_create(image, 10, true, aa_nearest));
    image_free(image, IMGFREE_ALL);

    Load(TEST_DATA_DIR "/image.bmp");

    ASSERT_TRUE(image_thumb_create(image, 10, true, aa_nearest));
    EXPECT_TRUE(image->thumbnail.data);
    EXPECT_EQ(image->thumbnail.width, static_cast<size_t>(10));
    EXPECT_EQ(image->thumbnail.height, static_cast<size_t>(10));
}

TEST_F(Image, LoadFromExec)
{
    image = image_create(LDRSRC_EXEC "cat " TEST_DATA_DIR "/image.bmp");
    ASSERT_TRUE(image);
    ASSERT_EQ(image_load(image), imgload_success);
}

#ifdef HAVE_LIBRSVG
TEST_F(Image, RescaleSVG)
{
    Load(TEST_DATA_DIR "/image.svg");
    ASSERT_TRUE(image->frames[0].pm.height == 1024);
    ASSERT_TRUE(image->frames[0].pm.width == 1024);

    adjust_svg_render_size(1.5);

    image_free(image, IMGFREE_FRAMES);
    Load(TEST_DATA_DIR "/image.svg");
    ASSERT_TRUE(image->frames[0].pm.height == 1536);
    ASSERT_TRUE(image->frames[0].pm.width == 1536);

    reset_svg_render_size();

    image_free(image, IMGFREE_FRAMES);
    Load(TEST_DATA_DIR "/image.svg");
    ASSERT_TRUE(image->frames[0].pm.height == 1024);
    ASSERT_TRUE(image->frames[0].pm.width == 1024);
}
#endif // HAVE_LIBRSVG

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

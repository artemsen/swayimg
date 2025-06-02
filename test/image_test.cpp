// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "buildcfg.h"
#include "image.h"
}

#include <gtest/gtest.h>
#include <unistd.h>

class Image : public ::testing::Test {
protected:
    void TearDown() override
    {
        if (image) {
            image_free(image, IMGDATA_SELF);
        }
    }

    void Load(const char* file)
    {
        image = image_create(file);
        ASSERT_TRUE(image);
        ASSERT_TRUE(image->name);
        ASSERT_TRUE(image->source);
        ASSERT_EQ(image_load(image), imgload_success);

        ASSERT_TRUE(image->data);
        ASSERT_TRUE(image->data->parent);
        ASSERT_TRUE(image->data->format);
        ASSERT_TRUE(image->data->frames);

        const struct imgframe* frame = static_cast<const struct imgframe*>(
            arr_nth(image->data->frames, 0));
        EXPECT_NE(frame->pm.width, static_cast<size_t>(0));
        EXPECT_NE(frame->pm.height, static_cast<size_t>(0));
        EXPECT_TRUE(frame->pm.data);
    }

    struct image* image = nullptr;
};

TEST_F(Image, Create)
{
    image = image_create("file123");
    ASSERT_TRUE(image);
    ASSERT_STREQ(image->source, "file123");
}

TEST_F(Image, Attach)
{
    Load(TEST_DATA_DIR "/image.bmp");
    image->index = 123;

    struct image* new_img = image_create(TEST_DATA_DIR "/image.bmp");
    ASSERT_TRUE(new_img);
    new_img->index = 321;

    image_attach(new_img, image);

    ASSERT_TRUE(new_img->data);
    EXPECT_TRUE(new_img->data->frames);
    EXPECT_EQ(new_img->index, static_cast<size_t>(321));
    EXPECT_FALSE(image->data->frames);

    image_free(new_img, IMGDATA_SELF);
}

TEST_F(Image, Free)
{
    Load(TEST_DATA_DIR "/image.bmp");

    EXPECT_FALSE(image_thumb_get(image));
    image_thumb_create(image, 1, true, aa_nearest);
    EXPECT_TRUE(image_thumb_get(image));
    image_free(image, IMGDATA_THUMB);
    EXPECT_FALSE(image_thumb_get(image));

    EXPECT_TRUE(image_has_frames(image));
    image_free(image, IMGDATA_FRAMES);
    EXPECT_FALSE(image_has_frames(image));

    EXPECT_FALSE(image->data);
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
    image_free(image, IMGDATA_SELF);

    Load(TEST_DATA_DIR "/image.bmp");

    ASSERT_FALSE(image_thumb_get(image));
    ASSERT_TRUE(image_thumb_create(image, 10, true, aa_nearest));
    ASSERT_TRUE(image_thumb_get(image));
    ASSERT_TRUE(image->data);
    EXPECT_TRUE(image->data->thumbnail.data);
    EXPECT_EQ(image->data->thumbnail.width, static_cast<size_t>(10));
    EXPECT_EQ(image->data->thumbnail.height, static_cast<size_t>(10));
}

TEST_F(Image, SetFormat)
{
    image = image_create("file");
    image->data = static_cast<struct imgdata*>(calloc(1, sizeof(*image->data)));

    image_set_format(image->data, "Test%d", 123);
    ASSERT_STREQ(image->data->format, "Test123");
}

TEST_F(Image, AddMetaInfo)
{
    const struct imginfo* inf;

    image = image_create("file");
    image->data = static_cast<struct imgdata*>(calloc(1, sizeof(*image->data)));

    ASSERT_FALSE(image->data->info);
    image_add_info(image->data, "Key1", "InfoLine%d", 1);
    ASSERT_TRUE(image->data->info);
    ASSERT_EQ(image->data->info->size, static_cast<size_t>(1));

    inf = static_cast<struct imginfo*>(arr_nth(image->data->info, 0));
    EXPECT_STREQ(inf->key, "Key1");
    EXPECT_STREQ(inf->value, "InfoLine1");

    image_add_info(image->data, "Key2", "InfoLine%d", 2);
    ASSERT_EQ(image->data->info->size, static_cast<size_t>(2));
    inf = static_cast<struct imginfo*>(arr_nth(image->data->info, 0));
    EXPECT_STREQ(inf->key, "Key1");
    EXPECT_STREQ(inf->value, "InfoLine1");
    inf = static_cast<struct imginfo*>(arr_nth(image->data->info, 1));
    EXPECT_STREQ(inf->key, "Key2");
    EXPECT_STREQ(inf->value, "InfoLine2");
}

TEST_F(Image, AllocFrame)
{
    image = image_create("file");
    image->data = static_cast<struct imgdata*>(calloc(1, sizeof(*image->data)));

    const struct pixmap* pm = image_alloc_frame(image->data, 123, 456);
    ASSERT_TRUE(image->data->frames);
    ASSERT_EQ(image->data->frames->size, static_cast<size_t>(1));
    ASSERT_EQ(pm, arr_nth(image->data->frames, 0));

    ASSERT_TRUE(pm);
    ASSERT_EQ(pm->width, static_cast<size_t>(123));
    ASSERT_EQ(pm->height, static_cast<size_t>(456));
    ASSERT_TRUE(pm->data);
}

TEST_F(Image, AllocFrames)
{
    image = image_create("file");
    image->data = static_cast<struct imgdata*>(calloc(1, sizeof(*image->data)));

    ASSERT_TRUE(image_alloc_frames(image->data, 5));
    ASSERT_EQ(image->data->frames->size, static_cast<size_t>(5));
}

TEST_F(Image, LoadFromExec)
{
    image = image_create(LDRSRC_EXEC "cat " TEST_DATA_DIR "/image.bmp");
    ASSERT_TRUE(image);
    ASSERT_EQ(image_load(image), imgload_success);
}

#ifdef HAVE_LIBRSVG
TEST_F(Image, Load_svg)
{
    Load(TEST_DATA_DIR "/image.svg");
    struct pixmap* pm =
        &static_cast<struct imgframe*>(arr_nth(image->data->frames, 0))->pm;
    image_render(image, 0, aa_nearest, 1.0, false, 0, 0, pm);
}
#endif

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
#ifdef HAVE_LIBTIFF
TEST_LOADER(tiff);
#endif
#ifdef HAVE_LIBSIXEL
TEST_LOADER(six);
#endif
#ifdef HAVE_LIBWEBP
TEST_LOADER(webp);
#endif

// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "buildconf.hpp"
#include "imageloader.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

static ImagePtr load_image(const char* path)
{
    ImageList::EntryPtr entry = std::make_shared<ImageList::Entry>();
    entry->path = path;
    return ImageLoader::load(entry);
}

#define TEST_IMAGE_LOAD(fmt)                                       \
    TEST(ImageLoadTest, fmt)                                       \
    {                                                              \
        ImagePtr image = load_image(TEST_DATA_DIR "/image." #fmt); \
        ASSERT_TRUE(image);                                        \
        EXPECT_NE(image->frames.size(), 0UL);                      \
    }

TEST_IMAGE_LOAD(bmp);
TEST_IMAGE_LOAD(dcm);
TEST_IMAGE_LOAD(ff);
TEST_IMAGE_LOAD(pnm);
TEST_IMAGE_LOAD(qoi);
TEST_IMAGE_LOAD(tga);
#ifdef HAVE_LIBEXR
TEST_IMAGE_LOAD(exr);
#endif
#ifdef HAVE_LIBGIF
TEST_IMAGE_LOAD(gif);
#endif
#ifdef HAVE_LIBHEIF
TEST_IMAGE_LOAD(heif);
#endif
#ifdef HAVE_LIBAVIF
TEST_IMAGE_LOAD(avif);
#endif
#ifdef HAVE_LIBJPEG
TEST_IMAGE_LOAD(jpg);
#endif
#ifdef HAVE_LIBJXL
TEST_IMAGE_LOAD(jxl);
#endif
#ifdef HAVE_LIBPNG
TEST_IMAGE_LOAD(png);
#endif
#ifdef HAVE_LIBTIFF
TEST_IMAGE_LOAD(tiff);
#endif
#ifdef HAVE_LIBSIXEL
TEST_IMAGE_LOAD(six);
#endif
#ifdef HAVE_LIBWEBP
TEST_IMAGE_LOAD(webp);
#endif
#ifdef HAVE_LIBRSVG
TEST_IMAGE_LOAD(svg);
#endif

#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBEXIV2)
TEST(ImageTest, Exif)
{
    ImagePtr image = load_image(TEST_DATA_DIR "/exif.jpg");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->meta["Exif.Image.Make"], "Google");
    EXPECT_EQ(image->meta["Exif.Image.Model"], "Pixel 7");
}
#endif

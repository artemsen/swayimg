// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "application.h"
#include "buildcfg.h"
#include "loader.h"
#include "ui.h"
#include "viewer.h"
}

#include <gtest/gtest.h>

// stubs for linker (application and ui are not included to tests)
extern "C" {
void app_watch(int, fd_callback, void*) { }
void app_reload() { }
void app_redraw() { }
void app_on_resize() { }
void app_on_keyboard(xkb_keysym_t, uint8_t) { }
void app_on_drag(int, int) { }
void app_exit(int) { }
void app_on_load(struct image*, size_t) { }
bool app_is_viewer()
{
    return true;
}
}

class Loader : public ::testing::Test {
protected:
    void TearDown() override { image_free(image); }

    void Load(const char* file)
    {
        EXPECT_EQ(loader_from_source(file, &image), ldr_success);
        ASSERT_NE(image, nullptr);
        EXPECT_NE(image->frames[0].pm.width, static_cast<size_t>(0));
        EXPECT_NE(image->frames[0].pm.height, static_cast<size_t>(0));
        EXPECT_NE(image->frames[0].pm.data[0], static_cast<argb_t>(0));
    }
    struct image* image = nullptr;
};

TEST_F(Loader, External)
{
    const enum loader_status status = loader_from_source(
        LDRSRC_EXEC "cat " TEST_DATA_DIR "/image.bmp", &image);
    EXPECT_EQ(status, ldr_success);
    ASSERT_NE(image, nullptr);
}

#define TEST_LOADER(n)                    \
    TEST_F(Loader, n)                     \
    {                                     \
        Load(TEST_DATA_DIR "/image." #n); \
    }

TEST_LOADER(bmp);
TEST_LOADER(pnm);
TEST_LOADER(qoi);
TEST_LOADER(tga);
TEST_LOADER(ff);
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
#ifdef HAVE_LIBWEBP
TEST_LOADER(webp);
#endif

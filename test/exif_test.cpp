// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "exif.h"
}

#include <gtest/gtest.h>

#include <fstream>

TEST(Exif, Read)
{
    std::ifstream file(TEST_DATA_DIR "/exif.jpg", std::ios::binary);
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                    (std::istreambuf_iterator<char>()));

    struct image* image = image_create();
    process_exif(image, data.data(), data.size());

    EXPECT_EQ(image->num_info, 7);
    EXPECT_STREQ(image->info[0].value, "2024:07:06 12:31:44");
    EXPECT_STREQ(image->info[1].value, "Google");
    EXPECT_STREQ(image->info[2].value, "Pixel 7");
    EXPECT_STREQ(image->info[3].value, "GIMP 2.99.16");
    EXPECT_STREQ(image->info[4].value, "1/50 sec.");
    EXPECT_STREQ(image->info[5].value, "f/1.9");
    EXPECT_STREQ(image->info[6].value, "55°44'28.41\"N, 37°37'25.46\"E");

    image_free(image);
}

TEST(Exif, Fail)
{
    struct image* image = image_create();

    process_exif(image, nullptr, 0);
    EXPECT_EQ(image->num_info, 0);

    process_exif(image, reinterpret_cast<const uint8_t*>("abcd"), 4);
    EXPECT_EQ(image->num_info, 0);

    image_free(image);
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "exif.h"
}

#include <gtest/gtest.h>

#include <fstream>

class Exif : public ::testing::Test {
protected:
    void SetUp() override { image = image_create("no_matter"); }
    void TearDown() override { image_free(image, IMGFREE_ALL); }
    struct image* image;
};

TEST_F(Exif, Read)
{
    std::ifstream file(TEST_DATA_DIR "/exif.jpg", std::ios::binary);
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                    (std::istreambuf_iterator<char>()));

    process_exif(image, data.data(), data.size());

    ASSERT_EQ(list_size(&image->info->list), static_cast<size_t>(7));

    size_t i = 0;
    list_for_each(image->info, const struct image_info, it) {
        const char* expect_key;
        const char* expect_val;
        switch (i) {
            case 0:
                expect_key = "DateTime";
                expect_val = "2024:05:30 21:18:48";
                break;
            case 1:
                expect_key = "Camera";
                expect_val = "Google";
                break;
            case 2:
                expect_key = "Model";
                expect_val = "Pixel 7";
                break;
            case 3:
                expect_key = "Software";
                expect_val = "GIMP 2.99.16";
                break;
            case 4:
                expect_key = "Exposure";
                expect_val = "1/50 sec.";
                break;
            case 5:
                expect_key = "F Number";
                expect_val = "f/1.9";
                break;
            case 6:
                expect_key = "Location";
                expect_val = "55°44'28.41\"N, 37°37'25.46\"E";
                break;
            default:
                GTEST_FAIL();
                break;
        }
        EXPECT_STREQ(it->key, expect_key);
        EXPECT_STREQ(it->value, expect_val);
        ++i;
    }
}

TEST_F(Exif, Fail)
{
    process_exif(image, nullptr, 0);
    EXPECT_EQ(list_size(&image->info->list), static_cast<size_t>(0));

    process_exif(image, reinterpret_cast<const uint8_t*>("abcd"), 4);
    EXPECT_EQ(list_size(&image->info->list), static_cast<size_t>(0));
}

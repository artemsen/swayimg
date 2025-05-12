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
    void TearDown() override { image_free(image, IMGDATA_SELF); }
    struct image* image;
};

TEST_F(Exif, Read)
{
    std::ifstream file(TEST_DATA_DIR "/exif.jpg", std::ios::binary);
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                    (std::istreambuf_iterator<char>()));

    image->data = static_cast<struct imgdata*>(calloc(1, sizeof(*image->data)));

    process_exif(image->data, data.data(), data.size());

    ASSERT_EQ(image->data->info->size, static_cast<size_t>(7));

    const std::pair<const char*, const char*> etalon[] = {
        { "DateTime", "2024:05:30 21:18:48"            },
        { "Camera",   "Google"                         },
        { "Model",    "Pixel 7"                        },
        { "Software", "GIMP 2.99.16"                   },
        { "Exposure", "1/50 sec."                      },
        { "F Number", "f/1.9"                          },
        { "Location", "55°44'28.41\"N, 37°37'25.46\"E" },
    };

    for (size_t i = 0; i < sizeof(etalon) / sizeof(etalon[0]); ++i) {
        const struct imginfo* inf =
            static_cast<struct imginfo*>(arr_nth(image->data->info, i));
        EXPECT_STREQ(inf->key, etalon[i].first);
        EXPECT_STREQ(inf->value, etalon[i].second);
    }
}

TEST_F(Exif, Fail)
{
    image->data = static_cast<struct imgdata*>(calloc(1, sizeof(*image->data)));

    process_exif(image->data, nullptr, 0);
    EXPECT_FALSE(image->data->info);

    process_exif(image->data, reinterpret_cast<const uint8_t*>("abcd"), 4);
    EXPECT_FALSE(image->data->info);
}

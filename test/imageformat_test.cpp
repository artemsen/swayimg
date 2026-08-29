// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "formatfactory.hpp"

#include <gtest/gtest.h>

#include <memory>

TEST(ImageFormatTest, FormatFactoryInstance)
{
    const FormatFactory& factory = FormatFactory::self();
    EXPECT_EQ(&factory, &FormatFactory::self());
}

TEST(ImageFormatTest, FormatList)
{
    const FormatFactory& factory = FormatFactory::self();
    const std::string list = factory.list();
    EXPECT_FALSE(list.empty());
    EXPECT_NE(list.find("tga"), std::string::npos);
    EXPECT_NE(list.find("bmp"), std::string::npos);
}

TEST(ImageFormatTest, GetFormatByName)
{
    FormatFactory& factory = FormatFactory::self();

    const ImageFormat* fmt = factory.get("bmp");
    ASSERT_TRUE(fmt);
    EXPECT_STREQ(fmt->name, "bmp");

    fmt = factory.get("invalid_format");
    EXPECT_FALSE(fmt);
}

TEST(ImageFormatTest, DecodeCorruptedData)
{
    const FormatFactory& factory = FormatFactory::self();

    const uint8_t corrupted_data[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    ImageFormat::Data data;
    data.data = const_cast<uint8_t*>(corrupted_data);
    data.size = sizeof(corrupted_data);

    const ImagePtr image = factory.decode(data);
    EXPECT_EQ(image, nullptr);
}

TEST(ImageFormatTest, PreviewInvalidData)
{
    const FormatFactory& factory = FormatFactory::self();

    const ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = "/nonexistent/image.jpg";

    const Pixmap preview = factory.preview(entry, 100, false);
    EXPECT_FALSE(preview);
}

TEST(ImageFormatTest, PreviewFromValidImageEntry)
{
    const FormatFactory& factory = FormatFactory::self();

    const ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = TEST_DATA_DIR "/image.bmp";

    const Pixmap preview = factory.preview(entry, 64, false);
    ASSERT_TRUE(preview);
    EXPECT_NE(preview.width(), 0UL);
}

TEST(ImageFormatTest, LoadNonExistentFile)
{
    const FormatFactory& factory = FormatFactory::self();

    const ImageEntryPtr entry = std::make_shared<ImageEntry>();
    entry->path = "/tmp/nonexistent_test_image.xyz";

    const ImagePtr image = factory.load(entry);
    EXPECT_EQ(image, nullptr);
}

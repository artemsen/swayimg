// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "ui.hpp"

#include <gtest/gtest.h>

namespace {

// Compose a string from raw bytes (no literals with bidirectional characters)
std::string bytes(const std::initializer_list<uint8_t> data)
{
    return { data.begin(), data.end() };
}

} // anonymous namespace

TEST(UiTest, SanitizeTitleKeepsText)
{
    EXPECT_EQ(Ui::sanitize_title(""), "");
    EXPECT_EQ(Ui::sanitize_title("Swayimg: image.png"), "Swayimg: image.png");
    // other UTF-8 text stays as is
    const std::string utf8 = "Swayimg: \xd1\x84\xd0\xbe\xd1\x82\xd0\xbe "
                             "\xe2\x82\xac \xf0\x9f\x98\x80";
    EXPECT_EQ(Ui::sanitize_title(utf8), utf8);
}

TEST(UiTest, SanitizeTitleControlChars)
{
    // C0, DEL and C1 (U+0085)
    EXPECT_EQ(Ui::sanitize_title("a\x1b]0;x\x07\n\x7f\xc2\x85z"),
              "a?]0;x????z");
}

TEST(UiTest, SanitizeTitleBidi)
{
    // U+202E (right-to-left override), U+2066, U+200F, U+061C
    // the bidi controls in the input are the point of the test
    // NOLINTBEGIN(misc-misleading-bidirectional)
    EXPECT_EQ(Ui::sanitize_title("a\xe2\x80\xae"
                                 "b\xe2\x81\xa6\xe2\x80\x8f\xd8\x9c"),
              "a?b???");
    // NOLINTEND(misc-misleading-bidirectional)
}

TEST(UiTest, SanitizeTitleTruncated)
{
    // incomplete sequences at the end are not unsafe, just text
    EXPECT_EQ(Ui::sanitize_title(bytes({ 'a', 0xe2, 0x80 })),
              bytes({ 'a', 0xe2, 0x80 }));
    EXPECT_EQ(Ui::sanitize_title(bytes({ 'a', 0xc2 })), bytes({ 'a', 0xc2 }));
}

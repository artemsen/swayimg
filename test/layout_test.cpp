// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "imagelist.hpp"
#include "layout.hpp"

#include <gtest/gtest.h>

class LayoutTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ImageList& il = ImageList::self();
        ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
        while (entry) {
            entry = il.remove(entry);
        }
    }

    void TearDown() override
    {
        ImageList& il = ImageList::self();
        ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
        while (entry) {
            entry = il.remove(entry);
        }
        if (HasFailure()) {
            puts("Test failed. Layout scheme:");
            PrintLayout();
        }
    }

    void InitLayout(size_t total)
    {
        ImageList& il = ImageList::self();
        for (size_t i = 0; i < total; ++i) {
            il.add(std::string(ImageEntry::SRC_EXEC) + std::to_string(i));
        }
        layout.set_window_size({ 80, 60 });
        layout.set_thumb_size(10);
        layout.set_padding(5);
    }

    const Layout::Thumbnail* Select(const size_t num)
    {
        ImageList& il = ImageList::self();
        ImageEntryPtr entry = il.get(nullptr, ImageList::Dir::First);
        entry = il.get(entry, num);
        layout.select(entry);
        for (const auto& it : layout.get_scheme()) {
            if (it.img == entry) {
                return &it;
            }
        }
        return nullptr;
    }

    std::tuple<size_t, size_t> GetSelection() const
    {
        for (auto& it : layout.get_scheme()) {
            if (it.img == layout.get_selected()) {
                return std::make_tuple(it.col, it.row);
            }
        }
        return std::make_tuple(std::numeric_limits<size_t>::max(),
                               std::numeric_limits<size_t>::max());
    }

    void PrintLayout() const
    {
        printf("  | ");
        for (size_t col = 0; col < layout.get_columns(); ++col) {
            printf("%2ld ", col);
        }
        printf("\n--+");
        for (size_t col = 0; col < layout.get_columns(); ++col) {
            printf("---");
        }
        printf("\n");

        size_t index = 0;
        for (auto& it : layout.get_scheme()) {
            if ((index % layout.get_columns()) == 0) {
                printf("%ld | ", index / layout.get_columns());
            }
            const std::string path =
                it.img->path.string().substr(strlen(ImageEntry::SRC_EXEC));
            if (it.img == layout.get_selected()) {
                printf("\x1b[1;97m");
            }
            printf("%2s ", path.c_str());
            if (it.img == layout.get_selected()) {
                printf("\x1b[0m");
            }
            ++index;
            if ((index % layout.get_columns()) == 0) {
                printf("\n");
            }
        }
        printf("\n");
    }

    Layout layout;
};

TEST_F(LayoutTest, BaseScheme)
{
    InitLayout(15);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
    ASSERT_EQ(layout.get_columns(), 5UL);
    ASSERT_EQ(layout.get_rows(), 4UL);

    ImageEntryPtr selection = layout.get_selected();
    ASSERT_TRUE(selection);
    ASSERT_TRUE(*selection);
    ASSERT_EQ(selection->path, std::string(ImageEntry::SRC_EXEC) + "0");

    ASSERT_EQ(layout.get_scheme().size(), 15UL);
    for (auto& it : layout.get_scheme()) {
        ASSERT_TRUE(it.img && *it.img);
        if (it.img == selection) {
            ASSERT_EQ(it.col, 0UL);
            ASSERT_EQ(it.row, 0UL);
        }
    }
}

TEST_F(LayoutTest, SelectByImage)
{
    InitLayout(30);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");

    const Layout::Thumbnail* th = Select(5);

    ASSERT_TRUE(th);
    ASSERT_TRUE(th->img);
    ASSERT_EQ(th->img, layout.get_selected());
    ASSERT_EQ(th->col, 0UL);
    ASSERT_EQ(th->row, 1UL);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
}

TEST_F(LayoutTest, SelectFirstLast)
{
    InitLayout(27);

    const std::vector<Layout::Thumbnail> scheme = layout.get_scheme();
    ASSERT_EQ(scheme.size(), 20UL);

    ASSERT_TRUE(layout.select(Layout::Last));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://10");
    ASSERT_EQ(layout.get_selected()->path, "exec://26");
    auto [col0, row0] = GetSelection();
    ASSERT_EQ(col0, 1UL);
    ASSERT_EQ(row0, 3UL);

    ASSERT_TRUE(layout.select(Layout::First));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
    ASSERT_EQ(layout.get_selected()->path, "exec://0");
    auto [col1, row1] = GetSelection();
    ASSERT_EQ(col1, 0UL);
    ASSERT_EQ(row1, 0UL);
}

TEST_F(LayoutTest, SelectLeft)
{
    InitLayout(30);

    Select(16);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    auto [col0, row0] = GetSelection();
    ASSERT_EQ(col0, 1UL);
    ASSERT_EQ(row0, 2UL);

    for (ssize_t i = 15; i >= 0; --i) {
        ASSERT_TRUE(layout.select(Layout::Left));
        ASSERT_EQ(layout.get_scheme()[0].img->path,
                  i >= 5 ? "exec://5" : "exec://0");
        ASSERT_EQ(layout.get_selected()->path, "exec://" + std::to_string(i));
    }
    ASSERT_FALSE(layout.select(Layout::Left));
}

TEST_F(LayoutTest, SelectRight)
{
    InitLayout(30);

    Select(16);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    auto [col0, row0] = GetSelection();
    ASSERT_EQ(col0, 1UL);
    ASSERT_EQ(row0, 2UL);

    for (size_t i = 17; i < 30; ++i) {
        ASSERT_TRUE(layout.select(Layout::Right));
        ASSERT_EQ(layout.get_scheme()[0].img->path,
                  i < 25 ? "exec://5" : "exec://10");
        ASSERT_EQ(layout.get_selected()->path, "exec://" + std::to_string(i));
    }
    ASSERT_FALSE(layout.select(Layout::Right));
}

TEST_F(LayoutTest, SelectUp)
{
    InitLayout(30);

    Select(18);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    auto [col0, row0] = GetSelection();
    ASSERT_EQ(col0, 3UL);
    ASSERT_EQ(row0, 2UL);

    ASSERT_TRUE(layout.select(Layout::Up));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    ASSERT_EQ(layout.get_selected()->path, "exec://13");
    auto [col1, row1] = GetSelection();
    ASSERT_EQ(col1, 3UL);
    ASSERT_EQ(row1, 1UL);

    ASSERT_TRUE(layout.select(Layout::Up));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    ASSERT_EQ(layout.get_selected()->path, "exec://8");
    auto [col2, row2] = GetSelection();
    ASSERT_EQ(col2, 3UL);
    ASSERT_EQ(row2, 0UL);

    ASSERT_TRUE(layout.select(Layout::Up));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
    ASSERT_EQ(layout.get_selected()->path, "exec://3");
    auto [col3, row3] = GetSelection();
    ASSERT_EQ(col3, 3UL);
    ASSERT_EQ(row3, 0UL);

    ASSERT_TRUE(layout.select(Layout::Up));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
    ASSERT_EQ(layout.get_selected()->path, "exec://0");
    auto [col4, row4] = GetSelection();
    ASSERT_EQ(col4, 0UL);
    ASSERT_EQ(row4, 0UL);

    ASSERT_FALSE(layout.select(Layout::Up));
}

TEST_F(LayoutTest, SelectDown)
{
    InitLayout(30);

    Select(18);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    auto [col0, row0] = GetSelection();
    ASSERT_EQ(col0, 3UL);
    ASSERT_EQ(row0, 2UL);

    ASSERT_TRUE(layout.select(Layout::Down));
    EXPECT_EQ(layout.get_scheme()[0].img->path, "exec://5");
    EXPECT_EQ(layout.get_selected()->path, "exec://23");
    auto [col1, row1] = GetSelection();
    EXPECT_EQ(col1, 3UL);
    EXPECT_EQ(row1, 3UL);

    ASSERT_TRUE(layout.select(Layout::Down));
    EXPECT_EQ(layout.get_scheme()[0].img->path, "exec://10");
    EXPECT_EQ(layout.get_selected()->path, "exec://28");
    auto [col2, row2] = GetSelection();
    EXPECT_EQ(col2, 3UL);
    EXPECT_EQ(row2, 3UL);

    ASSERT_TRUE(layout.select(Layout::Down));
    EXPECT_EQ(layout.get_scheme()[0].img->path, "exec://10");
    EXPECT_EQ(layout.get_selected()->path, "exec://29");
    auto [col3, row3] = GetSelection();
    EXPECT_EQ(col3, 4UL);
    EXPECT_EQ(row3, 3UL);

    ASSERT_FALSE(layout.select(Layout::Down));
}

TEST_F(LayoutTest, SelectPageUp)
{
    InitLayout(40);

    Select(34);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://20");

    ASSERT_TRUE(layout.select(Layout::PgUp));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
    ASSERT_EQ(layout.get_selected()->path, "exec://14");

    ASSERT_TRUE(layout.select(Layout::PgUp));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");
    ASSERT_EQ(layout.get_selected()->path, "exec://0");

    ASSERT_FALSE(layout.select(Layout::PgUp));
}

TEST_F(LayoutTest, SelectPageDown)
{
    InitLayout(40);

    Select(6);

    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://0");

    ASSERT_TRUE(layout.select(Layout::PgDown));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://20");
    ASSERT_EQ(layout.get_selected()->path, "exec://26");

    ASSERT_TRUE(layout.select(Layout::PgDown));
    ASSERT_EQ(layout.get_scheme()[0].img->path, "exec://20");
    ASSERT_EQ(layout.get_selected()->path, "exec://39");

    ASSERT_FALSE(layout.select(Layout::PgDown));
}

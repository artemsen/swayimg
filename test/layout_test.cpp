// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "imglist.h"
#include "layout.h"
}

#include "config_test.h"

class Layout : public ConfigTest {
protected:
    void SetUp() override
    {
        config_set(config, CFG_LIST, CFG_LIST_ORDER, "alpha");
        imglist_init(config);
        imglist_lock();
        layout_init(&layout, thsize);
    }

    void TearDown() override
    {
        if (HasFailure() && layout.thumb_total) {
            puts("Test failed. Latest layout:");
            PrintLayout();
        }
        layout_free(&layout);
        imglist_unlock();
        imglist_destroy();

        list_for_each(queue, struct image, it) {
            image_free(it, IMGDATA_SELF);
        }
    }

    void InitLayout(size_t total, size_t current = 0, size_t width = 80,
                    size_t height = 60)
    {
        std::vector<std::string> sources_str(total);
        std::vector<const char*> sources(total);
        for (size_t i = 0; i < total; ++i) {
            sources_str[i] = LDRSRC_EXEC;
            if (i < 10) {
                sources_str[i] += '0';
            }
            sources_str[i] += std::to_string(i);
            sources[i] = sources_str[i].c_str();
        }

        imglist_load(&sources[0], sources.size());
        layout.current = imglist_jump(imglist_first(), current);
        layout_resize(&layout, width, height);
    }

    const char* SelectNext(enum layout_dir dir)
    {
        if (!layout_select(&layout, dir)) {
            return "/NA";
        }
        return layout.current ? layout.current->source : "/ER";
    }

    void PrintLayout() const
    {
        printf("  | ");
        for (size_t col = 0; col < layout.columns; ++col) {
            printf("%2ld ", col);
        }
        printf("\n--+");
        for (size_t col = 0; col < layout.columns; ++col) {
            printf("---");
        }
        printf("\n");

        for (size_t row = 0; row < layout.rows; ++row) {
            printf("%ld | ", row);

            for (size_t col = 0; col < layout.columns; ++col) {
                const size_t idx = row * layout.columns + col;
                const char* name = "--";
                if (idx < layout.thumb_total) {
                    if (row == layout.current_row &&
                        col == layout.current_col) {
                        name = "##";
                    } else {
                        name = strrchr(layout.thumbs[idx].img->source, '/') + 1;
                    }
                }
                printf("%2s ", name);
            }
            printf("\n");
        }
    }

    struct layout layout;
    struct image* queue = nullptr;
    static constexpr const size_t thsize = 10;
};

TEST_F(Layout, BaseScheme)
{
    InitLayout(30, 15);

    ASSERT_EQ(layout.columns, static_cast<size_t>(5));
    ASSERT_EQ(layout.rows, static_cast<size_t>(4));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(0));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(2));
    ASSERT_EQ(layout.thumb_total, static_cast<size_t>(20));

    struct image* img = imglist_find("exec://05");
    for (size_t i = 0; i < layout.thumb_total; ++i) {
        EXPECT_EQ(layout.thumbs[i].img, img);
        EXPECT_NE(layout.thumbs[i].x, static_cast<size_t>(0));
        EXPECT_NE(layout.thumbs[i].y, static_cast<size_t>(0));
        img = imglist_next(img, false);
    }
}

TEST_F(Layout, SchemeScrollUp)
{
    InitLayout(30, 17);

    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://05");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(2));

    ASSERT_STREQ(SelectNext(layout_up), "exec://12");
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://05");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));

    ASSERT_STREQ(SelectNext(layout_up), "exec://07");
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://05");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));

    ASSERT_STREQ(SelectNext(layout_up), "exec://02");
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://00");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));
}

TEST_F(Layout, SchemeScrollDown)
{
    InitLayout(30, 17);

    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://05");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(2));

    ASSERT_STREQ(SelectNext(layout_down), "exec://22");
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://05");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(3));

    ASSERT_STREQ(SelectNext(layout_down), "exec://27");
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://10");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(3));
}

TEST_F(Layout, SchemeLast)
{
    InitLayout(7);
    ASSERT_STREQ(SelectNext(layout_last), "exec://06");

    ASSERT_TRUE(layout.thumbs[0].img);
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://00");
    ASSERT_EQ(layout.current_col, static_cast<size_t>(1));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));
}

TEST_F(Layout, SelectFirstLast)
{
    InitLayout(30);

    ASSERT_FALSE(layout_select(&layout, layout_first));
    ASSERT_FALSE(layout_select(&layout, layout_pgup));

    ASSERT_TRUE(layout_select(&layout, layout_last));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(4));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(3));
    ASSERT_EQ(layout.current, imglist_last());

    ASSERT_FALSE(layout_select(&layout, layout_last));
    ASSERT_FALSE(layout_select(&layout, layout_pgdown));
    ASSERT_FALSE(layout_select(&layout, layout_right));
    ASSERT_FALSE(layout_select(&layout, layout_down));

    ASSERT_TRUE(layout_select(&layout, layout_first));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(0));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));
    ASSERT_EQ(layout.current, imglist_first());
}

TEST_F(Layout, SelectEdge)
{
    InitLayout(30);

    ASSERT_TRUE(layout_select(&layout, layout_down));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(0));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 5));

    ASSERT_TRUE(layout_select(&layout, layout_left));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(4));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 4));

    ASSERT_TRUE(layout_select(&layout, layout_right));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(0));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 5));
}

TEST_F(Layout, SelectNearest)
{
    InitLayout(10);

    ASSERT_TRUE(layout_select(&layout, layout_right));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(1));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 1));

    ASSERT_TRUE(layout_select(&layout, layout_down));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(1));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 6));

    ASSERT_TRUE(layout_select(&layout, layout_left));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(0));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 5));

    ASSERT_TRUE(layout_select(&layout, layout_up));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(0));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));
    ASSERT_EQ(layout.current, imglist_first());
}

TEST_F(Layout, SelectPage)
{
    InitLayout(30, 2);

    ASSERT_TRUE(layout_select(&layout, layout_pgdown));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 17));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(1));
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://10");

    ASSERT_FALSE(layout_select(&layout, layout_pgdown));

    ASSERT_TRUE(layout_select(&layout, layout_pgup));
    ASSERT_EQ(layout.current, imglist_jump(imglist_first(), 2));
    ASSERT_EQ(layout.current_col, static_cast<size_t>(2));
    ASSERT_EQ(layout.current_row, static_cast<size_t>(0));
    ASSERT_STREQ(layout.thumbs[0].img->source, "exec://00");

    ASSERT_FALSE(layout_select(&layout, layout_pgup));
}

TEST_F(Layout, Current)
{
    InitLayout(5, 2);

    struct layout_thumb* th = layout_current(&layout);
    ASSERT_TRUE(th);
    EXPECT_EQ(th->img, imglist_jump(imglist_first(), 2));
}

TEST_F(Layout, LdQueueVisibleOnly)
{
    InitLayout(30, 15);

    queue = layout_ldqueue(&layout, 0);
    ASSERT_TRUE(queue);

    ASSERT_EQ(list_size(&queue->list), static_cast<size_t>(20));

    // clang-format off
    const char* const etalon[] = {
        "exec://15",
        "exec://14",
        "exec://16",
        "exec://13",
        "exec://17",
        "exec://12",
        "exec://18",
        "exec://11",
        "exec://19",
        "exec://10",
        "exec://20",
        "exec://09",
        "exec://21",
        "exec://08",
        "exec://22",
        "exec://07",
        "exec://23",
        "exec://06",
        "exec://24",
        "exec://05",
    };
    // clang-format on
    struct image* it = queue;
    for (auto e : etalon) {
        ASSERT_STREQ(it->source, e);
        it = reinterpret_cast<struct image*>(it->list.next);
    }
    EXPECT_FALSE(it);
}

TEST_F(Layout, LdQueueUnimited)
{
    InitLayout(30, 15);

    queue = layout_ldqueue(&layout, 999);
    ASSERT_TRUE(queue);
    ASSERT_EQ(list_size(&queue->list), static_cast<size_t>(30));

    struct image* last = reinterpret_cast<struct image*>(list_get_last(queue));
    ASSERT_TRUE(last);
    ASSERT_STREQ(last->source, "exec://00");
}

TEST_F(Layout, ClearAllInvisible)
{
    InitLayout(30, 15);

    struct image* img = imglist_first();
    while (img) {
        img->data = static_cast<struct imgdata*>(calloc(1, sizeof(*img->data)));
        ASSERT_TRUE(img->data);
        ASSERT_TRUE(pixmap_create(&img->data->thumbnail, pixmap_argb, 1, 1));
        img = imglist_next(img, false);
    }

    layout_clear(&layout, 0);

    img = imglist_first();
    while (img) {
        const bool loaded = image_thumb_get(img);
        const bool expect = img->index > 5 && img->index <= 25;
        if (loaded != expect) {
            fprintf(stderr, "Image %s [%ld] thumb: %c, expected %c\n",
                    img->source, img->index, loaded ? 'Y' : 'N',
                    expect ? 'Y' : 'N');
            FAIL();
        }
        img = imglist_next(img, false);
    }
}

TEST_F(Layout, ClearLimited)
{
    InitLayout(50, 20);

    struct image* img = imglist_first();
    while (img) {
        img->data = static_cast<struct imgdata*>(calloc(1, sizeof(*img->data)));
        ASSERT_TRUE(img->data);
        ASSERT_TRUE(pixmap_create(&img->data->thumbnail, pixmap_argb, 1, 1));
        img = imglist_next(img, false);
    }

    layout_clear(&layout, 10);

    img = imglist_first();
    while (img) {
        const bool loaded = image_thumb_get(img);
        const bool expect = img->index > 5 && img->index <= 35;
        if (loaded != expect) {
            fprintf(stderr, "Image %s [%ld] thumb: %c, expected %c\n",
                    img->source, img->index, loaded ? 'Y' : 'N',
                    expect ? 'Y' : 'N');
            FAIL();
        }
        img = imglist_next(img, false);
    }
}

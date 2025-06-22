// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "config_test.h"

TEST(ConfigLoader, Load)
{
    testing::internal::CaptureStderr();

    struct config* config = config_create();
    ASSERT_TRUE(config);
    ASSERT_TRUE(config_load(config, TEST_DATA_DIR "/config"));

    const struct config* general = config_section(config, CFG_GENERAL);
    ASSERT_NE(general, nullptr);
    EXPECT_STREQ(config_get(general, CFG_GNRL_MODE), "s p a c e s");
    EXPECT_STREQ(config_get(general, CFG_GNRL_APP_ID), "my_ap_id");

    config_free(config);

    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, Defaults)
{
    const struct config* general = config_section(config, CFG_GENERAL);
    EXPECT_STREQ(config_get_default(CFG_GENERAL, CFG_GNRL_APP_ID), "swayimg");
    EXPECT_STREQ(config_get(general, CFG_GNRL_APP_ID), "swayimg");
}

TEST_F(ConfigTest, Set)
{
    const struct config* general = config_section(config, CFG_GENERAL);

    EXPECT_TRUE(config_set(config, CFG_GENERAL, CFG_GNRL_APP_ID, "test123"));
    EXPECT_STREQ(config_get(general, CFG_GNRL_APP_ID), "test123");

    testing::internal::CaptureStderr();

    EXPECT_FALSE(config_set(config, CFG_GENERAL, CFG_GNRL_APP_ID, ""));
    EXPECT_FALSE(config_set(config, CFG_GENERAL, "unknown", "test123"));
    EXPECT_FALSE(config_set(config, "unknown", "unknown", "test123"));

    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, SetArg)
{
    const struct config* general = config_section(config, CFG_GENERAL);

    EXPECT_TRUE(
        config_set_arg(config, CFG_GENERAL "." CFG_GNRL_APP_ID "=test123"));
    EXPECT_STREQ(config_get(general, CFG_GNRL_APP_ID), "test123");

    EXPECT_TRUE(config_set_arg(
        config, "\t\n" CFG_GENERAL "." CFG_GNRL_APP_ID "  = \ttest321"));
    EXPECT_STREQ(config_get(general, CFG_GNRL_APP_ID), "test321");

    testing::internal::CaptureStderr();

    EXPECT_FALSE(config_set_arg(config, ""));
    EXPECT_FALSE(config_set_arg(config, "abc=1"));
    EXPECT_FALSE(config_set_arg(config, "abc.def"));
    EXPECT_FALSE(config_set_arg(config, "abc.def="));

    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, Add)
{
    const struct config* section = config_section(config, CFG_KEYS_VIEWER);

    testing::internal::CaptureStderr();
    EXPECT_STREQ(config_get(section, "F12"), "");
    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());

    EXPECT_TRUE(config_set(config, CFG_KEYS_VIEWER, "F12", "quit"));
    EXPECT_STREQ(config_get(section, "F12"), "quit");
}

TEST_F(ConfigTest, Replace)
{
    const struct config* section = config_section(config, CFG_KEYS_VIEWER);
    EXPECT_STREQ(config_get(section, "F1"), "help");
    EXPECT_TRUE(config_set(config, CFG_KEYS_VIEWER, "F1", "quit"));
    EXPECT_STREQ(config_get(section, "F1"), "quit");
}

TEST_F(ConfigTest, GetDefault)
{
    EXPECT_TRUE(config_set(config, CFG_GENERAL, CFG_GNRL_APP_ID, "test123"));
    EXPECT_STREQ(config_get_default(CFG_GENERAL, CFG_GNRL_APP_ID), "swayimg");

    testing::internal::CaptureStderr();

    EXPECT_STREQ(config_get_default(CFG_GENERAL, "unknown"), "");
    EXPECT_STREQ(config_get_default("unknown", "unknown"), "");

    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, Get)
{
    const struct config* section = config_section(config, CFG_GENERAL);
    EXPECT_STREQ(config_get(section, CFG_GNRL_APP_ID), "swayimg");
    testing::internal::CaptureStderr();
    EXPECT_STREQ(config_get(section, "unknown"), "");
    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, GetOneOf)
{
    const struct config* section = config_section(config, CFG_LIST);
    const char* possible[] = { "one", "two", "three" };

    EXPECT_TRUE(config_set(config, CFG_LIST, CFG_LIST_ORDER, "two"));
    EXPECT_EQ(config_get_oneof(section, CFG_LIST_ORDER, possible, 3), 1);

    testing::internal::CaptureStderr();
    EXPECT_TRUE(config_set(config, CFG_LIST, CFG_LIST_ORDER, "four"));
    EXPECT_EQ(config_get_oneof(section, CFG_LIST_ORDER, possible, 3), 0);
    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, GetBool)
{
    const struct config* section = config_section(config, CFG_GALLERY);

    EXPECT_TRUE(config_set(config, CFG_GALLERY, CFG_GLRY_FILL, CFG_YES));
    EXPECT_TRUE(config_get_bool(section, CFG_GLRY_FILL));
    EXPECT_TRUE(config_set(config, CFG_GALLERY, CFG_GLRY_FILL, CFG_NO));
    EXPECT_FALSE(config_get_bool(section, CFG_GLRY_FILL));
}

TEST_F(ConfigTest, GetNum)
{
    const struct config* section = config_section(config, CFG_FONT);

    EXPECT_TRUE(config_set(config, CFG_FONT, CFG_FONT_SIZE, "123"));
    EXPECT_EQ(config_get_num(section, CFG_FONT_SIZE, 0, 1024), 123);

    testing::internal::CaptureStderr();
    EXPECT_EQ(config_get_num(section, CFG_FONT_SIZE, 0, -1), 14);
    EXPECT_EQ(config_get_num(section, CFG_FONT_SIZE, 0, 1), 14);
    EXPECT_EQ(config_get_num(section, CFG_FONT_SIZE, -1, 0), 14);
    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

TEST_F(ConfigTest, GetColor)
{
    const struct config* section = config_section(config, CFG_VIEWER);

    config_set(config, CFG_VIEWER, CFG_VIEW_WINDOW, "#010203");
    EXPECT_EQ(config_get_color(section, CFG_VIEW_WINDOW),
              static_cast<argb_t>(0xff010203));

    config_set(config, CFG_VIEWER, CFG_VIEW_WINDOW, "#010203aa");
    EXPECT_EQ(config_get_color(section, CFG_VIEW_WINDOW),
              static_cast<argb_t>(0xaa010203));

    config_set(config, CFG_VIEWER, CFG_VIEW_WINDOW, "010203aa");
    EXPECT_EQ(config_get_color(section, CFG_VIEW_WINDOW),
              static_cast<argb_t>(0xaa010203));

    config_set(config, CFG_VIEWER, CFG_VIEW_WINDOW, "# 010203aa");
    EXPECT_EQ(config_get_color(section, CFG_VIEW_WINDOW),
              static_cast<argb_t>(0xaa010203));

    testing::internal::CaptureStderr();
    config_set(config, CFG_VIEWER, CFG_VIEW_WINDOW, "invalid");
    config_get_color(section, CFG_VIEW_WINDOW);
    EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "config.h"
}

#include <gtest/gtest.h>

class Config : public ::testing::Test {
protected:
    void TearDown() override { config_free(config); }
    struct config* config = nullptr;
};

TEST_F(Config, Set)
{
    config_set(&config, "section123", "key1", "value1");
    EXPECT_STREQ(config_get(config, "section123", "key1"), "value1");
    config_set(&config, "section123", "key1", "value2");
    EXPECT_STREQ(config_get(config, "section123", "key1"), "value2");

    config_set(&config, "section321", "key2", "");
    EXPECT_STREQ(config_get(config, "section321", "key2"), "");
    config_set(&config, "section321", "key3", "123");
    EXPECT_STREQ(config_get(config, "section321", "key3"), "123");
}

TEST_F(Config, SetArg)
{
    EXPECT_TRUE(config_set_arg(&config, "section123.key1=value1"));
    EXPECT_STREQ(config_get(config, "section123", "key1"), "value1");

    EXPECT_TRUE(config_set_arg(&config, "\t\nsub.section.command  = \t42"));
    EXPECT_STREQ(config_get(config, "sub.section", "command"), "42");

    EXPECT_FALSE(config_set_arg(&config, ""));
    EXPECT_FALSE(config_set_arg(&config, "abc=1"));
    EXPECT_FALSE(config_set_arg(&config, "abc.def"));
    EXPECT_FALSE(config_set_arg(&config, "abc.def="));
}

TEST_F(Config, Load)
{
    setenv("XDG_CONFIG_HOME", TEST_DATA_DIR, 1);
    config = config_load();

    ASSERT_NE(config, nullptr);
    EXPECT_STREQ(config_get(config, "test.section", "spaces"), "s p a c e s");
    EXPECT_STREQ(config_get(config, "test.section", "nospaces"), "nospaces");
    EXPECT_STREQ(config_get(config, "test.section", "empty"), "");
}

TEST_F(Config, GetString)
{
    config_set(&config, "section", "key", "value");
    EXPECT_STREQ(config_get_string(config, "section", "key", ""), "value");
    EXPECT_STREQ(config_get_string(config, "section", "key1", "def"), "def");
    EXPECT_STREQ(config_get_string(config, "section1", "key", "def"), "def");
}

TEST_F(Config, GetBool)
{
    config_set(&config, "section", "key", "yes");
    EXPECT_TRUE(config_get_bool(config, "section", "key", true));
    EXPECT_TRUE(config_get_bool(config, "section", "key", false));

    config_set(&config, "section", "key", "no");
    EXPECT_FALSE(config_get_bool(config, "section", "key", true));
    EXPECT_FALSE(config_get_bool(config, "section", "key", false));

    config_set(&config, "section", "key", "invalid");
    EXPECT_TRUE(config_get_bool(config, "section", "key", true));

    EXPECT_TRUE(config_get_bool(config, "section", "key1", true));
    EXPECT_FALSE(config_get_bool(config, "section", "key1", false));
    EXPECT_TRUE(config_get_bool(config, "section1", "key", true));
    EXPECT_FALSE(config_get_bool(config, "section1", "key", false));
}

TEST_F(Config, ToColor)
{
    config_set(&config, "section", "key", "#010203");
    EXPECT_EQ(config_get_color(config, "section", "key", 0xbaaaaaad),
              static_cast<argb_t>(0xff010203));

    config_set(&config, "section", "key", "#010203aa");
    EXPECT_EQ(config_get_color(config, "section", "key", 0xbaaaaaad),
              static_cast<argb_t>(0xaa010203));

    config_set(&config, "section", "key", "010203aa");
    EXPECT_EQ(config_get_color(config, "section", "key", 0xbaaaaaad),
              static_cast<argb_t>(0xaa010203));

    config_set(&config, "section", "key", "# 010203aa");
    EXPECT_EQ(config_get_color(config, "section", "key", 0xbaaaaaad),
              static_cast<argb_t>(0xaa010203));

    config_set(&config, "section", "key", "invalid");
    EXPECT_EQ(config_get_color(config, "section", "key", 0x11223344),
              static_cast<argb_t>(0x11223344));
    EXPECT_EQ(config_get_color(config, "section", "key1", 0x11223344),
              static_cast<argb_t>(0x11223344));
    EXPECT_EQ(config_get_color(config, "section1", "key", 0x11223344),
              static_cast<argb_t>(0x11223344));
}

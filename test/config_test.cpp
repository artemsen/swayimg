// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "config.h"
}

#include <gtest/gtest.h>

static std::map<std::string, std::string> config;

static enum config_status on_load(const char* key, const char* value)
{
    config.insert(std::make_pair(key, value));
    return cfgst_ok;
}

#define EXPECT_CONFIG(k, v)                                                  \
    {                                                                        \
        const auto& it = config.find(k);                                     \
        const char* val = (it == config.end() ? "N/A" : it->second.c_str()); \
        EXPECT_STREQ(val, v);                                                \
    }

TEST(Config, Load)
{
    setenv("XDG_CONFIG_HOME", TEST_DATA_DIR, 1);

    config_add_loader("test_section", on_load);
    config_init();

    EXPECT_EQ(config.size(), 3);

    auto check = [](const char* key, const char* val) {
        const auto& it = config.find(key);
        const char* real = (it == config.end() ? "N/A" : it->second.c_str());
        EXPECT_STREQ(real, val);
    };
    check("spaces", "s p a c e s");
    check("nospaces", "nospaces");
    check("empty", "");

    config_free();
}

TEST(Config, ToBool)
{
    bool rc;

    EXPECT_TRUE(config_to_bool("true", &rc));
    EXPECT_TRUE(rc);
    EXPECT_TRUE(config_to_bool("false", &rc));
    EXPECT_FALSE(rc);
    EXPECT_TRUE(config_to_bool("yes", &rc));
    EXPECT_TRUE(rc);
    EXPECT_TRUE(config_to_bool("no", &rc));
    EXPECT_FALSE(rc);

    EXPECT_FALSE(config_to_bool("", &rc));
    EXPECT_FALSE(config_to_bool("abc", &rc));
}

TEST(Config, ToColor)
{
    argb_t argb;

    EXPECT_TRUE(config_to_color("#010203", &argb));
    EXPECT_EQ(argb, 0xff010203);
    EXPECT_TRUE(config_to_color("#010203aa", &argb));
    EXPECT_EQ(argb, 0xaa010203);
    EXPECT_TRUE(config_to_color("010203aa", &argb));
    EXPECT_EQ(argb, 0xaa010203);
    EXPECT_TRUE(config_to_color("# 010203aa", &argb));
    EXPECT_EQ(argb, 0xaa010203);
    EXPECT_TRUE(config_to_color("", &argb));
    EXPECT_EQ(argb, 0xff000000);

    EXPECT_FALSE(config_to_color("invalid value", &argb));
}

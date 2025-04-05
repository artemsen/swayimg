// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "config.h"
}

#include <gtest/gtest.h>

class ConfigTest : public ::testing::Test {
protected:
    ConfigTest()
    {
        unsetenv("XDG_CONFIG_HOME");
        unsetenv("XDG_CONFIG_DIRS");
        unsetenv("HOME");
        config = config_load();
        EXPECT_TRUE(config);
    }

    virtual ~ConfigTest()
    {
        if (config) {
            config_free(config);
        }
    }

    struct config* config = nullptr;
};

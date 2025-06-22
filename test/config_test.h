// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

extern "C" {
#include "config.h"
}

#include <gtest/gtest.h>

class ConfigTest : public ::testing::Test {
protected:
    ConfigTest()
    {
        config = config_create();
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

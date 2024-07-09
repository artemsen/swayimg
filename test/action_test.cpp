// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "action.h"
}

#include <gtest/gtest.h>

class Action : public ::testing::Test {
protected:
    void TearDown() override { free(action.params); }
    struct action action = { action_none, nullptr };
};

TEST_F(Action, Create)
{
    ASSERT_TRUE(action_load(&action, "info", 4));
    EXPECT_STREQ(action_typename(&action), "info");
    EXPECT_EQ(action.params, nullptr);
}

TEST_F(Action, Params)
{
    const char* text = "exec \t  param 123";

    ASSERT_TRUE(action_load(&action, text, strlen(text)));
    EXPECT_STREQ(action.params, "param 123");
    EXPECT_STREQ(action_typename(&action), "exec");
}

TEST_F(Action, Fail)
{
    EXPECT_FALSE(action_load(&action, "", 0));
    EXPECT_FALSE(action_load(&action, "invalid", 7));
}

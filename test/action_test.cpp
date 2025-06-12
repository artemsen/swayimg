// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "action.h"
}

#include <gtest/gtest.h>

class Action : public ::testing::Test {
protected:
    void TearDown() override { action_free(actions); }
    struct action* actions = nullptr;
};

TEST_F(Action, Create)
{
    actions = action_create("info");
    ASSERT_TRUE(actions);
    EXPECT_FALSE(actions->next);
    EXPECT_STREQ(actions->params, "");
    EXPECT_EQ(actions->type, action_info);
}

TEST_F(Action, Fail)
{
    EXPECT_FALSE(action_create(""));
    EXPECT_FALSE(action_create("invalid"));
    EXPECT_FALSE(action_create("info123 exec"));
}

TEST_F(Action, Params)
{
    actions = action_create("exec \t  param 123 ");
    ASSERT_TRUE(actions);
    EXPECT_FALSE(actions->next);
    EXPECT_EQ(actions->type, action_exec);
    EXPECT_STREQ(actions->params, "param 123");
}

TEST_F(Action, Sequence)
{
    actions = action_create("exec cmd;\nreload ;\t exit;status ok");

    struct action* it = actions;
    ASSERT_TRUE(it);
    EXPECT_EQ(it->type, action_exec);
    EXPECT_STREQ(it->params, "cmd");

    it = it->next;
    ASSERT_TRUE(it);
    EXPECT_EQ(it->type, action_reload);
    EXPECT_STREQ(it->params, "");

    it = it->next;
    ASSERT_TRUE(it);
    EXPECT_EQ(it->type, action_exit);
    EXPECT_STREQ(it->params, "");

    it = it->next;
    ASSERT_TRUE(it);
    EXPECT_EQ(it->type, action_status);
    EXPECT_STREQ(it->params, "ok");

    EXPECT_FALSE(it->next);
}

TEST_F(Action, FailSequence)
{
    ASSERT_FALSE(action_create("exec cmd;\nreload;invalid"));
}

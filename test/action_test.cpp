// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "action.h"
}

#include <gtest/gtest.h>

class Action : public ::testing::Test {
protected:
    void TearDown() override { action_free(&actions); }
    struct action_seq actions = { nullptr, 0 };
};

TEST_F(Action, Create)
{
    ASSERT_TRUE(action_create("info", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(1));
    ASSERT_NE(actions.sequence, nullptr);
    ASSERT_EQ(actions.sequence[0].type, action_info);
    ASSERT_EQ(actions.sequence[0].params, nullptr);
}

TEST_F(Action, Fail)
{
    ASSERT_FALSE(action_create("", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(0));
    ASSERT_EQ(actions.sequence, nullptr);

    ASSERT_FALSE(action_create("invalid", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(0));
    ASSERT_EQ(actions.sequence, nullptr);

    ASSERT_FALSE(action_create("info123 exec", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(0));
    ASSERT_EQ(actions.sequence, nullptr);
}

TEST_F(Action, Params)
{
    ASSERT_TRUE(action_create("exec \t  param 123 ", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(1));
    ASSERT_NE(actions.sequence, nullptr);
    ASSERT_EQ(actions.sequence[0].type, action_exec);
    ASSERT_STREQ(actions.sequence[0].params, "param 123");
}

TEST_F(Action, Sequence)
{
    ASSERT_TRUE(action_create("exec cmd;\nreload;\t exit;status ok", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(4));
    ASSERT_NE(actions.sequence, nullptr);
    ASSERT_EQ(actions.sequence[0].type, action_exec);
    ASSERT_STREQ(actions.sequence[0].params, "cmd");
    ASSERT_EQ(actions.sequence[1].type, action_reload);
    ASSERT_EQ(actions.sequence[1].params, nullptr);
    ASSERT_EQ(actions.sequence[2].type, action_exit);
    ASSERT_EQ(actions.sequence[2].params, nullptr);
    ASSERT_EQ(actions.sequence[3].type, action_status);
    ASSERT_STREQ(actions.sequence[3].params, "ok");
}

TEST_F(Action, FailSequence)
{
    ASSERT_FALSE(action_create("exec cmd;\nreload;invalid", &actions));
    ASSERT_EQ(actions.num, static_cast<size_t>(0));
    ASSERT_EQ(actions.sequence, nullptr);
}

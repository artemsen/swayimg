// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "action.h"
}

#include <gtest/gtest.h>

class Action : public ::testing::Test {
protected:
    void TearDown() override { action_free(&sequence); }
    struct action actions[3];
    struct action_seq sequence = { actions, 3 };
};

TEST_F(Action, Create)
{
    ASSERT_EQ(action_create("info", &sequence), static_cast<size_t>(1));
    EXPECT_EQ(sequence.num, static_cast<size_t>(1));
    EXPECT_NE(sequence.sequence, nullptr);
    EXPECT_EQ(sequence.sequence[0].type, action_info);
    EXPECT_EQ(sequence.sequence[0].params, nullptr);
    EXPECT_STREQ(action_typename(sequence.sequence), "info");
}

TEST_F(Action, Fail)
{
    ASSERT_EQ(action_create("", &sequence), static_cast<size_t>(0));
    EXPECT_EQ(sequence.num, static_cast<size_t>(3));
    ASSERT_EQ(action_create("invalid", &sequence), static_cast<size_t>(0));
    sequence.num = 0;
}

TEST_F(Action, Params)
{
    ASSERT_EQ(action_create("exec \t  param 123 ", &sequence),
              static_cast<size_t>(1));
    EXPECT_EQ(sequence.num, static_cast<size_t>(1));
    EXPECT_NE(sequence.sequence, nullptr);
    EXPECT_EQ(sequence.sequence[0].type, action_exec);
    EXPECT_STREQ(sequence.sequence[0].params, "param 123");
}

TEST_F(Action, Sequence)
{
    ASSERT_EQ(action_create("exec cmd;\nreload;\t exit;status ok", &sequence),
              static_cast<size_t>(3));
    EXPECT_EQ(sequence.num, static_cast<size_t>(3));
    EXPECT_EQ(sequence.sequence[0].type, action_exec);
    EXPECT_STREQ(sequence.sequence[0].params, "cmd");
    EXPECT_EQ(sequence.sequence[1].type, action_reload);
    EXPECT_EQ(sequence.sequence[1].params, nullptr);
    EXPECT_EQ(sequence.sequence[2].type, action_exit);
    EXPECT_EQ(sequence.sequence[2].params, nullptr);
}

TEST_F(Action, FailSequence)
{
    ASSERT_EQ(action_create("exec cmd;\nreload;invalid", &sequence),
              static_cast<size_t>(0));
    sequence.num = 0;
}

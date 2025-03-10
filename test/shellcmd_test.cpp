// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "shellcmd.h"
}

#include <gtest/gtest.h>

TEST(ShellCmd, ExecOk)
{
    char* out = nullptr;

    EXPECT_EQ(shellcmd_expr("echo %/%%/%", "test123", &out), 0);
    ASSERT_TRUE(out);
    EXPECT_STREQ(out, "test123/%/test123\n");

    free(out);
}

TEST(ShellCmd, ExecFail)
{
    char* out = nullptr;

    EXPECT_EQ(shellcmd_expr("echo -n % && exit 42", "test123", &out), 42);
    ASSERT_TRUE(out);
    EXPECT_STREQ(out, "test123");

    free(out);
}

TEST(ShellCmd, ExecEmpty)
{
    char* out = nullptr;

    EXPECT_EQ(shellcmd_expr("", "", &out), EINVAL);
    ASSERT_FALSE(out);

    free(out);
}

TEST(ShellCmd, StdInRead)
{
    char* out = nullptr;
    EXPECT_NE(shellcmd_expr("read", "", &out), 0); // should not hang
    free(out);
}

class ShellCmdBad : public ::testing::Test {
protected:
    void SetUp() override
    {
        shell = getenv("SHELL");
        setenv("SHELL", "/bad/shell", 1);
    }
    void TearDown() override
    {
        if (shell) {
            setenv("SHELL", shell, 1);
        } else {
            unsetenv("SHELL");
        }
    }
    const char* shell;
};

TEST_F(ShellCmdBad, BadShell)
{
    char* out = nullptr;

    EXPECT_EQ(shellcmd_expr("echo %", "test123", &out), ENOENT);
    EXPECT_FALSE(out);

    free(out);
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "shellcmd.h"
}

#include <gtest/gtest.h>

class ShellCmd : public ::testing::Test {
protected:
    void TearDown() override { free(cmd); }
    char* cmd;
};

TEST_F(ShellCmd, Expression)
{
    cmd = shellcmd_expr("Expression: %/%%/ < %", "test123");
    EXPECT_STREQ(cmd, "Expression: test123/%/ < test123");
}

TEST_F(ShellCmd, ExpressionEmpty)
{
    cmd = shellcmd_expr("", "test123");
    EXPECT_FALSE(cmd);
}

class ShellExec : public ::testing::Test {
protected:
    void TearDown() override
    {
        arr_free(out);
        arr_free(err);
    }

    struct array* out = nullptr;
    struct array* err = nullptr;
};

TEST_F(ShellExec, Execute)
{
    const char last_null = 0;

    EXPECT_EQ(shellcmd_exec("echo out && echo err >&2", &out, &err), 0);

    ASSERT_TRUE(out);
    out = arr_append(out, &last_null, 1);
    ASSERT_TRUE(out);
    EXPECT_STREQ(reinterpret_cast<const char*>(out->data), "out\n");

    ASSERT_TRUE(err);
    err = arr_append(err, &last_null, 1);
    ASSERT_TRUE(err);
    EXPECT_STREQ(reinterpret_cast<const char*>(err->data), "err\n");
}

TEST_F(ShellExec, Fail)
{
    EXPECT_EQ(shellcmd_exec("exit 42", &out, &err), 42);
    EXPECT_FALSE(out);
    EXPECT_FALSE(err);
}

TEST_F(ShellExec, Empty)
{
    EXPECT_EQ(shellcmd_exec("", &out, &err), EINVAL);
    EXPECT_FALSE(out);
    EXPECT_FALSE(err);
}

TEST_F(ShellExec, Stdin)
{
    // should not hang
    EXPECT_NE(shellcmd_exec("read", &out, &err), 0);
}

class ShellBad : public ShellExec {
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
        ShellExec::TearDown();
    }
    const char* shell;
};

TEST_F(ShellBad, Execute)
{
    EXPECT_EQ(shellcmd_exec("echo test123", &out, &err), ENOENT);
}

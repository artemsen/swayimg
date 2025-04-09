// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "fs.h"
}

#include <gtest/gtest.h>

TEST(FileSystem, AppendPath)
{
    char path[256] = { 0 };

    EXPECT_FALSE(fs_append_path("123", path, 0));
    EXPECT_FALSE(fs_append_path("123", path, 1));

    strcpy(path, "/root");
    EXPECT_EQ(fs_append_path("abc", path, sizeof(path)), strlen("/root/abc"));
    EXPECT_STREQ(path, "/root/abc");

    strcpy(path, "/root");
    EXPECT_TRUE(fs_append_path("/abc", path, sizeof(path)));
    EXPECT_STREQ(path, "/root/abc");

    strcpy(path, "/root/");
    EXPECT_TRUE(fs_append_path("/abc", path, sizeof(path)));
    EXPECT_STREQ(path, "/root/abc");

    strcpy(path, "/root");
    EXPECT_TRUE(fs_append_path("", path, sizeof(path)));
    EXPECT_STREQ(path, "/root/");

    strcpy(path, "/root");
    EXPECT_TRUE(fs_append_path(NULL, path, sizeof(path)));
    EXPECT_STREQ(path, "/root/");
}

TEST(FileSystem, Absolute)
{
    char path[256] = { 0 };

    EXPECT_FALSE(fs_abspath("/abs/path", path, 0));
    EXPECT_FALSE(fs_abspath("/abs/path", path, 1));
    EXPECT_EQ(fs_abspath("/abs/path", path, sizeof(path)), strlen("/abs/path"));

    EXPECT_TRUE(fs_abspath("/abs/path", path, sizeof(path)));
    EXPECT_STREQ(path, "/abs/path");

    EXPECT_TRUE(fs_abspath("/1/./2/path", path, sizeof(path)));
    EXPECT_STREQ(path, "/1/2/path");

    EXPECT_TRUE(fs_abspath("/1/../2/path", path, sizeof(path)));
    EXPECT_STREQ(path, "/2/path");
    EXPECT_TRUE(fs_abspath("/1/path/2/..", path, sizeof(path)));
    EXPECT_STREQ(path, "/1/path/");

    EXPECT_TRUE(fs_abspath("/1/2/path/./", path, sizeof(path)));
    EXPECT_STREQ(path, "/1/2/path/");

    EXPECT_TRUE(fs_abspath("../path", path, sizeof(path)));
    EXPECT_EQ(path[0], '/');

    EXPECT_TRUE(fs_abspath("./path", path, sizeof(path)));
    EXPECT_EQ(path[0], '/');
}

TEST(FileSystem, EnvPath)
{
    char path[256] = { 0 };

    const char* postfix = "/dir/file.ext";
    EXPECT_EQ(fs_envpath(NULL, postfix, path, 0), static_cast<size_t>(0));
    EXPECT_EQ(fs_envpath(NULL, postfix, path, 1), static_cast<size_t>(0));
    EXPECT_EQ(fs_envpath(NULL, postfix, path, strlen(postfix)),
              static_cast<size_t>(0));

    EXPECT_EQ(fs_envpath(NULL, postfix, path, sizeof(path)), strlen(postfix));
    EXPECT_STREQ(path, postfix);

    const char* env_key = "SWAYIMG_TEST";
    const char* env_val = "/root";

    setenv(env_key, env_val, 1);
    EXPECT_EQ(fs_envpath(env_key, postfix, path, sizeof(path)),
              strlen(postfix) + strlen(env_val));
    EXPECT_STREQ(path, "/root/dir/file.ext");
    unsetenv(env_key);

    env_val = "/root:/abc";
    setenv(env_key, env_val, 1);
    EXPECT_TRUE(fs_envpath(env_key, postfix, path, sizeof(path)));
    EXPECT_STREQ(path, "/root/dir/file.ext");
    unsetenv(env_key);
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "config.h"
#include "keybind.h"
}

#include "config_test.h"

class Keybind : public ConfigTest {
protected:
    void TearDown() override { keybind_destroy(); }
};

TEST_F(Keybind, Add)
{
    config_set(config, CFG_KEYS_VIEWER, "a", "exit");
    keybind_init(config);

    const struct keybind* kb = keybind_find('a', 0);
    ASSERT_NE(kb, nullptr);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(1));
    EXPECT_EQ(kb->actions.sequence[0].type, action_exit);
    EXPECT_STREQ(kb->help, "a: exit");
}

TEST_F(Keybind, Replace)
{
    config_set(config, CFG_KEYS_VIEWER, "Escape", "info");
    keybind_init(config);

    const struct keybind* kb = keybind_find(XKB_KEY_Escape, 0);
    ASSERT_NE(kb, nullptr);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>(XKB_KEY_Escape));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(1));
    EXPECT_EQ(kb->actions.sequence[0].type, action_info);
}

TEST_F(Keybind, Mods)
{
    config_set(config, CFG_KEYS_VIEWER, "Ctrl+a", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Alt+b", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Shift+c", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Alt+Ctrl+d", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Ctrl+Shift+Alt+e", "exit");
    keybind_init(config);

    EXPECT_NE(keybind_find('a', KEYMOD_CTRL), nullptr);
    EXPECT_NE(keybind_find('b', KEYMOD_ALT), nullptr);
    EXPECT_NE(keybind_find('c', KEYMOD_SHIFT), nullptr);
    EXPECT_NE(keybind_find('d', KEYMOD_CTRL | KEYMOD_ALT), nullptr);
    EXPECT_NE(keybind_find('e', KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT),
              nullptr);
}

TEST_F(Keybind, ActionParams)
{
    config_set(config, CFG_KEYS_VIEWER, "a", "status  \t params 1 2 3\t");
    keybind_init(config);

    const struct keybind* kb = keybind_find('a', 0);
    ASSERT_NE(kb, nullptr);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(1));
    EXPECT_EQ(kb->actions.sequence[0].type, action_status);
    EXPECT_STREQ(kb->actions.sequence[0].params, "params 1 2 3");
    EXPECT_STREQ(kb->help, "a: status params 1 2 3");
}

TEST_F(Keybind, Multiaction)
{
    config_set(config, CFG_KEYS_VIEWER, "a", "exec cmd;reload;exit");
    keybind_init(config);

    const struct keybind* kb = keybind_find('a', 0);
    ASSERT_NE(kb, nullptr);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(3));
    EXPECT_EQ(kb->actions.sequence[0].type, action_exec);
    EXPECT_EQ(kb->actions.sequence[1].type, action_reload);
    EXPECT_EQ(kb->actions.sequence[2].type, action_exit);
    EXPECT_STREQ(kb->help, "a: exec cmd; ...");
}

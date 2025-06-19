// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "config.h"
#include "keybind.h"
}

#include "config_test.h"

class Keybind : public ConfigTest {
protected:
    void TearDown() override { keybind_free(keybind); }
    struct keybind* keybind = nullptr;
};

TEST_F(Keybind, Add)
{
    config_set(config, CFG_KEYS_VIEWER, "a", "exit");
    keybind = keybind_load(config_section(config, CFG_KEYS_VIEWER));

    const struct keybind* kb = keybind_find(keybind, 'a', 0);
    ASSERT_TRUE(kb);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_TRUE(kb->actions);
    EXPECT_EQ(kb->actions->type, action_exit);
    EXPECT_STREQ(kb->actions->params, "");
    EXPECT_FALSE(kb->actions->next);
    EXPECT_STREQ(kb->help, "a: exit");
}

TEST_F(Keybind, Replace)
{
    config_set(config, CFG_KEYS_VIEWER, "Escape", "info");
    keybind = keybind_load(config_section(config, CFG_KEYS_VIEWER));

    const struct keybind* kb = keybind_find(keybind, XKB_KEY_Escape, 0);
    ASSERT_TRUE(kb);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>(XKB_KEY_Escape));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    EXPECT_EQ(kb->actions->type, action_info);
    EXPECT_STREQ(kb->actions->params, "");
    EXPECT_FALSE(kb->actions->next);
}

TEST_F(Keybind, Mods)
{
    config_set(config, CFG_KEYS_VIEWER, "Ctrl+a", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Alt+b", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Shift+c", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Alt+Ctrl+d", "exit");
    config_set(config, CFG_KEYS_VIEWER, "Ctrl+Shift+Alt+e", "exit");
    keybind = keybind_load(config_section(config, CFG_KEYS_VIEWER));

    EXPECT_NE(keybind_find(keybind, 'a', KEYMOD_CTRL), nullptr);
    EXPECT_NE(keybind_find(keybind, 'b', KEYMOD_ALT), nullptr);
    EXPECT_NE(keybind_find(keybind, 'c', KEYMOD_SHIFT), nullptr);
    EXPECT_NE(keybind_find(keybind, 'd', KEYMOD_CTRL | KEYMOD_ALT), nullptr);
    EXPECT_NE(
        keybind_find(keybind, 'e', KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT),
        nullptr);
}

TEST_F(Keybind, ActionParams)
{
    config_set(config, CFG_KEYS_VIEWER, "a", "status  \t params 1 2 3\t");
    keybind = keybind_load(config_section(config, CFG_KEYS_VIEWER));

    const struct keybind* kb = keybind_find(keybind, 'a', 0);
    ASSERT_TRUE(kb);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));
    EXPECT_EQ(kb->actions->type, action_status);
    EXPECT_STREQ(kb->actions->params, "params 1 2 3");
    EXPECT_FALSE(kb->actions->next);
    EXPECT_STREQ(kb->help, "a: status params 1 2 3");
}

TEST_F(Keybind, Multiaction)
{
    config_set(config, CFG_KEYS_VIEWER, "a", "exec cmd;reload;exit");
    keybind = keybind_load(config_section(config, CFG_KEYS_VIEWER));

    const struct keybind* kb = keybind_find(keybind, 'a', 0);
    ASSERT_TRUE(kb);
    EXPECT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    EXPECT_EQ(kb->mods, static_cast<uint8_t>(0));

    struct action* it = kb->actions;
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

    EXPECT_FALSE(it->next);
}

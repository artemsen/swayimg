// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "config.h"
#include "keybind.h"
}

#include <gtest/gtest.h>

class Keybind : public ::testing::Test {
protected:
    void SetUp() override
    {
        keybind_create();  // register config callback
        keybind_destroy(); // clear default bindings
    }

    void TearDown() override
    {
        keybind_destroy();
        config_destroy();
    }

    static constexpr const char* section = "keys.viewer";
};

TEST_F(Keybind, AddOne)
{
    ASSERT_EQ(keybind_get(), nullptr);

    ASSERT_EQ(config_set(section, "a", "exit"), cfgst_ok);

    const struct keybind* kb = keybind_get();
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions.sequence[0].type, action_exit);
    ASSERT_STREQ(kb->help, "a: exit");
    ASSERT_EQ(kb->next, nullptr);
}

TEST_F(Keybind, Replace)
{
    ASSERT_EQ(config_set(section, "a", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "b", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "b", "reload"), cfgst_ok);

    const struct keybind* kb = keybind_get();
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('b'));
    ASSERT_EQ(kb->actions.sequence[0].type, action_reload);
    ASSERT_EQ(kb->next->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->next->actions.sequence[0].type, action_exit);
    ASSERT_EQ(kb->next->next, nullptr);
}

TEST_F(Keybind, Find)
{
    const struct keybind* kb;

    ASSERT_EQ(config_set(section, "a", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "Ctrl+Alt+Shift+b", "reload"), cfgst_ok);

    kb = keybind_find('a', 0);
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.sequence[0].type, action_exit);

    kb = keybind_find('b', KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT);
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('b'));
    ASSERT_EQ(kb->mods,
              static_cast<uint8_t>(KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT));
    ASSERT_EQ(kb->actions.sequence[0].type, action_reload);

    ASSERT_EQ(keybind_find('a', KEYMOD_CTRL), nullptr);
    ASSERT_EQ(keybind_find('c', 0), nullptr);
}

TEST_F(Keybind, Mods)
{
    ASSERT_EQ(config_set(section, "Ctrl+a", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "Alt+b", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "Shift+c", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "Alt+Ctrl+d", "exit"), cfgst_ok);
    ASSERT_EQ(config_set(section, "Ctrl+Shift+Alt+e", "exit"), cfgst_ok);

    ASSERT_NE(keybind_find('a', KEYMOD_CTRL), nullptr);
    ASSERT_NE(keybind_find('b', KEYMOD_ALT), nullptr);
    ASSERT_NE(keybind_find('c', KEYMOD_SHIFT), nullptr);
    ASSERT_NE(keybind_find('d', KEYMOD_CTRL | KEYMOD_ALT), nullptr);
    ASSERT_NE(keybind_find('e', KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT),
              nullptr);
}

TEST_F(Keybind, ActionParams)
{
    const struct keybind* kb;

    ASSERT_EQ(config_set(section, "a", "exit"), cfgst_ok);

    kb = keybind_get();
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions.sequence[0].type, action_exit);
    ASSERT_EQ(kb->next, nullptr);

    ASSERT_EQ(config_set(section, "a", "status  \t params 1 2 3\t"), cfgst_ok);
    kb = keybind_get();
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions.sequence[0].type, action_status);
    ASSERT_STREQ(kb->actions.sequence[0].params, "params 1 2 3");
    ASSERT_EQ(kb->next, nullptr);
}

TEST_F(Keybind, Multiaction)
{
    ASSERT_EQ(config_set(section, "a", "exec cmd;reload;exit"), cfgst_ok);

    const struct keybind* kb = keybind_get();
    ASSERT_NE(kb, nullptr);
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, static_cast<uint8_t>(0));
    ASSERT_EQ(kb->actions.num, static_cast<size_t>(3));
    ASSERT_EQ(kb->actions.sequence[0].type, action_exec);
    ASSERT_EQ(kb->actions.sequence[1].type, action_reload);
    ASSERT_EQ(kb->actions.sequence[2].type, action_exit);
    ASSERT_STREQ(kb->actions.sequence[0].params, "cmd");
    ASSERT_EQ(kb->next, nullptr);
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "keybind.h"
}

#include <gtest/gtest.h>

class Keybind : public ::testing::Test {
protected:
    void TearDown() override { keybind_destroy(); }

    size_t KeybindSize() const
    {
        const struct keybind* kb = keybind_all();
        size_t sz = 0;
        while (kb) {
            ++sz;
            kb = kb->next;
        }
        return sz;
    }
};

TEST_F(Keybind, Create)
{
    ASSERT_EQ(keybind_all(), nullptr);
    keybind_create();
    ASSERT_NE(keybind_all(), nullptr);
}

TEST_F(Keybind, Configure)
{
    const struct keybind* kb;

    ASSERT_EQ(keybind_configure("a", "exit"), cfgst_ok);
    ASSERT_EQ(KeybindSize(), static_cast<size_t>(1));
    ASSERT_EQ(keybind_configure("b", "exit"), cfgst_ok);
    ASSERT_EQ(keybind_configure("c", "exit"), cfgst_ok);
    ASSERT_EQ(KeybindSize(), static_cast<size_t>(3));

    kb = keybind_all();
    ASSERT_EQ(kb->actions[0].type, action_exit);
    ASSERT_EQ(kb->next->actions[0].type, action_exit);
    ASSERT_EQ(kb->next->next->actions[0].type, action_exit);

    ASSERT_EQ(keybind_configure("b", "reload"), cfgst_ok);
    ASSERT_EQ(KeybindSize(), static_cast<size_t>(3));

    kb = keybind_all();
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('b'));
    ASSERT_EQ(kb->actions[0].type, action_reload);
    ASSERT_EQ(kb->next->key, static_cast<xkb_keysym_t>('c'));
    ASSERT_EQ(kb->next->actions[0].type, action_exit);
    ASSERT_EQ(kb->next->next->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->next->next->actions[0].type, action_exit);
}

TEST_F(Keybind, ParseMods)
{
    const struct keybind* kb;

    ASSERT_EQ(keybind_configure("Ctrl+a", "exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, KEYMOD_CTRL);

    ASSERT_EQ(keybind_configure("Alt+a", "exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, KEYMOD_ALT);

    ASSERT_EQ(keybind_configure("Shift+a", "exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, KEYMOD_SHIFT);

    ASSERT_EQ(keybind_configure("Alt+Ctrl+a", "exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, KEYMOD_CTRL | KEYMOD_ALT);

    ASSERT_EQ(keybind_configure("Ctrl+Shift+Alt+a", "exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->key, static_cast<xkb_keysym_t>('a'));
    ASSERT_EQ(kb->mods, KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT);
}

TEST_F(Keybind, ParseParams)
{
    const struct keybind* kb;

    ASSERT_EQ(keybind_configure("a", "exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->num_actions, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions[0].type, action_exit);
    ASSERT_EQ(kb->actions[0].params, nullptr);

    ASSERT_EQ(keybind_configure("a", "exit; "), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->num_actions, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions[0].type, action_exit);
    ASSERT_EQ(kb->actions[0].params, nullptr);

    ASSERT_EQ(keybind_configure("a", "status  \t params 1 2 3\t"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->num_actions, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions[0].type, action_status);
    ASSERT_STREQ(kb->actions[0].params, "params 1 2 3");
}

TEST_F(Keybind, ParseMultiaction)
{
    const struct keybind* kb;

    ASSERT_EQ(keybind_configure("a", "exec cmd;reload; exit"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->num_actions, static_cast<size_t>(3));
    ASSERT_EQ(kb->actions[0].type, action_exec);
    ASSERT_STREQ(kb->actions[0].params, "cmd");
    ASSERT_EQ(kb->actions[1].type, action_reload);
    ASSERT_EQ(kb->actions[1].params, nullptr);
    ASSERT_EQ(kb->actions[2].type, action_exit);
    ASSERT_EQ(kb->actions[2].params, nullptr);

    ASSERT_EQ(keybind_configure("a", "exit;"), cfgst_ok);
    kb = keybind_all();
    ASSERT_EQ(kb->num_actions, static_cast<size_t>(1));
    ASSERT_EQ(kb->actions[0].type, action_exit);
}

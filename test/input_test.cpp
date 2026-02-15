// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "input.hpp"

#include <gtest/gtest.h>

#include <clocale>

TEST(InputKeyboardTest, Load)
{
    std::setlocale(LC_ALL, "");

    std::optional<InputKeyboard> input;

    input = InputKeyboard::load("A");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_NONE);
    EXPECT_EQ(input->key, static_cast<xkb_keysym_t>(XKB_KEY_a));

    input = InputKeyboard::load("Ы");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_NONE);
    EXPECT_EQ(input->key, static_cast<xkb_keysym_t>(XKB_KEY_Cyrillic_YERU));

    input = InputKeyboard::load("Alt-Alt+1");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_ALT);
    EXPECT_EQ(input->key, static_cast<xkb_keysym_t>(XKB_KEY_1));

    input = InputKeyboard::load("Ctrl+Alt-Shift-Escape");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT);
    EXPECT_EQ(input->key, static_cast<xkb_keysym_t>(XKB_KEY_Escape));

    EXPECT_FALSE(InputKeyboard::load("123"));
    EXPECT_FALSE(InputKeyboard::load("AA"));
    EXPECT_FALSE(InputKeyboard::load("Ctrl"));
    EXPECT_FALSE(InputKeyboard::load("Ctrl+Alt"));
    EXPECT_FALSE(InputKeyboard::load("Ctrla+1"));
}

TEST(InputKeyboardTest, ToString)
{
    EXPECT_EQ(InputKeyboard('a', KEYMOD_NONE).to_string(), "a");
    EXPECT_EQ(InputKeyboard('a', KEYMOD_CTRL).to_string(), "Ctrl+a");
    EXPECT_EQ(InputKeyboard('A', KEYMOD_ALT).to_string(), "Alt+a");
    EXPECT_EQ(InputKeyboard('A', KEYMOD_SHIFT).to_string(), "Shift+a");

    EXPECT_EQ(
        InputKeyboard(XKB_KEY_Escape, KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT)
            .to_string(),
        "Ctrl+Alt+Shift+Escape");
}

TEST(InputMouseTest, Load)
{
    std::optional<InputMouse> input;

    input = InputMouse::load("MouseLeft");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_NONE);
    EXPECT_EQ(input->buttons, InputMouse::BUTTON_LEFT);
    EXPECT_EQ(input->x, 0UL);
    EXPECT_EQ(input->y, 0UL);

    input = InputMouse::load("MouseRight+ScrollUp");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_NONE);
    EXPECT_EQ(input->buttons, InputMouse::BUTTON_RIGHT | InputMouse::SCROLL_UP);

    input = InputMouse::load("Alt-MouseLeft");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_ALT);
    EXPECT_EQ(input->buttons, InputMouse::BUTTON_LEFT);

    input = InputMouse::load("Ctrl+Alt-Shift-MouseRight+ScrollUp");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mods, KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT);
    EXPECT_EQ(input->buttons, InputMouse::BUTTON_RIGHT | InputMouse::SCROLL_UP);

    EXPECT_FALSE(InputMouse::load("AA"));
    EXPECT_FALSE(InputMouse::load("Ctrl"));
    EXPECT_FALSE(InputMouse::load("Ctrl+Alt"));
    EXPECT_FALSE(InputMouse::load("Ctrla+1"));
}

TEST(InputMouseTest, ToString)
{
    EXPECT_EQ(
        InputMouse(InputMouse::BUTTON_RIGHT, KEYMOD_NONE, 0, 0).to_string(),
        "MouseRight");
    EXPECT_EQ(
        InputMouse(InputMouse::BUTTON_LEFT, KEYMOD_CTRL, 0, 0).to_string(),
        "Ctrl+MouseLeft");
    EXPECT_EQ(InputMouse(InputMouse::BUTTON_LEFT,
                         KEYMOD_CTRL | KEYMOD_ALT | KEYMOD_SHIFT, 0, 0)
                  .to_string(),
              "Ctrl+Alt+Shift+MouseLeft");
}

TEST(InputSignalTest, Load)
{
    std::optional<InputSignal> input;

    input = InputSignal::load("USR1");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->signal, InputSignal::USR1);

    input = InputSignal::load("USR2");
    ASSERT_TRUE(input);
    EXPECT_EQ(input->signal, InputSignal::USR2);

    EXPECT_FALSE(InputSignal::load("USR123"));
}

TEST(InputSignalTest, ToString)
{
    EXPECT_EQ(InputSignal(InputSignal::USR1).to_string(), "USR1");
    EXPECT_EQ(InputSignal(InputSignal::USR2).to_string(), "USR2");
}

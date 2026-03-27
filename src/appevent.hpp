// SPDX-License-Identifier: MIT
// Applicataion event.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "geometry.hpp"
#include "input.hpp"

#include <filesystem>
#include <functional>
#include <variant>

/** Application event. */
namespace AppEvent {

/** Window close event. */
struct WindowClose {};

/** Window redraw event. */
struct WindowRedraw {};

/** Window resize event. */
struct WindowResize {
    Size size; ///< New size of the window
};

/** Window rescale event. */
struct WindowRescale {
    double scale; ///< New scale factor of the window
};

/** Key press event. */
struct KeyPress {
    InputKeyboard key; ///< Key description
};

/** Mouse clock event. */
struct MouseClick {
    InputMouse mouse; ///< Mouse key state
    Point pointer;    ///< Mouse pointer coordinates within the window
};

/** Mouse move event. */
struct MouseMove {
    InputMouse mouse; ///< Mouse key state
    Point pointer;    ///< Mouse pointer coordinates within the window
    Point delta;      ///< Delta between this and last call
};

/** Gesture: pinch event. */
struct GesturePinch {
    double scale_delta; ///< Scale diff
};

/** Signal occur event. */
struct Signal {
    InputSignal signal; ///< Signal description
};

/** File create event. */
struct FileCreate {
    std::filesystem::path path; ///< Path to the file
};

/** File change event. */
struct FileModify {
    std::filesystem::path path; ///< Path to the file
};

/** File remove event. */
struct FileRemove {
    std::filesystem::path path; ///< Path to the file
};

// clang-format off
using Holder = std::variant<WindowClose,
                            WindowRedraw,
                            WindowResize,
                            WindowRescale,
                            KeyPress,
                            MouseClick,
                            MouseMove,
                            GesturePinch,
                            Signal,
                            FileCreate,
                            FileModify,
                            FileRemove>;
// clang-format on

// event handler
using Handler = std::function<void(const Holder&)>;
};

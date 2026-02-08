// SPDX-License-Identifier: MIT
// Generic application mode interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"
#include "input.hpp"

#include <functional>
#include <map>

class AppMode {
public:
    using InputCallback = std::function<void()>;

    /**
     * Initialze mode instance.
     */
    virtual void initialize() = 0;

    /**
     * Activate mode.
     * @param image image to show/select
     */
    virtual void activate(ImagePtr image) = 0;

    /**
     * Deactivate mode.
     */
    virtual void deactivate() = 0;

    /**
     * Reset state.
     */
    virtual void reset() = 0;

    /**
     * Get currently showed/selected image.
     * @return current image
     */
    virtual ImagePtr current_image() = 0;

    /**
     * Window resize handler.
     */
    virtual void window_resize() = 0;

    /**
     * Window resize handler.
     * @param wnd window surface pixmap
     */
    virtual void window_redraw(Pixmap& wnd) = 0;

    /**
     * Handle key press event.
     * @param input input event description
     * @return false if event not supported
     */
    virtual bool handle_keyboard(const InputKeyboard& input);

    /**
     * Handle mouse click.
     * @param input input event description
     * @return false if event not supported
     */
    virtual bool handle_mclick(const InputMouse& input);

    /**
     * Handle mouse move.
     * @param input input event description
     */
    virtual void handle_mmove(const InputMouse& input);

    /**
     * Handle signal.
     * @param input input event description
     * @return false if event not supported
     */
    virtual bool handle_signal(const InputSignal& input);

    /**
     * Remove all bindings: keyboard/mouse/signals.
     */
    void bind_reset();

    /**
     * Bind key press event.
     * @param input event description
     * @param handler callback
     */
    void bind_input(const InputKeyboard& input, const InputCallback& handler);

    /**
     * Bind mouse click event.
     * @param input event description
     * @param handler callback
     */
    void bind_input(const InputMouse& input, const InputCallback& handler);

    /**
     * Bind signal event.
     * @param input event description
     * @param handler callback
     */
    void bind_input(const InputSignal& input, const InputCallback& handler);

private:
    std::map<InputKeyboard, InputCallback> kbindings; ///< Keyboard bindings
    std::map<InputMouse, InputCallback> mbindings;    ///< Mouse bindings
    std::map<InputSignal, InputCallback> sbindings;   ///< Signal bindings
};

// SPDX-License-Identifier: MIT
// Generic application mode interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"
#include "input.hpp"
#include "text.hpp"

#include <functional>
#include <map>

class AppMode {
public:
    /** Image list event types. */
    enum class ImageListEvent : uint8_t {
        Create,
        Modify,
        Remove,
    };

    using InputCallback = std::function<void()>;

    /** External handler called when switching an image. */
    using ImageSwitchCallback = std::function<void()>;
    /** External handler called when main window resizing. */
    using WindowResizeCallback = std::function<void()>;

    AppMode();

    /**
     * Initialize mode instance.
     */
    virtual void initialize() = 0;

    /**
     * Activate mode.
     * @param entry image entry to show/select
     * @param wnd window size
     */
    virtual void activate(const ImageEntryPtr& entry, const Size& wnd) = 0;

    /**
     * Deactivate mode.
     */
    virtual void deactivate() = 0;

    /**
     * Get currently showed/selected image entry.
     * @return current image entry
     */
    virtual ImageEntryPtr current_entry() = 0;

    /**
     * Window resize handler.
     * @param wnd window size
     */
    virtual void window_resize(const Size& wnd);

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
     * @param pos mouse pointer coordinates
     * @return false if event not supported
     */
    virtual bool handle_mclick(const InputMouse& input, const Point& pos);

    /**
     * Handle mouse move.
     * @param input input event description
     * @param pos mouse pointer coordinates
     * @param delta diff between this and last call
     */
    virtual void handle_mmove(const InputMouse& input, const Point& pos,
                              const Point& delta) = 0;

    /**
     * Handle pinch gesture.
     * @param scale_delta scale diff
     */
    virtual void handle_pinch(const double scale_delta) = 0;

    /**
     * Handle signal.
     * @param input input event description
     * @return false if event not supported
     */
    virtual bool handle_signal(const InputSignal& input);

    /**
     * Handle image list changes.
     * @param event event type
     * @param entry updated image entry
     */
    virtual void handle_imagelist(const ImageListEvent event,
                                  const ImageEntryPtr& entry) = 0;

    /**
     * Check if current mode is active.
     * @return true if current mode is active
     */
    bool is_active() const;

    /**
     * Subscribe to the image switch event.
     * @param cb event handler
     */
    void subscribe_image_switch(const ImageSwitchCallback& cb);

    /**
     * Subscribe to window resize event.
     * @param cb event handler
     */
    void subscribe_window_resize(const WindowResizeCallback& cb);

    /**
     * Set mark icon color.
     * @param color mark icon color
     */
    void set_mark_color(const argb_t& color);

    /**
     * Set text layer scheme.
     * @param pos block position
     * @param scheme scheme description
     */
    void set_text_scheme(const Text::Position pos,
                         const std::vector<std::string>& scheme);

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

protected:
    /**
     * Switch to another image.
     */
    void switch_current();

protected:
    argb_t mark_color;                       ///< Mark icon color
    std::vector<std::string> text_scheme[4]; ///< Text layer scheme

private:
    std::vector<ImageSwitchCallback> imswitch_cb; ///< Image switch callbacks
    std::vector<WindowResizeCallback> wndrsz_cb;  ///< Window resize callbacks
    std::map<InputKeyboard, InputCallback> kbindings; ///< Keyboard bindings
    std::map<InputMouse, InputCallback> mbindings;    ///< Mouse bindings
    std::map<InputSignal, InputCallback> sbindings;   ///< Signal bindings
};

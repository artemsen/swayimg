// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appevent.hpp"
#include "appmode.hpp"
#include "fdevent.hpp"
#include "ui.hpp"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

class Application {
public:
    /** Application mode types. */
    enum class Mode : uint8_t {
        Viewer,
        Slideshow,
        Gallery,
    };

    /** File descriptor event handler. */
    using FdEventHandler = std::function<void()>;
    /** Notification handler that is called when the window is resized. */
    using WindowResizeNotify = std::function<void()>;

    /**
     * Get global instance of the application.
     * @return application instance
     */
    static Application& self();

    /**
     * Get UI instance.
     * @return UI instance
     */
    static inline Ui* get_ui() { return self().ui.get(); }

    /**
     * Redraw app window.
     */
    static inline void redraw() { self().add_event(AppEvent::WindowRedraw {}); }

    Application();

    /**
     * Run the application.
     * @return exit code
     */
    int run();

    /**
     * Exit from application.
     * @param rc result code to set
     */
    void exit(int rc);

    /**
     * Switch mode (viewer/slideshow/gallery).
     * @param mode mode to set
     */
    void set_mode(Mode mode);

    /**
     * Switch mode (viewer/slideshow/gallery).
     * @param mode mode to set
     */
    [[nodiscard]] Mode get_mode() const { return active_mode; }

    /**
     * Get currently active application mode instance.
     * @return application mode instance
     */
    AppMode* current_mode();

    /**
     * Add entry to image list.
     * @param path image path
     * @return added entry (first one for directory)
     */
    ImageEntryPtr add_image_entry(const std::filesystem::path& path);

    /**
     * Remove entry from image list.
     * @param path image path
     */
    void remove_image_entry(const std::filesystem::path& path);

    /**
     * Add file descriptor to monitor.
     * @param fd file descriptor to watch
     * @param handler callback
     */
    void add_fdpoll(int fd, const FdEventHandler& handler);

    /**
     * Add application event to the queue.
     * @param event application event to add
     */
    void add_event(const AppEvent::Holder& event);

    /**
     * Subscribe to window resize event.
     * @param cb event handler
     */
    void subscribe_window_resize(const WindowResizeNotify& cb);

private:
    /**
     * Initialize image list.
     * @return first image entry to open, nullptr on errors
     */
    ImageEntryPtr il_initialize();

    /**
     * Initialize UI subsystem.
     * @return true if UI initialized successfully
     */
    bool ui_initialize();

    /**
     * Application event loop handler.
     */
    void event_loop();

    // Application event handlers
    void handle_event(const AppEvent::Holder& event);
    void handle_event(const AppEvent::WindowClose& event);
    void handle_event(const AppEvent::WindowResize& event);
    void handle_event(const AppEvent::WindowRescale& event);
    void handle_event(const AppEvent::WindowRedraw& event);
    void handle_event(const AppEvent::KeyPress& event);
    void handle_event(const AppEvent::MouseClick& event);
    void handle_event(const AppEvent::MouseMove& event);
    void handle_event(const AppEvent::GesturePinch& event);
    void handle_event(const AppEvent::Signal& event);
    void handle_event(const AppEvent::FileCreate& event);
    void handle_event(const AppEvent::FileModify& event);
    void handle_event(const AppEvent::FileRemove& event);

public:
    /** Application startup parameters. */
    struct StartupParams {
        std::filesystem::path config;    ///< Lua config file to load
        std::string lua_script;          ///< Lua script to start
        std::filesystem::path from_file; ///< Load image list from file
        std::vector<std::filesystem::path> sources; ///< Image list
        std::optional<Mode> mode;                   ///< Initial mode
        InputMouse dnd { InputMouse::BUTTON_RIGHT,
                         KEYMOD_NONE }; ///< Mouse used for drag-and-drop
        Rectangle window;               ///< Main window position/size
        bool use_overlay = false;       ///< Use overlay mode (Wayland only)
        std::optional<bool> fullscreen; ///< Full screen mode (Wayland only)
        bool decoration = true;         ///< Window decoration (Wayland only)
        uint32_t cursor_hide = 3000;    ///< Cursor hide time (Wayland only)
        std::string app_id = "swayimg"; ///< Window class name (Wayland only)
        size_t drm_freq = 0;            ///< Display frequency (DRM only)
    } sparams;

    /** Initialization complete callback. */
    std::function<void()> on_init_complete = nullptr;

    std::vector<WindowResizeNotify> wnd_resize_cb; ///< Window resize callbacks

private:
    std::unique_ptr<Ui> ui; ///< UI instance

    bool initialized = false; ///< Initialization complete flag

    Mode active_mode = Mode::Viewer; ///< Currently active mode

    std::atomic<bool> stop_flag = false; ///< Application stop flag
    int exit_code = -1;                  ///< Application exit code
    FdEvent exit_event;                  ///< Application stop event

    std::vector<std::pair<int, FdEventHandler>>
        fds; ///< Monitored file descriptors

    std::deque<AppEvent::Holder> event_queue; ///< Event queue
    std::mutex event_mutex;                   ///< Event queue mutex
    FdEvent event_notify;                     ///< Event queue notification
};

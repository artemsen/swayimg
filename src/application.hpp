// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appevent.hpp"
#include "appmode.hpp"
#include "fdevent.hpp"
#include "sparam.hpp"
#include "ui.hpp"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class Application {
public:
    /** File descriptor event handler. */
    using FdEventHandler = std::function<void()>;

    /**
     * Get global instance of the application.
     * @return application instance
     */
    static Application& self();

    /**
     * Get UI instance.
     * @return UI instance
     */
    static Ui* get_ui() { return self().ui.get(); }

    /**
     * Redraw app window.
     */
    static void redraw() { self().add_event(AppEvent::WindowRedraw {}); }

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
    void exit(const int rc);

    /**
     * Switch mode (viewer/slideshow/gallery).
     * @param mode mode to set
     */
    void set_mode(const AppMode::Type mode);

    /**
     * Get currently active mode (viewer/slideshow/gallery).
     * @param mode mode to set
     */
    [[nodiscard]] AppMode::Type get_mode() const { return active_mode; }

    /**
     * Get currently active application mode instance.
     * @return application mode instance
     */
    AppMode* current_mode();

    /**
     * Add entries to the image list.
     * @param paths list of image paths to add
     * @return first added entry
     */
    ImageEntryPtr add_images(const std::vector<std::filesystem::path>& paths);

    /**
     * Remove entries from the image list.
     * @param paths list of image paths to remove
     */
    void remove_images(const std::vector<std::filesystem::path>& paths);

    /**
     * Remove all entries from the image list.
     */
    void remove_all_images();

    /**
     * Add file descriptor to monitor.
     * @param fd file descriptor to watch
     * @param handler callback
     */
    void add_fdpoll(const int fd, const FdEventHandler& handler);

    /**
     * Add application event to the queue.
     * @param event application event to add
     */
    void add_event(const AppEvent::Holder& event);

    /**
     * Check if all subsystems were initialized.
     * @return true if all subsystems were initialized
     */
    [[nodiscard]] bool initialized() const { return !sparams; }

    /**
     * Get application id.
     * @return application id
     */
    [[nodiscard]] const std::string& get_appid() const;

private:
    /**
     * Initialize image list.
     * @return first image entry to open, nullptr on errors
     */
    [[nodiscard]] ImageEntryPtr il_initialize() const;

    /**
     * Initialize Wayland UI.
     * @return pointer to UI interface if initialized successfully
     */
    [[nodiscard]] Ui* ui_init_wayland() const;

    /**
     * Initialize DRM UI.
     * @return pointer to UI interface if initialized successfully
     */
    [[nodiscard]] Ui* ui_init_drm() const;

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
    void handle_event(const AppEvent::DragAndDrop& event);
    void handle_event(const AppEvent::FileCreate& event);
    void handle_event(const AppEvent::FileModify& event);
    void handle_event(const AppEvent::FileRemove& event);

    // Signal handler, see std::signal for details
    static void signal_handler(int signal);

public:
    std::unique_ptr<StartupParams> sparams; ///< Startup parameters
    std::function<void()> on_init_complete; ///< Init complete callback
    std::function<void()> on_wnd_resize;    ///< Window resize callback

private:
    std::unique_ptr<Ui> ui; ///< UI instance

    std::string app_id;                          ///< Application id
    AppMode::Type active_mode = AppMode::Viewer; ///< Currently active mode

    std::atomic<bool> stop_flag = false; ///< Application stop flag
    int exit_code = -1;                  ///< Application exit code
    FdEvent exit_event;                  ///< Application stop event
    FdEvent signal_fds[2];               ///< Signal notifiers (USR1/USR2)

    std::vector<std::pair<int, FdEventHandler>>
        fds; ///< Monitored file descriptors

    std::deque<AppEvent::Holder> event_queue; ///< Event queue
    std::mutex event_mutex;                   ///< Event queue mutex
    FdEvent event_notify;                     ///< Event queue notification
};

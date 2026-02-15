// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"
#include "font.hpp"
#include "gallery.hpp"
#include "imagelist.hpp"
#include "luaengine.hpp"
#include "slideshow.hpp"
#include "text.hpp"
#include "ui.hpp"
#include "viewer.hpp"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
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

    /** Application startup parameters. */
    struct StartupParams {
        std::filesystem::path config;
        std::filesystem::path from_file;
        std::vector<std::filesystem::path> sources;
        Mode mode = Application::Mode::Viewer;
        Rectangle window;
        bool fullscreen = false;
        std::string app_id = "swayimg";
    };

    /**
     * Get global instance of the application.
     * @return application instance
     */
    static Application& self();

    // Getters for app subsystems
    static Ui* get_ui();
    static ImageList& get_imagelist();
    static Viewer& get_viewer();
    static Slideshow& get_slideshow();
    static Gallery& get_gallery();
    static Font& get_font();
    static Text& get_text();

    /**
     * Run the application.
     * @param params startup parameters
     * @return exit code
     */
    int run(const StartupParams& params);

    /**
     * Exit from application.
     * @param rc result code to set
     */
    void exit(int rc);

    /**
     * Switch mode (viewer/slideshow/gallery).
     * @param next mode to set
     */
    void switch_mode(Mode next);

    /**
     * Redraw app window.
     */
    void redraw();

    /**
     * Add file descriptor to monitor.
     * @param fd file descriptor to watch
     * @param handler callback
     */
    void add_fdpoll(int fd, const FdEventHandler& handler);

private:
    /**
     * Get current application mode instance.
     * @return application mode instance
     */
    AppMode* appmode();

    /**
     * Initialize image list.
     * @param params startup parameters
     * @return first image entry to open, nullptr on errors
     */
    ImageList::EntryPtr il_initialize(const StartupParams& params);

    bool ui_initialize(const Rectangle& wnd, const std::string& app_id);
    void ui_append_event(Ui::Event event);
    void ui_handle_event(Ui::Event event);

    void list_change(const FsMonitor::Event event,
                     const ImageList::EntryPtr& entry);

private:
    std::unique_ptr<Ui> ui;
    ImageList image_list; ///< Image list
    Font font;            ///< Font instance
    Text text;            ///< Text layer

    Viewer viewer;            ///< Viewer mode instance
    Slideshow slideshow;      ///< Slideshow mode instance
    Gallery gallery;          ///< Gallery mode instance
    Mode mode = Mode::Viewer; ///< Currently active mode

    LuaEngine lua; ///< Lua engine

    int exit_code = -1; ///< Application exit code
    FdEvent exit_event; ///< Application stop event

    std::vector<std::pair<int, FdEventHandler>>
        fds; ///< Monitored file descriptors

    std::deque<Ui::Event> ui_events; ///< Event queue
    std::mutex ui_mutex;             ///< Event queue mutex
    FdEvent ui_notify;               ///< Event queue notification
};

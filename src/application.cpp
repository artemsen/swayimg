// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"

#include "buildconf.hpp"
#include "fsmonitor.hpp"
#include "gallery.hpp"
#include "imagelist.hpp"
#include "log.hpp"
#include "luaengine.hpp"
#include "slideshow.hpp"
#include "text.hpp"
#include "viewer.hpp"

#ifdef HAVE_WAYLAND
#include "ui_wayland.hpp"
#ifdef HAVE_COMPOSITOR
#include "compositor.hpp"
#endif // HAVE_COMPOSITOR
#endif // HAVE_WAYLAND
#ifdef HAVE_DRM
#include "ui_drm.hpp"
#endif // HAVE_DRM

#include <poll.h>

#include <csignal>

Application& Application::self()
{
    static Application singleton;
    return singleton;
}

Application::Application()
{
#ifdef HAVE_COMPOSITOR
    // defaults for people with Sway compositor
    const Compositor compositor;
    if (compositor.type == Compositor::Sway) {
        sparams.use_overlay = true;
        sparams.decoration = false;
    }
#endif // HAVE_COMPOSITOR
}

int Application::run()
{
    // initialize and load Lua
    LuaEngine& lua = LuaEngine::self();
    lua.initialize(sparams.config);
    if (!sparams.lua_script.empty()) {
        lua.execute(sparams.lua_script);
    }

    // initialize filemon and image list
    FsMonitor::self().initialize();
    const ImageEntryPtr first_entry = il_initialize();
    if (!first_entry) {
        Log::info("Image list is empty, exit");
        return 0;
    }

    // initialize UI
    ui.reset(ui_init_wayland());
    if (!ui) {
        ui.reset(ui_init_drm());
    }
    if (!ui) {
        return 1;
    }

    // set signal handlers
    std::signal(SIGUSR1, &Application::signal_handler);
    std::signal(SIGUSR2, &Application::signal_handler);

    // initialize other subsystems
    Text::self().initialize();
    Viewer::self().initialize();
    Slideshow::self().initialize();
    Gallery::self().initialize();

    initialized = true;
    active_mode = sparams.mode.value_or(Mode::Viewer);

    current_mode()->activate(first_entry, ui->get_window_size());
    ui->run();

    if (on_init_complete) {
        on_init_complete();
    }

    event_loop();
    current_mode()->deactivate();
    ui->stop();

    return exit_code;
}

void Application::exit(const int rc)
{
    stop_flag = true;
    exit_code = rc;
    exit_event.set();
}

void Application::set_mode(const Mode mode)
{
    if (!initialized) {
        if (!sparams.mode.has_value()) {
            sparams.mode = mode;
        }
        return; // not yet initialized
    }

    if (mode != active_mode) {
        AppMode* app_mode;

        app_mode = current_mode();
        const ImageEntryPtr entry = app_mode->get_current();
        app_mode->deactivate();

        active_mode = mode;
        app_mode = current_mode();
        app_mode->activate(entry, ui->get_window_size());
    }
}

AppMode* Application::current_mode()
{
    if (!initialized) {
        return nullptr; // not yet initialized
    }

    switch (active_mode) {
        case Mode::Viewer:
            return &Viewer::self();
        case Mode::Slideshow:
            return &Slideshow::self();
        case Mode::Gallery:
            return &Gallery::self();
    };

    assert(false && "unreachable");
    return nullptr;
}

ImageEntryPtr
Application::add_images(const std::vector<std::filesystem::path>& paths)
{
    ImageList& il = ImageList::self();

    std::list<ImageEntryPtr> entries;
    for (const auto& path : paths) {
        entries.splice(entries.end(), il.add(path));
    }

    if (!entries.empty()) {
        current_mode()->handle_imagelist(AppMode::ImageListEvent::Create,
                                         entries);
        redraw();
        return entries.front();
    }

    return nullptr;
}

void Application::remove_images(const std::vector<std::filesystem::path>& paths)
{
    ImageList& il = ImageList::self();

    std::list<ImageEntryPtr> entries;
    for (const auto& path : paths) {
        const ImageEntryPtr entry = il.find(path);
        if (entry) {
            entries.push_back(entry);
            il.remove(entry);
        }
    }

    if (!entries.empty()) {
        current_mode()->handle_imagelist(AppMode::ImageListEvent::Remove,
                                         entries);
        redraw();
    }
}

void Application::add_fdpoll(const int fd, const FdEventHandler& handler)
{
    fds.emplace_back(fd, handler);
}

void Application::add_event(const AppEvent::Holder& event)
{
    const std::scoped_lock lock(event_mutex);

    // check if redraw event already exists
    const bool has_redraw = !event_queue.empty() &&
        std::holds_alternative<AppEvent::WindowRedraw>(event_queue.back());
    if (has_redraw && std::holds_alternative<AppEvent::WindowRedraw>(event)) {
        return;
    }

    // append event to queue, but preserve redraw at last position
    auto pos = event_queue.end();
    if (has_redraw) {
        --pos;
    }
    event_queue.insert(pos, event);

    event_notify.set();
}

void Application::subscribe_window_resize(const WindowResizeNotify& cb)
{
    wnd_resize_cb.push_back(cb);
}

ImageEntryPtr Application::il_initialize()
{
    ImageList& il = ImageList::self();

    ImageEntryPtr first_entry = nullptr;

    const Log::PerfTimer timer;

    if (!sparams.from_file.empty()) {
        first_entry = il.load(sparams.from_file);
    }
    if (!sparams.sources.empty()) {
        if (sparams.sources.size() == 1 && sparams.sources[0] == "-") {
            first_entry = il.add(std::vector<std::filesystem::path> {
                                     ImageEntry::SRC_STDIN })
                              .front();
        } else {
            first_entry = il.add(sparams.sources).front();
        }
    }
    if (sparams.from_file.empty() && sparams.sources.empty()) {
        first_entry =
            il.add(std::vector<std::filesystem::path> { "." }).front();
    }

    if (Log::verbose_enable()) {
        Log::verbose("Image list loaded in {:.6f} sec", timer.time());
    }

    return first_entry;
}

Ui* Application::ui_init_wayland()
{
#ifdef HAVE_WAYLAND
    if (!sparams.app_id.has_value()) {
        sparams.app_id = "swayimg";
    }

#ifdef HAVE_COMPOSITOR
    if (sparams.use_overlay || sparams.window.position_valid()) {
        const Compositor compositor;
        if (compositor.type == Compositor::None) {
            Log::error("Current compositor not supported for managing window "
                       "position");
        } else {
            const Rectangle focused = compositor.get_focus();
            if (focused) {
                if (!sparams.window.position_valid()) {
                    sparams.window.x = focused.x;
                    sparams.window.y = focused.y;
                }
                if (!sparams.window.size_valid()) {
                    sparams.window.width = focused.width;
                    sparams.window.height = focused.height;
                }
            }
            std::string app_id = sparams.app_id.value();
            compositor.set_overlay(sparams.window, app_id);
            sparams.app_id = app_id;
        }
    }
#endif // HAVE_COMPOSITOR

    UiWayland* wayland = new UiWayland();
    if (sparams.window.size_valid()) {
        wayland->width = sparams.window.width;
        wayland->height = sparams.window.height;
    }
    wayland->dnd = sparams.dnd;
    wayland->cursor_hide = sparams.cursor_hide;
    wayland->decoration = sparams.decoration;
    if (sparams.fullscreen.has_value()) {
        wayland->fullscreen = sparams.fullscreen.value();
    }
    if (!wayland->initialize(sparams.app_id.value_or("swayimg"))) {
        delete wayland;
        wayland = nullptr;
    }
    return wayland;

#else
    return nullptr;
#endif // HAVE_WAYLAND
}

Ui* Application::ui_init_drm() const
{
#ifdef HAVE_DRM
    UiDrm* drm = new UiDrm();
    if (sparams.window.size_valid()) {
        drm->width = sparams.window.width;
        drm->height = sparams.window.height;
    }
    drm->freq = sparams.drm_freq;
    if (!drm->initialize()) {
        delete drm;
        drm = nullptr;
    }
    return drm;
#else
    return nullptr;
#endif // HAVE_DRM
}

void Application::event_loop()
{
    // register app event handler
    add_fdpoll(event_notify, [this]() {
        AppEvent::Holder event;
        {
            const std::scoped_lock lock(event_mutex);
            assert(!event_queue.empty());
            event = event_queue.front();
            event_queue.pop_front();
            if (event_queue.empty()) {
                event_notify.reset();
            }
        }

        handle_event(event);
    });

    // register signal handlers
    add_fdpoll(signal_fds[0], [this]() {
        signal_fds[0].reset();
        Application::self().add_event(AppEvent::Signal { InputSignal::USR1 });
    });
    add_fdpoll(signal_fds[1], [this]() {
        signal_fds[1].reset();
        Application::self().add_event(AppEvent::Signal { InputSignal::USR2 });
    });

    // register exit handler
    add_fdpoll(exit_event, [this]() {
        stop_flag = true;
    });

    // create fd array to poll
    std::vector<pollfd> poll_fds;
    poll_fds.reserve(fds.size());
    for (auto& it : fds) {
        poll_fds.push_back({
            .fd = it.first,
            .events = POLLIN,
            .revents = 0,
        });
    }

    // main loop: handle events
    while (!stop_flag) {
        if (poll(poll_fds.data(), poll_fds.size(), -1) < 0 && errno != EINTR) {
            exit_code = errno;
            Log::error(errno, "Failed to poll events");
            break;
        }
        // call handlers for each active event
        for (size_t i = 0; !stop_flag && i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                fds[i].second();
            }
        }
    }
}

void Application::handle_event(const AppEvent::Holder& event)
{
    std::visit(
        [this](const auto& event) {
            if constexpr (std::is_same_v<decltype(event),
                                         const AppEvent::WindowClose&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<
                                     decltype(event),
                                     const AppEvent::WindowResize&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<
                                     decltype(event),
                                     const AppEvent::WindowRescale&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<
                                     decltype(event),
                                     const AppEvent::WindowRedraw&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::KeyPress&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::MouseClick&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::MouseMove&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<
                                     decltype(event),
                                     const AppEvent::GesturePinch&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::Signal&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::DragAndDrop&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::FileCreate&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::FileModify&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::FileRemove&>) {
                handle_event(event);
            } else {
                assert(false && "unhnadled event type");
                handle_event(event);
            }
        },
        event);
}

void Application::handle_event(const AppEvent::WindowClose&)
{
    exit(0);
}

void Application::handle_event(const AppEvent::WindowResize& event)
{
    current_mode()->window_resize(event.size);
    redraw();
    for (auto& it : wnd_resize_cb) {
        it();
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Application::handle_event(const AppEvent::WindowRescale& event)
{
    Text::self().set_scale(event.scale);
    redraw();
}

void Application::handle_event(const AppEvent::WindowRedraw&)
{
    Pixmap* wnd = ui->lock_surface();
    if (wnd) {
        const Log::PerfTimer timer;

        current_mode()->window_redraw(*wnd);
        Text::self().draw(*wnd);
        ui->commit_surface();

        if (Log::verbose_enable()) {
            Log::verbose("Redraw in {:.6f} sec", timer.time());
        }
    }
}

void Application::handle_event(const AppEvent::KeyPress& event)
{
    if (!current_mode()->handle_keyboard(event.key) &&
        !Xkb::is_modifier(event.key.key)) {
        const std::string msg =
            std::format("Unhandled key: {}", event.key.to_string());
        Text::self().set_status(msg);
    }
}

void Application::handle_event(const AppEvent::MouseClick& event)
{
    if (!current_mode()->handle_mclick(event.mouse, event.pointer)) {
        const std::string msg =
            std::format("Unhandled mouse: {}", event.mouse.to_string());
        Text::self().set_status(msg);
    }
}

void Application::handle_event(const AppEvent::MouseMove& event)
{
    current_mode()->handle_mmove(event.mouse, event.pointer, event.delta);
}

void Application::handle_event(const AppEvent::GesturePinch& event)
{
    current_mode()->handle_pinch(event.scale_delta);
}

void Application::handle_event(const AppEvent::Signal& event)
{
    if (!current_mode()->handle_signal(event.signal)) {
        const std::string msg =
            std::format("Unhandled signal {}", event.signal.to_string());
        Text::self().set_status(msg);
    }
}

void Application::handle_event(const AppEvent::DragAndDrop& event)
{
    const ImageEntryPtr first = add_images(event.paths);
    if (first) {
        current_mode()->set_current(first);
    }
}

void Application::handle_event(const AppEvent::FileCreate& event)
{
    const ImageList& il = ImageList::self();
    if (!std::filesystem::is_directory(event.path) || il.recursive) {
        add_images({ event.path });
    }
}

void Application::handle_event(const AppEvent::FileModify& event)
{
    if (!std::filesystem::is_directory(event.path)) {
        ImageList& il = ImageList::self();
        const ImageEntryPtr entry = il.find(event.path);
        if (entry) {
            current_mode()->handle_imagelist(AppMode::ImageListEvent::Modify,
                                             { entry });
        }
    }
}

void Application::handle_event(const AppEvent::FileRemove& event)
{
    // ignore directories, files will be removed as standalone event
    if (!std::filesystem::is_directory(event.path)) {
        remove_images({ event.path });
    }
}

void Application::signal_handler(int signal)
{
    switch (signal) {
        case SIGUSR1:
            Application::self().signal_fds[0].set();
            break;
        case SIGUSR2:
            Application::self().signal_fds[1].set();
            break;
    }
}

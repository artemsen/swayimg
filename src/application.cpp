// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"

#include "buildconf.hpp"
#include "defaults.hpp"
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
    : sparams(new StartupParams())
{
    sparams->app_id = Defaults::app::app_id;
    sparams->mode = Defaults::app::mode;
    sparams->fullscreen = Defaults::app::fullscreen;
    sparams->dnd = Defaults::app::dnd;
    sparams->use_overlay = Defaults::app::use_overlay;
    sparams->decoration = Defaults::app::decoration;
    sparams->cursor_hide = Defaults::app::cursor_hide;

#ifdef HAVE_COMPOSITOR
    // defaults for people with Sway compositor
    const Compositor compositor;
    if (compositor.type == Compositor::Sway) {
        sparams->use_overlay = true;
        sparams->decoration = false;
    }
#endif // HAVE_COMPOSITOR
}

int Application::run()
{
    // initialize and load Lua
    LuaEngine& lua = LuaEngine::self();
    lua.initialize(sparams->config);
    if (!sparams->lua_exec.empty()) {
        lua.execute(sparams->lua_exec);
    }

    // initialize filemon and image list
    FsMonitor::self().initialize();
    const ImageEntryPtr first_entry = il_initialize();
    if (!first_entry && active_mode != AppMode::Gallery) {
        Log::warning("Image list is empty, exit");
        return 1;
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

    active_mode = sparams->mode;
    app_id = sparams->app_id;
    sparams.reset();

    current_mode()->activate(first_entry, ui->get_window_size());
    if (active_mode != AppMode::Gallery && !current_mode()->get_current()) {
        Log::warning("Failed to open any images, exit");
        return 1;
    }
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

void Application::set_mode(const AppMode::Type mode)
{
    if (!initialized()) {
        sparams->mode = mode;
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
    if (!initialized()) {
        return nullptr; // not yet initialized
    }

    switch (active_mode) {
        case AppMode::Viewer:
            return &Viewer::self();
        case AppMode::Slideshow:
            return &Slideshow::self();
        case AppMode::Gallery:
            return &Gallery::self();
    };

    assert(false && "unreachable");
    return nullptr;
}

ImageEntryPtr
Application::add_images(const std::vector<std::filesystem::path>& paths)
{
    ImageList& il = ImageList::self();

    const std::vector<ImageEntryPtr> entries = il.add(paths);

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
    const std::vector<ImageEntryPtr> entries = ImageList::self().remove(paths);
    if (!entries.empty()) {
        current_mode()->handle_imagelist(AppMode::ImageListEvent::Remove,
                                         entries);
        redraw();
    }
}

void Application::remove_all_images()
{
    const std::vector<ImageEntryPtr> entries = ImageList::self().clear();
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

const std::string& Application::get_appid() const
{
    if (sparams) {
        return sparams->app_id;
    }
    return app_id;
}

ImageEntryPtr Application::il_initialize() const
{
    assert(!sparams->sources.empty());

    ImageList& il = ImageList::self();
    std::vector<ImageEntryPtr> added;

    const Log::PerfTimer timer;

    if (sparams->sources.size() == 1 && sparams->sources[0] == "-") {
        added = il.add({ ImageEntry::SRC_STDIN });
    } else {
        added = il.add(sparams->sources);
    }

    if (Log::verbose_enable()) {
        Log::verbose("Image list loaded in {:.6f} sec", timer.time());
    }

    // disable loading of adjacent files, otherwise fs mon will add unnecessary
    // files to the list on every call after startup
    il.adjacent = false;

    return added.empty() ? nullptr : added.front();
}

Ui* Application::ui_init_wayland() const
{
#ifdef HAVE_WAYLAND
    Rectangle window(sparams->wnd_pos.get().x, sparams->wnd_pos.get().y,
                     sparams->wnd_size.get().width,
                     sparams->wnd_size.get().height);
#ifdef HAVE_COMPOSITOR
    if (sparams->use_overlay || sparams->wnd_pos.get()) {
        const Compositor compositor;
        if (compositor.type == Compositor::None) {
            Log::error("Current compositor not supported for managing window "
                       "position");
        } else if (!window) {
            const Rectangle focused = compositor.get_focus();
            if (focused) {
                if (!window.position_valid()) {
                    window.x = focused.x;
                    window.y = focused.y;
                }
                if (!window.size_valid()) {
                    window.width = focused.width;
                    window.height = focused.height;
                }
            }
        }
        compositor.set_overlay(window, sparams->app_id.unlock());
    }
#endif // HAVE_COMPOSITOR

    UiWayland* wayland = new UiWayland();
    if (window.size_valid()) {
        wayland->width = window.width;
        wayland->height = window.height;
    } else {
        wayland->width = Defaults::app::wnd_width;
        wayland->height = Defaults::app::wnd_height;
    }
    wayland->dnd = sparams->dnd;
    wayland->cursor_hide = sparams->cursor_hide;
    wayland->decoration = sparams->decoration;
    wayland->fullscreen = sparams->fullscreen;
    if (!wayland->initialize(sparams->app_id)) {
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
    if (sparams->wnd_size.get()) {
        drm->width = sparams->wnd_size.get().width;
        drm->height = sparams->wnd_size.get().height;
    }
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
    // NOLINTBEGIN(bugprone-branch-clone)
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
    // NOLINTEND(bugprone-branch-clone)
}

void Application::handle_event(const AppEvent::WindowClose&)
{
    exit(0);
}

void Application::handle_event(const AppEvent::WindowResize& event)
{
    current_mode()->window_resize(event.size);
    if (on_wnd_resize) {
        on_wnd_resize();
    }
    redraw();
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

        if (on_redraw_complete) {
            on_redraw_complete();
        }

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
    remove_all_images();
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
        default:
            break;
    }
}

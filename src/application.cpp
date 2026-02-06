// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"

#include <poll.h>

#include "buildconf.hpp"
#include "image.hpp"
#include "log.hpp"
#include "text.hpp"
#include "ui_wayland.hpp"

#ifdef HAVE_COMPOSITOR
#include "compositor.hpp"
#endif

Application& Application::self()
{
    static Application singleton;
    return singleton;
}

Ui* Application::get_ui()
{
    return Application::self().ui.get();
}

ImageList& Application::get_imagelist()
{
    return Application::self().image_list;
}

Viewer& Application::get_viewer()
{
    return Application::self().viewer;
}

Slideshow& Application::get_slideshow()
{
    return Application::self().slideshow;
}

Gallery& Application::get_gallery()
{
    return Application::self().gallery;
}

Font& Application::get_font()
{
    return Application::self().font;
}

Text& Application::get_text()
{
    return Application::self().text;
}

int Application::run(const StartupParams& params)
{
    image_list.initialize(std::bind(&Application::list_change, this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));

    if (!params.from_file.empty()) {
        image_list.load(params.from_file);
    }
    if (!params.sources.empty()) {
        image_list.load(params.sources);
    }
    if (params.from_file.empty() && params.sources.empty()) {
        image_list.add(".");
    }

    if (!ui_initialize(params.window, params.app_id)) {
        return 1;
    }

    lua.initialize();
    font.initialize();
    viewer.initialize();
    slideshow.initialize();
    gallery.initialize();

    // register ui event handler
    add_fdpoll(ui_notify, [this]() {
        std::unique_lock lock(ui_mutex);
        assert(!ui_events.empty());
        Ui::Event event = ui_events.front();
        ui_events.pop_front();
        if (ui_events.empty()) {
            ui_notify.reset();
        }
        lock.unlock();
        ui_handle_event(event);
    });

    // register exit handler
    bool stop = false;
    add_fdpoll(exit_event, [&stop]() {
        stop = true;
    });

    // run ui
    appmode()->activate(nullptr);
    ui->run();

    // create fd array to poll
    std::vector<pollfd> poll_fds;
    poll_fds.reserve(fds.size());
    for (auto& it : fds) {
        pollfd pfd = {
            .fd = it.first,
            .events = POLLIN,
            .revents = 0,
        };
        poll_fds.push_back(pfd);
    }

    // main loop: handle events
    while (!stop) {
        if (poll(poll_fds.data(), poll_fds.size(), -1) < 0 && errno != EINTR) {
            exit_code = errno;
            Log::error(errno, "Failed to poll events");
            break;
        }
        // call handlers for each active event
        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                fds[i].second();
            }
        }
    }

    appmode()->deactivate();
    ui->stop();

    return exit_code;
}

void Application::add_fdpoll(int fd, const FdEventHandler& handler)
{
    fds.push_back(std::make_pair(fd, handler));
}

void Application::exit(int rc)
{
    exit_code = rc;
    exit_event.set();
}

bool Application::ui_initialize(const Rectangle& wnd, const std::string& app_id)
{
    std::string new_app_id = app_id;
    Rectangle new_wnd = wnd;

#ifdef HAVE_COMPOSITOR
    Compositor::setup_overlay(new_wnd, new_app_id);
#endif

    UiWayland* wayland = new UiWayland([this](Ui::Event event) {
        ui_append_event(event);
    });
    wayland->width = new_wnd.width;
    wayland->height = new_wnd.height;
    if (!wayland->initialize(new_app_id)) {
        delete wayland;
        return false;
    }
    ui.reset(wayland);
    return true;
}

void Application::ui_append_event(Ui::Event event)
{
    std::lock_guard lock(ui_mutex);

    if (event.type == Ui::Event::Type::WindowRedraw) {
        if (!ui_events.empty() &&
            ui_events.back().type == Ui::Event::Type::WindowRedraw) {
            return; // already exists
        }
        // remove the same event to append new one to the tail
        std::erase_if(ui_events, [](const Ui::Event& event) {
            return event.type == Ui::Event::Type::WindowRedraw;
        });
    }

    ui_events.emplace_back(event);
    ui_notify.set();
}

void Application::ui_handle_event(Ui::Event event)
{
    switch (event.type) {
        case Ui::Event::Type::WindowClose:
            exit(0);
            break;
        case Ui::Event::Type::WindowResize:
            appmode()->window_resize();
            break;
        case Ui::Event::Type::WindowRedraw: {
            Pixmap& wnd = ui->lock_surface();
            if (wnd) {
// #define TRACE_DRAW_TIME
#ifdef TRACE_DRAW_TIME
                struct timespec begin_time;
                clock_gettime(CLOCK_MONOTONIC, &begin_time);
#endif
                appmode()->window_redraw(wnd);
                text.draw(wnd);
                ui->commit_surface();
#ifdef TRACE_DRAW_TIME
                struct timespec end_time;
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                const size_t ns =
                    (end_time.tv_sec * 1000000000 + end_time.tv_nsec) -
                    (begin_time.tv_sec * 1000000000 + begin_time.tv_nsec);
                Log::info("Redraw in {:.6f} sec",
                          static_cast<double>(ns) / 1000000000);
#endif
            }

        } break;
        case Ui::Event::Type::WindowRescale:
            // TODO
            break;
        case Ui::Event::Type::KeyPress:
            if (!appmode()->handle_keyboard(event.data.key)) {
                printf("Unhandled key: %s\n",
                       event.data.key.to_string().c_str());
            }
            break;
        case Ui::Event::Type::MouseMove:
            appmode()->handle_mmove(event.data.mouse);
            break;
        case Ui::Event::Type::MouseClick:
            if (!appmode()->handle_mclick(event.data.mouse)) {
                printf("Unhandled mouse: %s\n",
                       event.data.mouse.to_string().c_str());
            }
            break;
    }
}

AppMode* Application::appmode()
{
    switch (mode) {
        case Mode::Viewer:
            return &viewer;
        case Mode::Slideshow:
            return &slideshow;
        case Mode::Gallery:
            return &gallery;
    };
    return nullptr;
}

void Application::switch_mode(Mode next)
{
    if (next != mode) {
        AppMode* cmode = appmode();
        mode = next;
        AppMode* nmode = appmode();

        ImagePtr image = cmode->current_image();
        cmode->deactivate();
        nmode->activate(image);
    }
}

void Application::redraw()
{
    ui_append_event({ Ui::Event::Type::WindowRedraw, {} });
}

void Application::list_change(const FsMonitor::Event event,
                              const ImageList::EntryPtr& entry)
{
    (void)event;
    (void)entry;
}

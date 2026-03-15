// SPDX-License-Identifier: MIT
// File system monitor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "fsmonitor.hpp"

#include "application.hpp"
#include "buildconf.hpp"
#include "log.hpp"

#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <ranges>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

FsMonitor& FsMonitor::self()
{
    static FsMonitor singleton;
    return singleton;
}

#ifndef HAVE_INOTIFY
FsMonitor::~FsMonitor() {}
void FsMonitor::initialize() {}
void FsMonitor::add(const std::filesystem::path&) {}
void FsMonitor::handle_event(const inotify_event*) {}
#else

FsMonitor::~FsMonitor()
{
    if (fd != -1) {
        for (const auto& wd : std::views::keys(watch)) {
            inotify_rm_watch(fd, wd);
        }
        close(fd);
    }
}

void FsMonitor::initialize()
{
    assert(fd == -1);

    fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        Log::error(errno, "Unable to initialize FS monitor");
    } else {
        Application::self().add_fdpoll(fd, [this]() {
            while (true) {
                uint8_t buffer[PATH_MAX];
                const ssize_t len = read(fd, buffer, sizeof(buffer));
                if (len < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    break; // something went wrong
                }
                ssize_t pos = 0;
                while (pos + sizeof(struct inotify_event) <=
                       static_cast<size_t>(len)) {
                    const inotify_event* event =
                        reinterpret_cast<const inotify_event*>(&buffer[pos]);
                    handle_event(event);
                    pos += sizeof(inotify_event) + event->len;
                }
            }
        });
    }
}

void FsMonitor::add(const std::filesystem::path& path)
{
    if (fd == -1) {
        return; // not available
    }

    assert(path.is_absolute());

    // check if file is already watched by its parent
    if (std::filesystem::is_regular_file(path)) {
        const std::filesystem::path& parent = path.parent_path();
        const auto it =
            std::find_if(watch.begin(), watch.end(), [&parent](const auto& it) {
                return parent == it.second.parent_path();
            });
        if (it != watch.end()) {
            return; // already watched
        }
    }

    const int wd =
        inotify_add_watch(fd, path.c_str(),
                          IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVE |
                              IN_DELETE_SELF | IN_MOVE_SELF);
    if (wd == -1) {
        Log::error(errno, "Unable to add monitoring path {}", path.string());
        return;
    }

    watch.insert(std::make_pair(wd, path));
}

void FsMonitor::handle_event(const inotify_event* event)
{
    if (event->mask & IN_IGNORED) {
        // remove from the watch list
        watch.erase(event->wd);
        return;
    }

    const auto it = watch.find(event->wd);
    assert(it != watch.end());
    if (it == watch.end()) {
        return;
    }

    // compose full path
    std::filesystem::path path = it->second;
    if (event->len) {
        path /= event->name;
    }

    // send app event
    Application& app = Application::self();
    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
        Log::verbose("FSMON: Create {}", path.c_str());
        app.add_event(AppEvent::FileCreate { path, false });
    } else if (event->mask &
               (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_MOVE_SELF)) {
        Log::verbose("FSMON: Remove {}", path.c_str());
        app.add_event(AppEvent::FileRemove { path });
    } else if (event->mask & IN_CLOSE) {
        Log::verbose("FSMON: Modify {}", path.c_str());
        app.add_event(AppEvent::FileModify { path });
    } else {
        assert(false && "unhandled event");
    }
}

#endif // HAVE_INOTIFY

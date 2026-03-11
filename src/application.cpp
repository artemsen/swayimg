// SPDX-License-Identifier: MIT
// Image viewer application: main loop and event handler.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"

#include "buildconf.hpp"
#include "frame_profiler.hpp"
#include "fsmonitor.hpp"

#include <chrono>
#include "gallery.hpp"
#include "imagelist.hpp"
#include "log.hpp"
#include "luaengine.hpp"
#include "resources.hpp"
#include "slideshow.hpp"
#include "text.hpp"
#include "viewer.hpp"

#ifdef HAVE_WAYLAND
#include "ui_wayland.hpp"
#ifdef HAVE_COMPOSITOR
#include "compositor.hpp"
#endif // HAVE_COMPOSITOR
#endif // HAVE_WAYLAND
#ifdef HAVE_VULKAN
#include "vulkan_aa.hpp"
#include "vulkan_blur.hpp"
#include "vulkan_ctx.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_swapchain.hpp"
#include "vulkan_texture.hpp"
#include <memory>
static std::unique_ptr<VulkanSwapchain> vk_swapchain;
static TextureCache vk_texcache;
#endif // HAVE_VULKAN
#ifdef HAVE_DRM
#include "ui_drm.hpp"
#endif // HAVE_DRM

#include <poll.h>
#include <signal.h>

Application& Application::self()
{
    static Application singleton;
    return singleton;
}

Application::Application()
{
#ifdef HAVE_COMPOSITOR
    // defaults for people with Sway compositor
    Compositor compositor;
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
    ImageEntryPtr first_entry = il_initialize();
    if (!first_entry) {
        Log::info("Image list is empty, exit");
        return 0;
    }

    // initialize UI
    if (!ui_initialize()) {
        return 1;
    }

    // initialize signal handler
    sig_initialize();

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

#ifdef HAVE_VULKAN
    // Destroy Vulkan resources before Wayland disconnect, to avoid
    // use-after-free of wl_surface in static destructors.
    if (VulkanCtx::self().is_active()) {
        vk_texcache.clear();
        VulkanAA::self().destroy();
        VulkanBlur::self().destroy();
        VulkanPipeline::self().destroy();
        vk_swapchain.reset();
        VulkanCtx::self().destroy();
    }
#endif // HAVE_VULKAN

    ui->stop();

    // Print frame profiling statistics
    FrameProfiler::instance().print_stats();

    return exit_code;
}

void Application::exit(int rc)
{
    stop_flag = true;
    exit_code = rc;
    exit_event.set();
}

void Application::set_mode(Mode mode)
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
        ImageEntryPtr entry = app_mode->current_entry();
        app_mode->deactivate();

#ifdef HAVE_VULKAN
        if (VulkanCtx::self().is_active()) {
            vk_texcache.clear();
        }
#endif

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

void Application::add_fdpoll(int fd, const FdEventHandler& handler)
{
    fds.push_back(std::make_pair(fd, handler));
}

void Application::add_event(const AppEvent::Holder& event)
{
    std::lock_guard lock(event_mutex);

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

ImageEntryPtr Application::il_initialize()
{
    ImageList& il = ImageList::self();

    ImageEntryPtr first_entry = nullptr;

    if (!sparams.from_file.empty()) {
        first_entry = il.load(sparams.from_file);
    }
    if (!sparams.sources.empty()) {
        if (sparams.sources.size() == 1 && sparams.sources[0] == "-") {
            first_entry = il.load(
                std::vector<std::filesystem::path> { ImageEntry::SRC_STDIN });
        } else {
            first_entry = il.load(sparams.sources);
        }
    }
    if (sparams.from_file.empty() && sparams.sources.empty()) {
        first_entry = il.load(std::vector<std::filesystem::path> { "." });
    }

    return first_entry;
}

bool Application::ui_initialize()
{
    Rectangle window = sparams.window;

#ifdef HAVE_WAYLAND
    std::string app_id = sparams.app_id;

#ifdef HAVE_COMPOSITOR
    if (sparams.use_overlay || static_cast<Point>(window)) {
        Compositor compositor;
        if (compositor.type == Compositor::None) {
            Log::error("Current compositor not supported for managing window "
                       "position");
        } else {
            const Rectangle focused = compositor.get_focus();
            if (focused) {
                if (!static_cast<Point>(window)) {
                    window.x = focused.x;
                    window.y = focused.y;
                }
                if (!static_cast<Size>(window)) {
                    window.width = focused.width;
                    window.height = focused.height;
                }
            }
            compositor.set_overlay(window, app_id);
        }
    }
#endif // HAVE_COMPOSITOR

    UiWayland* wayland = new UiWayland();
    if (static_cast<Size>(window)) {
        wayland->width = window.width;
        wayland->height = window.height;
    }
    wayland->dnd = sparams.dnd;
    wayland->cursor_hide = sparams.cursor_hide;
    wayland->decoration = sparams.decoration;
    if (sparams.fullscreen.has_value()) {
        wayland->fullscreen = sparams.fullscreen.value();
    }
    if (wayland->initialize(app_id)) {
        ui.reset(wayland);

#ifdef HAVE_VULKAN
        if (sparams.renderer != Renderer::Software) {
            auto& vk = VulkanCtx::self();
            if (vk.init(wayland->get_wl_display(),
                         wayland->get_wl_surface(), wayland->width,
                         wayland->height)) {
                vk_swapchain = std::make_unique<VulkanSwapchain>();
                if (vk_swapchain->init(
                        wayland->get_wl_display(),
                        wayland->get_wl_surface(), wayland->width,
                        wayland->height)) {
                    if (VulkanPipeline::self().init(
                            vk_swapchain->get_extent())) {
                        vk_texcache.set_budget(vk.get_vram_budget());
                        VulkanBlur::self().init(
                            vk_swapchain->get_extent().width,
                            vk_swapchain->get_extent().height);
                        VulkanAA::self().init();
                        Log::info("Vulkan rendering initialized "
                                  "(VRAM budget: {}MB)",
                                  vk.get_vram_budget() / (1024 * 1024));
                    } else {
                        Log::error("Vulkan pipeline init failed");
                        vk_swapchain.reset();
                        vk.destroy();
                    }
                } else {
                    Log::error("Vulkan swapchain init failed");
                    vk_swapchain.reset();
                    vk.destroy();
                }
            } else if (sparams.renderer == Renderer::Vulkan) {
                Log::error("Vulkan requested but not available");
            } else {
                Log::info("Vulkan not available, using software "
                           "renderer");
            }
        } else {
            Log::info("Software renderer selected");
        }
#endif // HAVE_VULKAN

    } else {
        delete wayland;
    }

#endif // HAVE_WAYLAND

#ifdef HAVE_DRM
    if (!ui) {
        if (sparams.renderer == Renderer::Vulkan) {
            Log::info("DRM backend: Vulkan not supported, using software");
        }
        UiDrm* drm = new UiDrm();
        if (static_cast<Size>(window)) {
            drm->width = window.width;
            drm->height = window.height;
        }
        drm->freq = sparams.drm_freq;
        if (drm->initialize()) {
            ui.reset(drm);
        } else {
            delete drm;
        }
    }
#endif // HAVE_DRM

    return !!ui;
}

void Application::sig_initialize()
{
    struct sigaction sigact;
    sigact.sa_handler = [](int signum) {
        Application::self().add_event(AppEvent::Signal { signum });
    };
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGUSR1, &sigact, nullptr);
    sigaction(SIGUSR2, &sigact, nullptr);
}

void Application::event_loop()
{
    // register app event handler
    add_fdpoll(event_notify, [this]() {
        std::unique_lock lock(event_mutex);
        assert(!event_queue.empty());
        const AppEvent::Holder event = event_queue.front();
        event_queue.pop_front();
        if (event_queue.empty()) {
            event_notify.reset();
        }
        lock.unlock();
        handle_event(event);
    });

    // register exit handler
    add_fdpoll(exit_event, [this]() {
        stop_flag = true;
    });

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
    auto profiling_start = std::chrono::high_resolution_clock::now();
    int nav_counter = 0;
    bool is_profiling = FrameProfiler::instance().is_profiling_enabled();

    while (!stop_flag) {
        // Auto-navigate during profiling mode
        if (is_profiling) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - profiling_start).count();

            // Queue navigation events periodically during profiling
            if (elapsed_ms > (++nav_counter * 1000)) {
                // Create navigation events to simulate user input
                int nav_type = (nav_counter / 3) % 3;
                if (nav_type == 0) {
                    add_event(AppEvent::KeyPress { XKB_KEY_Down, 0 });
                } else if (nav_type == 1) {
                    add_event(AppEvent::KeyPress { XKB_KEY_Up, 0 });
                } else {
                    add_event(AppEvent::KeyPress { XKB_KEY_Page_Down, 0 });
                }
            }

            // Stop after ~30 seconds or when frame profiler indicates stop
            if (elapsed_ms > 30000 || FrameProfiler::instance().should_stop_profiling()) {
                stop_flag = true;
                continue;
            }
        }

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
                                                const AppEvent::FileCreate&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::FileModify&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::FileRemove&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<
                                     decltype(event),
                                     const AppEvent::ScanProgress&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<
                                     decltype(event),
                                     const AppEvent::ScanComplete&>) {
                handle_event(event);
            } else if constexpr (std::is_same_v<decltype(event),
                                                const AppEvent::ImageReady&>) {
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
#ifdef HAVE_VULKAN
    if (VulkanCtx::self().is_active() && vk_swapchain) {
        vk_swapchain->recreate(event.size.width, event.size.height);
        VulkanPipeline::self().update_viewport(vk_swapchain->get_extent());
        VulkanBlur::self().resize(vk_swapchain->get_extent().width,
                                  vk_swapchain->get_extent().height);
    }
#endif
    current_mode()->window_resize(event.size);
    redraw();
}

void Application::handle_event(const AppEvent::WindowRescale& event)
{
#ifdef HAVE_VULKAN
    if (VulkanCtx::self().is_active() && vk_swapchain) {
        const Size wnd_size = ui->get_window_size();
        vk_swapchain->recreate(wnd_size.width, wnd_size.height);
        VulkanPipeline::self().update_viewport(vk_swapchain->get_extent());
        VulkanBlur::self().resize(vk_swapchain->get_extent().width,
                                  vk_swapchain->get_extent().height);
    }
#endif
    Text::self().set_scale(event.scale);
    redraw();
}

void Application::handle_event(const AppEvent::WindowRedraw&)
{
#ifdef HAVE_VULKAN
    if (VulkanCtx::self().is_active() && vk_swapchain) {
        Log::PerfTimer timer;

        VkCommandBuffer cmd = vk_swapchain->begin_frame();
        if (cmd != VK_NULL_HANDLE) {
            // Destroy textures deferred from previous frame (safe after fence)
            vk_texcache.flush_pending();

            // Pre-render pass: compute/transfer work (e.g., blur)
            current_mode()->pre_render_vk(cmd, vk_texcache);

            // Begin render pass
            vk_swapchain->begin_render_pass();

            if (active_mode == Mode::Viewer ||
                active_mode == Mode::Slideshow) {
                static_cast<Viewer*>(current_mode())
                    ->window_redraw_vk(cmd, vk_texcache);
            } else if (active_mode == Mode::Gallery) {
                Gallery::self().window_redraw_vk(cmd, vk_texcache);
            }

            // Draw text overlay and mark icon: render to CPU pixmap,
            // upload, draw
            {
                const auto ext = vk_swapchain->get_extent();
                Pixmap text_pm;
                text_pm.create(Pixmap::ARGB, ext.width, ext.height);
                Text::self().draw(text_pm);

                // Draw mark icon if current image is marked
                auto entry = current_mode()->current_entry();
                if (entry && entry->mark) {
                    constexpr ssize_t margin = 10;
                    const ssize_t mx =
                        static_cast<ssize_t>(ext.width) -
                        static_cast<ssize_t>(Resource::mark.width()) - margin;
                    const ssize_t my =
                        static_cast<ssize_t>(ext.height) -
                        static_cast<ssize_t>(Resource::mark.height()) - margin;
                    text_pm.mask(Resource::mark, { mx, my },
                                 current_mode()->get_mark_color());
                }

                // Release previous text overlay and upload new one
                vk_texcache.release(SIZE_MAX);
                GpuTexture* ttex =
                    vk_texcache.get_or_upload(text_pm, SIZE_MAX, 0);
                if (ttex) {
                    VulkanPipeline::self().draw_image(
                        cmd, *ttex, 0, 0, static_cast<float>(ext.width),
                        static_cast<float>(ext.height));
                }
            }

            if (!vk_swapchain->end_frame()) {
                const Size wnd_size = ui->get_window_size();
                if (!vk_swapchain->recreate(wnd_size.width,
                                            wnd_size.height)) {
                    // Device lost or unrecoverable — fall back
                    Log::error("Vulkan device lost, falling back to "
                               "software renderer");
                    vk_texcache.clear();
                    VulkanAA::self().destroy();
                    VulkanBlur::self().destroy();
                    VulkanPipeline::self().destroy();
                    vk_swapchain.reset();
                    VulkanCtx::self().destroy();
                } else {
                    VulkanPipeline::self().update_viewport(
                        vk_swapchain->get_extent());
                }
            }

            if (Log::verbose_enable()) {
                Log::verbose("Vulkan redraw in {:.6f} sec", timer.time());
            }
        }
        return;
    }
#endif // HAVE_VULKAN

    Pixmap& wnd = ui->lock_surface();
    if (wnd) {
        Log::PerfTimer timer;

        // Frame profiling for performance investigation
        FrameProfiler::instance().frame_start();

        current_mode()->window_redraw(wnd);
        Text::self().draw(wnd);
        ui->commit_surface();

        FrameProfiler::instance().frame_end();

        if (Log::verbose_enable()) {
            Log::verbose("Redraw in {:.6f} sec", timer.time());
        }
    } else {
        // frame not ready, re-queue redraw for next frame callback
        redraw();
    }
}

void Application::handle_event(const AppEvent::KeyPress& event)
{
    Log::verbose("Key event: {} (mode={})", event.key.to_string(),
                 static_cast<int>(active_mode));
    if (!current_mode()->handle_keyboard(event.key) &&
        !Xkb::is_modifier(event.key.key)) {
        const std::string msg =
            std::format("Unhandled key: {}", event.key.to_string());
        Log::info("Unhandled key: {}", event.key.to_string());
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
    current_mode()->handle_signal(event.signal);
}

void Application::handle_event(const AppEvent::FileCreate& event)
{
    ImageList& il = ImageList::self();
    std::vector<ImageEntryPtr> entries;
    if (std::filesystem::is_directory(event.path)) {
        if (il.recursive || event.force) {
            entries = il.add(event.path);
        }
    } else {
        entries = il.add(event.path);
    }
    for (auto& it : entries) {
        current_mode()->handle_imagelist(AppMode::ImageListEvent::Create, it);
    }
    redraw();
}

void Application::handle_event(const AppEvent::FileModify& event)
{
    if (!std::filesystem::is_directory(event.path)) {
        ImageList& il = ImageList::self();
        ImageEntryPtr entry = il.find(event.path);
        if (entry) {
            current_mode()->handle_imagelist(AppMode::ImageListEvent::Modify,
                                             entry);
        }
    }
}

void Application::handle_event(const AppEvent::FileRemove& event)
{
    if (!std::filesystem::is_directory(event.path)) {
        ImageList& il = ImageList::self();
        ImageEntryPtr entry = il.find(event.path);
        if (entry) {
            il.remove(entry);
            current_mode()->handle_imagelist(AppMode::ImageListEvent::Remove,
                                             entry);
        }
    }
}

void Application::handle_event(const AppEvent::ScanProgress&)
{
    ImageList::self().drain_pending();
    redraw();
}

void Application::handle_event(const AppEvent::ScanComplete&)
{
    redraw();
}

void Application::handle_event(const AppEvent::ImageReady& event)
{
    if (active_mode == Mode::Viewer || active_mode == Mode::Slideshow) {
        Viewer& viewer = Viewer::self();
        viewer.on_image_ready(event.image, event.entry);
    }
}

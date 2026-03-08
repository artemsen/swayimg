// SPDX-License-Identifier: MIT
// DRM based user interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "ui_drm.hpp"

#include "log.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <memory>

using DrmResource =
    std::unique_ptr<drmModeRes, decltype(&drmModeFreeResources)>;
using DrmConnector =
    std::unique_ptr<drmModeConnector, decltype(&drmModeFreeConnector)>;

UiDrm::FrameBuffer::~FrameBuffer()
{
    if (id) {
        drmModeRmFB(fd, id);
    }
    if (handle) {
        struct drm_mode_destroy_dumb destroy = { handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    if (data) {
        munmap(data, size);
    }
}

bool UiDrm::FrameBuffer::create(const int fd, const Size& sz)
{
    // create dumb
    struct drm_mode_create_dumb dumb_create = {};
    dumb_create.width = sz.width;
    dumb_create.height = sz.height;
    dumb_create.bpp = 32; // xrgb
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb_create) < 0) {
        Log::error(errno, "Unable to create dumb DRM");
        return false;
    }

    handle = dumb_create.handle;
    size = dumb_create.size;

    // create frame buffer
    uint32_t handles[4] = {};
    uint32_t strides[4] = {};
    uint32_t offsets[4] = {};
    handles[0] = dumb_create.handle;
    strides[0] = dumb_create.pitch;
    if (drmModeAddFB2(fd, dumb_create.width, dumb_create.height,
                      DRM_FORMAT_XRGB8888, handles, strides, offsets, &id,
                      0) < 0) {
        Log::error(errno, "Unable to add DRM frambuffer");
        return false;
    }

    // map frame buffer
    struct drm_mode_map_dumb dumb_map = {};
    dumb_map.handle = dumb_create.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dumb_map) < 0) {
        Log::error(errno, "Unable to map DRM frambuffer");
        return false;
    }

    // create memory map
    data = mmap(0, dumb_create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                dumb_map.offset);
    if (data == MAP_FAILED) {
        Log::error(errno, "Unable to create mmap");
        return false;
    }

    return true;
}

UiDrm::~UiDrm()
{
    if (fd != -1) {
        if (crtc_save) {
            drmModeSetCrtc(fd, crtc_save->crtc_id, crtc_save->buffer_id,
                           crtc_save->x, crtc_save->y, &conn_id, 1,
                           &crtc_save->mode);
            drmModeFreeCrtc(crtc_save);
        }
        close(fd);
    }
}

bool UiDrm::initialize()
{
    // open drm device
    if (!device_path.empty()) {
        if (!drm_open(device_path, false)) {
            return false;
        }
    } else {
        // try first 3 cards
        for (size_t i = 0; i < 3; ++i) {
            std::filesystem::path path = "/dev/dri";
            path /= std::format("card{}", i);
            if (drm_open(path, true)) {
                break;
            }
        }
        if (fd == -1) {
            Log::error("No suitable DRM card found");
            return false;
        }
    }

    DrmResource resources(drmModeGetResources(fd), &drmModeFreeResources);
    if (!resources) {
        Log::error(errno, "Unable to get DRM resources");
        return false;
    }

    DrmConnector connector(get_connector(resources.get()),
                           &drmModeFreeConnector);
    if (!connector) {
        Log::error("Unable to get DRM connector");
        return false;
    }

    crtc_id = get_crtc(resources.get(), connector.get());
    if (!crtc_id) {
        Log::error("Unable to get CRTC");
        return false;
    }

    drmModeModeInfo* mode_info = get_mode(connector.get());
    if (!mode_info) {
        Log::error("Unable to get any mode");
        return false;
    }
    drmModeModeInfo mode = *mode_info;

    // create frame buffers
    for (auto& it : fb) {
        if (!it.create(fd, { mode.hdisplay, mode.vdisplay })) {
            return false;
        }
    }

    cfb = &fb[0];
    pm.attach(Pixmap::RGB, mode.hdisplay, mode.vdisplay, cfb->data);
    conn_id = connector->connector_id;

    // save the previous CRTC configuration and set new one
    crtc_save = drmModeGetCrtc(fd, crtc_id);
    if (drmModeSetCrtc(fd, crtc_id, cfb->id, 0, 0, &conn_id, 1, &mode) < 0) {
        Log::error(errno, "Unable to set CRTC mode");
        return false;
    }

    return true;
}

Size UiDrm::get_window_size()
{
    return pm;
}

Pixmap& UiDrm::lock_surface()
{
    return pm;
}

void UiDrm::commit_surface()
{
    drmEventContext event {};
    event.version = DRM_EVENT_CONTEXT_VERSION;
    event.page_flip_handler = &UiDrm::on_page_flipped;

    struct pollfd fds {};
    fds.fd = fd;
    fds.events = POLLIN;

    drmModePageFlip(fd, crtc_id, cfb->id, DRM_MODE_PAGE_FLIP_EVENT, this);
    if (poll(&fds, 1, -1) > 0 && fds.revents & POLLIN) {
        drmHandleEvent(fd, &event);
    }
}

bool UiDrm::drm_open(const std::filesystem::path& path, bool silent)
{
    assert(fd == -1);

    fd = open(path.c_str(), O_RDWR);

    if (fd == -1) {
        if (!silent) {
            Log::error(errno, "Unable to open {}", path.string());
        }
    } else {
        // check capability
        uint64_t cap;
        if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap) < 0 || !cap) {
            if (!silent) {
                Log::error("Unsupported DRM {}", path.string());
            }
            close(fd);
            fd = -1;
        }
    }

    return fd != -1;
}

drmModeConnectorPtr UiDrm::get_connector(drmModeResPtr resources) const
{
    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnectorPtr conn =
            drmModeGetConnector(fd, resources->connectors[i]);
        if (!conn) {
            continue;
        }
        // filter out empty and unconnected nodes
        if (conn->count_modes == 0 || conn->count_encoders == 0 ||
            conn->connection != DRM_MODE_CONNECTED) {
            drmModeFreeConnector(conn);
            continue;
        }

        // filter out by name
        if (!connector_name.empty()) {
            const std::string name = std::format(
                "{}-{}", drmModeGetConnectorTypeName(conn->connector_type),
                conn->connector_type_id);
            if (connector_name != name) {
                drmModeFreeConnector(conn);
                continue;
            }
        }

        return conn;
    }

    return nullptr;
}

uint32_t UiDrm::get_crtc(drmModeResPtr resources,
                         drmModeConnectorPtr connector) const
{
    for (int i = 0; i < connector->count_encoders; ++i) {
        drmModeEncoderPtr enc = drmModeGetEncoder(fd, connector->encoders[i]);
        if (enc) {
            for (int i = 0; i < resources->count_crtcs; ++i) {
                if (enc->possible_crtcs & (1 << i)) {
                    drmModeFreeEncoder(enc);
                    return resources->crtcs[i];
                }
            }
            drmModeFreeEncoder(enc);
        }
    }
    return 0;
}

drmModeModeInfoPtr UiDrm::get_mode(drmModeConnectorPtr connector) const
{
    if (width && height) {
        for (int i = 0; i < connector->count_modes; ++i) {
            drmModeModeInfoPtr mode = &connector->modes[i];
            // filter out resolution
            if (mode->hdisplay != width || mode->vdisplay != height) {
                continue;
            }
            // filter out frequency
            if (freq &&
                freq != mode->clock * 1000 / (mode->htotal * mode->vtotal)) {
                continue;
            }
            return mode;
        }
    }

    // get preferred mode
    for (int i = 0; i < connector->count_modes; ++i) {
        drmModeModeInfo* mode = &connector->modes[i];
        if (mode->flags & DRM_MODE_TYPE_PREFERRED) {
            return mode;
        }
    }

    return &connector->modes[0]; // use first mode as fallback
}

void UiDrm::on_page_flipped(int, unsigned int, unsigned int, unsigned int,
                            void* data)
{
    // switch frame buffer
    UiDrm* instance = reinterpret_cast<UiDrm*>(data);
    instance->cfb =
        instance->cfb == &instance->fb[0] ? &instance->fb[1] : &instance->fb[0];
    instance->pm.attach(Pixmap::RGB, instance->pm.width(),
                        instance->pm.height(), instance->cfb->data);
}

// SPDX-License-Identifier: MIT
// DRM based user interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "../application.h"
#include "buildcfg.h"
#include "uiface.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#pragma GCC diagnostic pop

/** DRM frame buffer. */
struct fbuffer {
    uint8_t* data;   ///< Buffer data
    size_t size;     ///< Total size of the buffer (bytes)
    uint32_t id;     ///< DRM buffer Id
    uint32_t handle; ///< DRM buffer handle
};

/** DRM context. */
struct drm {
    int fd;                   ///< DRM file handle
    uint32_t conn_id;         ///< Connector Id
    uint32_t crtc_id;         ///< CRTC Id
    drmModeCrtcPtr crtc_save; ///< Previous CRTC mode

    struct fbuffer fb[2]; ///< Frame buffers
    struct fbuffer* cfb;  ///< Currently displayed frame buffer
    struct pixmap pm;     ///< Currently displayed pixmap
};

/**
 * Free frame buffer.
 * @param fd DRM file descriptor
 * @param fb pointer to the frame buffer description
 */
static void free_fb(int fd, struct fbuffer* fb)
{
    if (fb->id) {
        drmModeRmFB(fd, fb->id);
    }
    if (fb->handle) {
        struct drm_mode_destroy_dumb destroy = {
            .handle = fb->handle,
        };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    if (fb->data) {
        munmap(fb->data, fb->size);
    }
}

/**
 * Create frame buffer.
 * @param fd DRM file descriptor
 * @param fb pointer to the frame buffer description
 * @param width,height size of frame buffer in pixels
 * @return true if frame buffer was created successfully
 */
static bool create_fb(int fd, struct fbuffer* fb, size_t width, size_t height)
{
    struct drm_mode_create_dumb dumb_create = { 0 };
    struct drm_mode_map_dumb dumb_map = { 0 };
    uint32_t handles[4] = { 0 };
    uint32_t strides[4] = { 0 };
    uint32_t offsets[4] = { 0 };

    // create dumb
    dumb_create.width = width;
    dumb_create.height = height;
    dumb_create.bpp = 32; // xrgb
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb_create) < 0) {
        const int rc = errno;
        fprintf(stderr, "Unable to create dumb: [%d] %s\n", rc, strerror(rc));
        goto fail;
    }

    fb->handle = dumb_create.handle;
    fb->size = dumb_create.size;

    // create frambuffer
    handles[0] = dumb_create.handle;
    strides[0] = dumb_create.pitch;
    if (drmModeAddFB2(fd, dumb_create.width, dumb_create.height,
                      DRM_FORMAT_XRGB8888, handles, strides, offsets, &fb->id,
                      0) < 0) {
        const int rc = errno;
        fprintf(stderr, "Unable to add frambuffer: [%d] %s\n", rc,
                strerror(rc));
        goto fail;
    }

    // map frambuffer
    dumb_map.handle = dumb_create.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dumb_map) < 0) {
        const int rc = errno;
        fprintf(stderr, "Unable to map frambuffer: [%d] %s\n", rc,
                strerror(rc));
        goto fail;
    }

    // create memory map
    fb->data = mmap(0, dumb_create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                    dumb_map.offset);
    if (fb->data == MAP_FAILED) {
        const int rc = errno;
        fprintf(stderr, "Unable to create mmap: [%d] %s\n", rc, strerror(rc));
        fb->data = NULL;
        goto fail;
    }

    return true;

fail:
    free_fb(fd, fb);
    memset(fb, 0, sizeof(*fb));
    return false;
}

/**
 * Get CRTC id.
 * @param fd DRM file descriptor
 * @param res DRM resources
 * @param conn DRM connector
 * @return CRTC id or 0 if not found
 */
static uint32_t get_crtc(int fd, drmModeRes* res, drmModeConnector* conn)
{
    for (int i = 0; i < conn->count_encoders; ++i) {
        drmModeEncoder* enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (enc) {
            for (int i = 0; i < res->count_crtcs; ++i) {
                if (enc->possible_crtcs & (1 << i)) {
                    drmModeFreeEncoder(enc);
                    return res->crtcs[i];
                }
            }
            drmModeFreeEncoder(enc);
        }
    }
    return 0;
}

/**
 * Initialize DRM connection.
 * @param fd DRM file descriptor
 * @param conn_id output connector id
 * @param crtc_id output CRTC id
 * @param mode output mode
 * @return true DRM initialized
 */
static bool drm_init(int fd, uint32_t* conn_id, uint32_t* crtc_id,
                     drmModeModeInfo* mode)
{
    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
        const int rc = errno;
        fprintf(stderr, "Unable to get DRM modes: [%d] %s\n", rc, strerror(rc));
        return false;
    }

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector* conn;
        drmModeEncoder* enc = NULL;

        // open next connector
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) {
            continue;
        }
        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        // get first available encoder
        for (int j = 0; !enc && j < conn->count_encoders; ++j) {
            enc = drmModeGetEncoder(fd, conn->encoders[j]);
        }
        if (!enc) {
            drmModeFreeConnector(conn);
            continue;
        }

        // get preferred mode
        *mode = conn->modes[0]; // use first available as fallback
        for (int j = 0; j < conn->count_modes; ++j) {
            if (conn->modes[j].flags & DRM_MODE_TYPE_PREFERRED) {
                *mode = conn->modes[0];
                break;
            }
        }

        *conn_id = conn->connector_id;
        *crtc_id = get_crtc(fd, res, conn);

        drmModeFreeEncoder(enc);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        return true;
    }

    drmModeFreeResources(res);

    fprintf(stderr, "DRM connector not found\n");
    return false;
}

static struct pixmap* drm_draw_begin(void* data)
{
    struct drm* ctx = data;
    ctx->cfb = ctx->cfb == &ctx->fb[0] ? &ctx->fb[1] : &ctx->fb[0];
    ctx->pm.data = (argb_t*)ctx->cfb->data;
    return &ctx->pm;
}

static void drm_draw_commit(void* data)
{
    struct drm* ctx = data;
    drmModePageFlip(ctx->fd, ctx->crtc_id, ctx->cfb->id,
                    DRM_MODE_PAGE_FLIP_EVENT, NULL);
}

static size_t drm_get_width(void* data)
{
    struct drm* ctx = data;
    return ctx->pm.width;
}

static size_t drm_get_height(void* data)
{
    struct drm* ctx = data;
    return ctx->pm.height;
}

static void drm_free(void* data)
{
    struct drm* ctx = data;

    if (ctx->crtc_save) {
        drmModeCrtcPtr crtc = ctx->crtc_save;
        drmModeSetCrtc(ctx->fd, crtc->crtc_id, crtc->buffer_id, crtc->x,
                       crtc->y, &ctx->conn_id, 1, &crtc->mode);
        drmModeFreeCrtc(crtc);
    }

    free_fb(ctx->fd, &ctx->fb[0]);
    free_fb(ctx->fd, &ctx->fb[1]);

    if (ctx->fd != -1) {
        close(ctx->fd);
    }

    free(ctx);
}

void* ui_init_drm(struct ui* handlers)
{
    drmModeModeInfo mode;
    struct drm* ctx;

    ctx = calloc(1, sizeof(struct drm));
    if (!ctx) {
        return NULL;
    }

    // open DRM, try first 2 cards
    ctx->fd = -1;
    for (size_t i = 0; ctx->fd == -1 && i < 2; ++i) {
        int fd;
        uint64_t cap;
        char path[16];
        snprintf(path, sizeof(path), "/dev/dri/card%zu", i);

        // open drm and check capability
        fd = open(path, O_RDWR);
        if (fd == -1) {
            continue;
        }
        if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap) < 0 || !cap) {
            close(fd);
        } else {
            ctx->fd = fd;
        }
    }
    if (ctx->fd == -1) {
        fprintf(stderr, "Suitable DRM card not found\n");
        goto fail;
    }

    if (!drm_init(ctx->fd, &ctx->conn_id, &ctx->crtc_id, &mode)) {
        goto fail;
    }

    if (!create_fb(ctx->fd, &ctx->fb[0], mode.hdisplay, mode.vdisplay) ||
        !create_fb(ctx->fd, &ctx->fb[1], mode.hdisplay, mode.vdisplay)) {
        goto fail;
    }
    ctx->cfb = &ctx->fb[0];
    ctx->pm.format = pixmap_xrgb;
    ctx->pm.width = mode.hdisplay;
    ctx->pm.height = mode.vdisplay;

    // save the previous CRTC configuration and set new one
    ctx->crtc_save = drmModeGetCrtc(ctx->fd, ctx->crtc_id);
    if (drmModeSetCrtc(ctx->fd, ctx->crtc_id, ctx->cfb->id, 0, 0, &ctx->conn_id,
                       1, &mode) < 0) {
        const int rc = errno;
        fprintf(stderr, "Unable to set CRTC mode: [%d] %s\n", rc, strerror(rc));
        goto fail;
    }

    handlers->draw_begin = drm_draw_begin;
    handlers->draw_commit = drm_draw_commit;
    handlers->get_width = drm_get_width;
    handlers->get_height = drm_get_height;
    handlers->free = drm_free;

    return ctx;

fail:
    drm_free(ctx);
    return NULL;
}

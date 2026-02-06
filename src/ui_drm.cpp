// SPDX-License-Identifier: MIT
// DRM based user interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "ui_drm.hpp"

// #include <errno.h>
// #include <fcntl.h>
// #include <poll.h>
// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/mman.h>
// #include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#pragma GCC diagnostic pop

UiDrm::UiDrm(const EventHandler& handler)
    : Ui(handler)
{
}

bool UiDrm::initialize()
{
    return false;
}

size_t UiDrm::get_width()
{
    return 0;
}

size_t UiDrm::get_height()
{
    return 0;
}

Pixmap& UiDrm::lock_surface()
{
    static Pixmap pm;
    return pm;
}

void UiDrm::commit_surface() { }

// /** DRM frame buffer. */
// struct fbuffer {
//     uint8_t* data;   ///< Buffer data
//     size_t size;     ///< Total size of the buffer (bytes)
//     uint32_t id;     ///< DRM buffer Id
//     uint32_t handle; ///< DRM buffer handle
// };

// /** DRM context. */
// struct drm {
//     int fd;                   ///< DRM file handle
//     uint32_t conn_id;         ///< Connector Id
//     uint32_t crtc_id;         ///< CRTC Id
//     drmModeCrtcPtr crtc_save; ///< Previous CRTC mode

//     struct fbuffer fb[2]; ///< Frame buffers
//     struct fbuffer* cfb;  ///< Currently displayed frame buffer
//     struct pixmap pm;     ///< Currently displayed pixmap
// };

// /** DRM configuration. */
// struct drm_conf {
//     const char* path;      ///< Path to DRM device
//     const char* connector; ///< Connector name
//     size_t width;          ///< Display width
//     size_t height;         ///< Display height
//     size_t freq;           ///< Display frequency
// };

// /**
//  * Free frame buffer.
//  * @param fd DRM file descriptor
//  * @param fb pointer to the frame buffer description
//  */
// static void free_fb(int fd, struct fbuffer* fb)
// {
//     if (fb->id) {
//         drmModeRmFB(fd, fb->id);
//     }
//     if (fb->handle) {
//         struct drm_mode_destroy_dumb destroy = {
//             .handle = fb->handle,
//         };
//         drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
//     }
//     if (fb->data) {
//         munmap(fb->data, fb->size);
//     }
// }

// /**
//  * Create frame buffer.
//  * @param fd DRM file descriptor
//  * @param fb pointer to the frame buffer description
//  * @param width,height size of frame buffer in pixels
//  * @return true if frame buffer was created successfully
//  */
// static bool create_fb(int fd, struct fbuffer* fb, size_t width, size_t
// height)
// {
//     struct drm_mode_create_dumb dumb_create = { 0 };
//     struct drm_mode_map_dumb dumb_map = { 0 };
//     uint32_t handles[4] = { 0 };
//     uint32_t strides[4] = { 0 };
//     uint32_t offsets[4] = { 0 };

//     // create dumb
//     dumb_create.width = width;
//     dumb_create.height = height;
//     dumb_create.bpp = 32; // xrgb
//     if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb_create) < 0) {
//         const int rc = errno;
//         fprintf(stderr, "Unable to create dumb: [%d] %s\n", rc,
//         strerror(rc)); goto fail;
//     }

//     fb->handle = dumb_create.handle;
//     fb->size = dumb_create.size;

//     // create frame buffer
//     handles[0] = dumb_create.handle;
//     strides[0] = dumb_create.pitch;
//     if (drmModeAddFB2(fd, dumb_create.width, dumb_create.height,
//                       DRM_FORMAT_XRGB8888, handles, strides, offsets,
//                       &fb->id, 0) < 0) {
//         const int rc = errno;
//         fprintf(stderr, "Unable to add frambuffer: [%d] %s\n", rc,
//                 strerror(rc));
//         goto fail;
//     }

//     // map frame buffer
//     dumb_map.handle = dumb_create.handle;
//     if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dumb_map) < 0) {
//         const int rc = errno;
//         fprintf(stderr, "Unable to map frambuffer: [%d] %s\n", rc,
//                 strerror(rc));
//         goto fail;
//     }

//     // create memory map
//     fb->data = mmap(0, dumb_create.size, PROT_READ | PROT_WRITE, MAP_SHARED,
//     fd,
//                     dumb_map.offset);
//     if (fb->data == MAP_FAILED) {
//         const int rc = errno;
//         fprintf(stderr, "Unable to create mmap: [%d] %s\n", rc,
//         strerror(rc)); fb->data = NULL; goto fail;
//     }

//     return true;

// fail:
//     free_fb(fd, fb);
//     memset(fb, 0, sizeof(*fb));
//     return false;
// }

// /**
//  * Get CRTC id.
//  * @param fd DRM file descriptor
//  * @param res DRM resources
//  * @param conn DRM connector
//  * @return CRTC id or 0 if not found
//  */
// static uint32_t get_crtc(int fd, drmModeRes* res, drmModeConnector* conn)
// {
//     for (int i = 0; i < conn->count_encoders; ++i) {
//         drmModeEncoder* enc = drmModeGetEncoder(fd, conn->encoders[i]);
//         if (enc) {
//             for (int i = 0; i < res->count_crtcs; ++i) {
//                 if (enc->possible_crtcs & (1 << i)) {
//                     drmModeFreeEncoder(enc);
//                     return res->crtcs[i];
//                 }
//             }
//             drmModeFreeEncoder(enc);
//         }
//     }
//     return 0;
// }

// /**
//  * Get DRM connector.
//  * @param fd DRM file descriptor
//  * @param res DRM resources
//  * @param name optional connector name, NULL to get first available connector
//  * @return connector instance or NULL if not found
//  */
// static drmModeConnector* get_connector(int fd, drmModeRes* res,
//                                        const char* name)
// {
//     for (int i = 0; i < res->count_connectors; ++i) {
//         drmModeConnector* conn = drmModeGetConnector(fd, res->connectors[i]);
//         if (!conn) {
//             continue;
//         }
//         // filter out empty and unconnected nodes
//         if (conn->count_modes == 0 || conn->count_encoders == 0 ||
//             conn->connection != DRM_MODE_CONNECTED) {
//             drmModeFreeConnector(conn);
//             continue;
//         }
//         if (name) {
//             // filter out by name
//             const char* type_name =
//                 drmModeGetConnectorTypeName(conn->connector_type);
//             char full_name[DRM_CONNECTOR_NAME_LEN];
//             snprintf(full_name, sizeof(full_name), "%s-%d", type_name,
//                      conn->connector_type_id);
//             if (strcmp(name, full_name) != 0) {
//                 drmModeFreeConnector(conn);
//                 continue;
//             }
//         }

//         return conn;
//     }

//     return NULL;
// }

// /**
//  * Get DRM mode.
//  * @param conn DRM connector
//  * @param width,height,freq preferred display resolution and frequency
//  * @return mode instance
//  */
// static drmModeModeInfo* get_mode(drmModeConnector* conn, size_t width,
//                                  size_t height, size_t freq)
// {
//     if (width && height) {
//         for (int i = 0; i < conn->count_modes; ++i) {
//             drmModeModeInfo* mode = &conn->modes[i];
//             // filter out resolution
//             if (mode->hdisplay != width || mode->vdisplay != height) {
//                 continue;
//             }
//             // filter out frequency
//             if (freq &&
//                 freq != mode->clock * 1000 / (mode->htotal * mode->vtotal)) {
//                 continue;
//             }
//             return mode;
//         }
//     }

//     // get preferred mode
//     for (int i = 0; i < conn->count_modes; ++i) {
//         drmModeModeInfo* mode = &conn->modes[i];
//         if (mode->flags & DRM_MODE_TYPE_PREFERRED) {
//             return mode;
//         }
//     }

//     return &conn->modes[0]; // use first mode as fallback
// }

// /**
//  * Open DRM device.
//  * @param path device path to open
//  * @return file descriptor, -1 on errors
//  */
// static int drm_open(const char* path, bool silent)
// {
//     int fd = open(path, O_RDWR);

//     if (fd == -1) {
//         if (!silent) {
//             const int rc = errno;
//             fprintf(stderr, "Unable to open %s: [%d] %s\n", path, rc,
//                     strerror(rc));
//         }
//     } else {
//         // check capability
//         uint64_t cap;
//         if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap) < 0 || !cap) {
//             if (!silent) {
//                 fprintf(stderr, "Unsupported DRM %s\n", path);
//             }
//             close(fd);
//             fd = -1;
//         }
//     }

//     return fd;
// }

// /**
//  * Initialize DRM.
//  * @param cfg DRM configuration
//  * @return pointer to DRM context or NULL on errors
//  */
// static struct drm* drm_init(const struct drm_conf* cfg)
// {
//     struct drm* ctx;
//     drmModeRes* resources = NULL;
//     drmModeConnector* connector = NULL;
//     drmModeModeInfo mode;

//     ctx = calloc(1, sizeof(struct drm));
//     if (!ctx) {
//         return NULL;
//     }

//     // open drm device
//     if (cfg->path) {
//         ctx->fd = drm_open(cfg->path, false);
//         if (ctx->fd == -1) {
//             goto fail;
//         }
//     } else {
//         // try first 3 cards
//         for (size_t i = 0; i < 3; ++i) {
//             char path[16];
//             snprintf(path, sizeof(path), "/dev/dri/card%zu", i);
//             ctx->fd = drm_open(path, true);
//             if (ctx->fd != -1) {
//                 break;
//             }
//         }
//         if (ctx->fd == -1) {
//             fprintf(stderr, "No suitable DRM card found\n");
//             goto fail;
//         }
//     }

//     resources = drmModeGetResources(ctx->fd);
//     if (!resources) {
//         const int rc = errno;
//         fprintf(stderr, "Unable to get DRM resources: [%d] %s\n", rc,
//                 strerror(rc));
//         goto fail;
//     }

//     connector = get_connector(ctx->fd, resources, cfg->connector);
//     if (!connector) {
//         fprintf(stderr, "Unable to get DRM connector\n");
//         goto fail;
//     }

//     ctx->crtc_id = get_crtc(ctx->fd, resources, connector);
//     if (!ctx->crtc_id) {
//         fprintf(stderr, "Unable to get CRTC\n");
//         goto fail;
//     }

//     mode = *get_mode(connector, cfg->width, cfg->height, cfg->freq);

//     // create frame buffers
//     if (!create_fb(ctx->fd, &ctx->fb[0], mode.hdisplay, mode.vdisplay) ||
//         !create_fb(ctx->fd, &ctx->fb[1], mode.hdisplay, mode.vdisplay)) {
//         goto fail;
//     }

//     ctx->cfb = &ctx->fb[0];
//     ctx->pm.data = (argb_t*)ctx->cfb->data;
//     ctx->pm.format = pixmap_xrgb;
//     ctx->pm.width = mode.hdisplay;
//     ctx->pm.height = mode.vdisplay;
//     ctx->conn_id = connector->connector_id;

//     // save the previous CRTC configuration and set new one
//     ctx->crtc_save = drmModeGetCrtc(ctx->fd, ctx->crtc_id);
//     if (drmModeSetCrtc(ctx->fd, ctx->crtc_id, ctx->cfb->id, 0, 0,
//     &ctx->conn_id,
//                        1, &mode) < 0) {
//         const int rc = errno;
//         fprintf(stderr, "Unable to set CRTC mode: [%d] %s\n", rc,
//         strerror(rc)); goto fail;
//     }

//     drmModeFreeConnector(connector);
//     drmModeFreeResources(resources);

//     return ctx;

// fail:
//     if (ctx) {
//         if (ctx->crtc_save) {
//             drmModeFreeCrtc(ctx->crtc_save);
//         }
//         if (ctx->fd > 0) {
//             close(ctx->fd);
//         }
//         free(ctx);
//     }
//     if (connector) {
//         drmModeFreeConnector(connector);
//     }
//     if (resources) {
//         drmModeFreeResources(resources);
//     }
//     return NULL;
// }

// static void on_page_flipped(__attribute__((unused)) int fd,
//                             __attribute__((unused)) unsigned int frame,
//                             __attribute__((unused)) unsigned int sec,
//                             __attribute__((unused)) unsigned int usec,
//                             void* data)
// {
//     // switch frame buffer
//     struct drm* ctx = data;
//     ctx->cfb = ctx->cfb == &ctx->fb[0] ? &ctx->fb[1] : &ctx->fb[0];
//     ctx->pm.data = (argb_t*)ctx->cfb->data;
// }

// static struct pixmap* drm_draw_begin(void* data)
// {
//     struct drm* ctx = data;
//     return &ctx->pm;
// }

// static void drm_draw_commit(void* data)
// {
//     struct drm* ctx = data;
//     drmEventContext ev = {
//         .version = DRM_EVENT_CONTEXT_VERSION,
//         .page_flip_handler = on_page_flipped,
//     };
//     struct pollfd fds = {
//         .fd = ctx->fd,
//         .events = POLLIN,
//     };

//     drmModePageFlip(ctx->fd, ctx->crtc_id, ctx->cfb->id,
//                     DRM_MODE_PAGE_FLIP_EVENT, ctx);
//     if (poll(&fds, 1, -1) > 0 && fds.revents & POLLIN) {
//         drmHandleEvent(ctx->fd, &ev);
//     }
// }

// static size_t drm_get_width(void* data)
// {
//     struct drm* ctx = data;
//     return ctx->pm.width;
// }

// static size_t drm_get_height(void* data)
// {
//     struct drm* ctx = data;
//     return ctx->pm.height;
// }

// static void drm_free(void* data)
// {
//     struct drm* ctx = data;

//     if (ctx->crtc_save) {
//         drmModeCrtcPtr crtc = ctx->crtc_save;
//         drmModeSetCrtc(ctx->fd, crtc->crtc_id, crtc->buffer_id, crtc->x,
//                        crtc->y, &ctx->conn_id, 1, &crtc->mode);
//         drmModeFreeCrtc(crtc);
//     }

//     free_fb(ctx->fd, &ctx->fb[0]);
//     free_fb(ctx->fd, &ctx->fb[1]);

//     if (ctx->fd != -1) {
//         close(ctx->fd);
//     }

//     free(ctx);
// }

// void* ui_init_drm(const struct config* cfg, struct ui* handlers)
// {
//     struct drm* ctx;
//     struct drm_conf drm_conf = { 0 };
//     const struct config* drm_sect = config_section(cfg, CFG_DRM);
//     const char* value;

//     // get DRM config
//     value = config_get(drm_sect, CFG_DRM_PATH);
//     if (strcmp(value, CFG_AUTO) != 0) {
//         drm_conf.path = value;
//     }
//     value = config_get(drm_sect, CFG_DRM_CONNECTOR);
//     if (strcmp(value, CFG_AUTO) != 0) {
//         drm_conf.connector = value;
//     }
//     value = config_get(drm_sect, CFG_DRM_MODE);
//     if (strcmp(value, CFG_AUTO) != 0) {
//         unsigned int width = 0, height = 0, freq = 0;
//         if (sscanf(value, "%ux%u@%u", &width, &height, &freq) > 0) {
//             drm_conf.width = width;
//             drm_conf.height = height;
//             drm_conf.freq = freq;
//         }
//     }

//     // init drm
//     ctx = drm_init(&drm_conf);

//     if (ctx) {
//         handlers->draw_begin = drm_draw_begin;
//         handlers->draw_commit = drm_draw_commit;
//         handlers->get_width = drm_get_width;
//         handlers->get_height = drm_get_height;
//         handlers->free = drm_free;
//     }

//     return ctx;
// }

// SPDX-License-Identifier: MIT
// DRM based user interface.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "ui.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#pragma GCC diagnostic pop

#include <filesystem>
#include <string>

/** DRM based user interface. */
class UiDrm : public Ui {
public:
    ~UiDrm();

    /**
     * Initialize DRM UI.
     * @return true on success
     */
    bool initialize();

    // Implementation of UI generic interface
    Size get_window_size() override;
    Pixmap& lock_surface() override;
    void commit_surface() override;

private:
    /**
     * Open DRM device.
     * @param path device path to open
     * @param silent flag to suppress errro message
     * @return true if DRM device was opened
     */
    bool drm_open(const std::filesystem::path& path, bool silent);

    /**
     * Get DRM connector.
     * @param resources DRM resources
     * @return connector instance or nullptr if not found
     */
    drmModeConnectorPtr get_connector(drmModeResPtr resources) const;

    /**
     * Get CRTC id.
     * @param resources DRM resources
     * @param connector DRM connector
     * @return CRTC id or 0 if not found
     */
    uint32_t get_crtc(drmModeResPtr resources,
                      drmModeConnectorPtr connector) const;

    /**
     * Get DRM mode.
     * @param connector DRM connector
     * @return mode instance
     */
    drmModeModeInfoPtr get_mode(drmModeConnectorPtr connector) const;

    // Page flip callback, see DRM API for details
    static void on_page_flipped(int, unsigned int, unsigned int, unsigned int,
                                void* data);

public:
    // DRM configuration
    std::filesystem::path device_path; ///< Path to DRM device
    std::string connector_name;        ///< Connector name
    size_t width = 0;                  ///< Display width
    size_t height = 0;                 ///< Display height
    size_t freq = 0;                   ///< Display frequency

private:
    /** DRM frame buffer. */
    struct FrameBuffer {
        ~FrameBuffer();
        /**
         * Create frame buffer.
         * @param fd DRM file descriptor
         * @param size frame buffer size in pixels
         * @return true if frame buffer was created successfully
         */
        bool create(const int fd, const Size& size);

        int fd = -1;          ///< DRM file handle
        void* data = nullptr; ///< Buffer data
        size_t size = 0;      ///< Total size of the buffer (bytes)
        uint32_t id = 0;      ///< DRM buffer Id
        uint32_t handle = 0;  ///< DRM buffer handle
    };

    int fd = -1;                        ///< DRM file handle
    uint32_t conn_id = 0;               ///< Connector Id
    uint32_t crtc_id = 0;               ///< CRTC Id
    drmModeCrtcPtr crtc_save = nullptr; ///< Previous CRTC mode

    FrameBuffer fb[2];          ///< Frame buffers
    FrameBuffer* cfb = nullptr; ///< Currently displayed frame buffer
    Pixmap pm;                  ///< Currently displayed pixmap
};

// SPDX-License-Identifier: MIT
// Image view mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"
#include "fdevent.hpp"
#include "image.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

class Viewer : public AppMode {
public:
    /** Fixed image scale. */
    enum class Scale : uint8_t {
        Optimal,    ///< Fit to window, but not more than 100%
        FitWindow,  ///< Fit to window size
        FitWidth,   ///< Fit width to window width
        FitHeight,  ///< Fit height to window height
        FillWindow, ///< Fill the window
        RealSize,   ///< Real image size (100%)
        Keep,       ///< Keep absolute zoom across images
    };

    /** Fixed viewport position (horizontal/vertical). */
    enum class Position : uint8_t {
        Center,
        TopCenter,
        BottomCenter,
        LeftCenter,
        RightCenter,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    /** Background modes. */
    enum class Background : uint8_t {
        Solid,
        Mirror,
        Extend,
        Auto,
    };

    /** External handler called when switching an image. */
    using ImageSwitchHadler = std::function<void(const ImagePtr&)>;

    Viewer();

    /**
     * Switch to the next image file.
     * @param pos next entry position
     * @param from first entry
     * @return true if image was loaded
     */
    bool open_file(const ImageList::Pos pos,
                   const ImageList::EntryPtr& from = nullptr);

    /**
     * Switch to specified frame.
     * @param index frame index to activate.
     */
    void set_frame(const size_t index);

    /**
     * Get curren scale.
     * @return scale factor
     */
    double get_scale() const { return cur_scale; }

    /**
     * Set fixed scale.
     * @param scale fixed scale type
     */
    void set_scale(const Scale scale);

    /**
     * Set absolute scale.
     * @param scale scale factor (1.0 = 100%)
     * @param preserve_x, preserve_y image coordinates to preserve
     */
    void
    set_scale(const double scale,
              const size_t preserve_x = std::numeric_limits<size_t>::max(),
              const size_t preserve_y = std::numeric_limits<size_t>::max());

    /**
     * Set image pisition on canvas.
     * @param pos one of predefined position
     */
    void move(const Position pos);

    /**
     * Move image on canvas.
     * @param dx,dy step delta
     */
    void move(const ssize_t dx, const ssize_t dy);

    /**
     * Flip image vertically.
     */
    void flip_vertical();

    /**
     * Flip image horizontally.
     */
    void flip_horizontal();

    /**
     * Rotate image.
     * @param angle rotation angle (only 90, 180, or 270)
     */
    void rotate(const size_t angle);

    /**
     * Resume animation.
     */
    void animation_resume();

    /**
     * Stop animation.
     */
    void animation_stop();

    /**
     * Subscribe to the image switch event.
     * @param handler event handler
     */
    void subscribe(const ImageSwitchHadler& handler);

    // app mode interface implementation
    void initialize() override;
    void activate(ImageList::EntryPtr entry) override;
    void deactivate() override;
    void reset() override;
    ImageList::EntryPtr current_image() override;
    void window_resize() override;
    void window_redraw(Pixmap& wnd) override;
    void handle_mmove(const InputMouse& input) override;
    void handle_imagelist(const FsMonitor::Event event,
                          const ImageList::EntryPtr& entry) override;

private:
    /**
     * Fix up image position.
     */
    void fixup_position();

    /**
     * Start preloader.
     */
    void preloader_start();

    /**
     * Stop preloader.
     */
    void preloader_stop();

private:
    /** Image cache. */
    struct Cache {
        /**
         * Trim cache.
         * @param size number of entries to preserve
         */
        void trim(const size_t size);

        /**
         * Put image to the cache.
         * @param image image instance to put
         */
        void put(const ImagePtr& image);

        /**
         * Get image from the cache.
         * @param entry image entry
         * @return image instance or nullptr if no such image in the cache
         */
        ImagePtr get(const ImageList::EntryPtr& entry);

        size_t capacity = 0;        ///< Cache capacity
        std::deque<ImagePtr> cache; ///< Cache container
    };

public:
    bool free_move = false; /// < Flag to allow free image move on canvas
    Scale default_scale = Scale::Optimal;    ///< Default image scale
    Position default_pos = Position::Center; ///< Default image position
    bool imagelist_loop = true;              ///< Flag to loop image list

    bool animation_enable = true; ///< Flag to start animation automatically

    size_t preload_limit = 1; ///< Max number of preloaded images
    size_t history_limit = 1; ///< Max number of images in history cache

    argb_t bkg_window = { argb_t::max, 0, 0, 0 }; ///< Window background
    argb_t bkg_transp = { argb_t::max, 0, 0, 0 }; ///< Transparent background

    Background bkg_mode = Background::Solid; ///< Background mode

    // Grid parameters
    struct Grid {
        bool use = true;
        size_t size = 20;
        argb_t color[2] = {
            { argb_t::max, 0x33, 0x33, 0x33 },
            { argb_t::max, 0x4c, 0x4c, 0x4c }
        };
    } grid;

private:
    std::vector<ImageSwitchHadler> imsw_handlers; ///< Image switch handlers

    ImagePtr image;         ///< Currently shown image
    Point img;              ///< Image position on canvas
    double cur_scale = 1.0; ///< Current scale factor of the image

    size_t wnd_width = 0, wnd_height = 0; ///< Window size in pixels

    size_t mouse_x = 0, mouse_y = 0; ///< Last known mouse position

    FdTimer animation;  ///< Animation timer
    size_t frame_index; ///< Index of the currently displayed frame

    /** Image pool. */
    struct ImagePool {
        Cache preload;          ///< Preloaded images (read ahead)
        Cache history;          ///< Recently viewed images
        std::thread thread;     ///< Preload thread
        std::atomic<bool> stop; ///< Stop signal for preload thread
        std::mutex mutex;       ///< Sync mutex for pool access
    } imp;
};

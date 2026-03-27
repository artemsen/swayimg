// SPDX-License-Identifier: MIT
// Image view mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"
#include "fdevent.hpp"
#include "imagelist.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <variant>

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
        Mirror,
        Extend,
        Auto,
    };

    /**
     * Get global instance of the viewer.
     * @return viewer instance
     */
    static Viewer& self();

    Viewer();

    /**
     * Open next image file.
     * @param dir next entry direction
     * @return true if image was loaded
     */
    bool open(const ImageList::Dir dir);

    /**
     * Load and switch to specified entry.
     * @param from entry to load
     * @return true if image was loaded
     */
    bool open(const ImageEntryPtr& entry);

    /**
     * Reload current image.
     * @return true if image was reloaded
     */
    bool reload();

    /**
     * Get currently showed image instance.
     * @return current image instance
     */
    [[nodiscard]] ImagePtr current_image() const { return image; }

    /**
     * Switch to next frame.
     * @return current frame index
     */
    size_t next_frame();

    /**
     * Switch to previous frame.
     * @return current frame index
     */
    size_t prev_frame();

    /**
     * Export current frame to PNG file.
     * @param path path to file
     */
    void export_frame(const std::filesystem::path& path) const;

    /**
     * Get current scale.
     * @return scale factor
     */
    [[nodiscard]] double get_scale() const { return scale; }

    /**
     * Set fixed scale.
     * @param sc fixed scale type
     */
    void set_scale(const Scale sc);

    /**
     * Set absolute scale.
     * @param sc absolute scale factor (1.0 = 100%)
     * @param preserve image coordinates to preserve
     */
    void set_scale(const double sc, const Point& preserve = Point());

    /**
     * Reset scale and position to defaults.
     */
    void reset();

    /**
     * Set predefined image position on canvas.
     * @param pos one of predefined position
     */
    void set_position(const Position pos);

    /**
     * Set image position on canvas.
     * @param pos new image position
     */
    void set_position(const Point& pos);

    /**
     * Get image position on canvas.
     * @return image position
     */
    [[nodiscard]] Point get_position() const { return position; }

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
     * Set window background to solid color.
     * @param color window background color
     */
    void set_window_background(const argb_t& color);

    /**
     * Set window background to one of extended mode.
     * @param mode window background mode
     */
    void set_window_background(const Background mode);

    /**
     * Set image background for transparent images to solid color.
     * @param color image background color
     */
    void set_image_background(const argb_t& color);

    /**
     * Set image background for transparent images to chessboard.
     * @param size size of single grid cell in pixels
     * @param color1 first color color for cell
     * @param color2 second color color for cell
     */
    void set_image_chessboard(const size_t size, const argb_t& color1,
                              const argb_t& color2);

    /**
     * Set number of images to preload in a separate thread.
     * @param size number of images to preload
     */
    void set_preload_limit(const size_t size);

    /**
     * Set number of previously viewed images to store in cache.
     * @param size number of images to store
     */
    void set_history_limit(const size_t size);

    /**
     * Bind mouse input state to image dragging.
     * @param input state description
     */
    void bind_image_drag(const InputMouse& input);

    // app mode interface implementation
    void initialize() override;
    void activate(const ImageEntryPtr& entry, const Size& wnd) override;
    void deactivate() override;
    ImageEntryPtr current_entry() override;
    void window_resize(const Size& wnd) override;
    void window_redraw(Pixmap& wnd) override;
    void handle_mmove(const InputMouse& input, const Point& pos,
                      const Point& delta) override;
    void handle_pinch(const double scale_delta) override;
    void handle_imagelist(const ImageListEvent event,
                          const ImageEntryPtr& entry) override;

private:
    /**
     * Open next image file.
     * @param dir next entry direction
     * @param entry starting entry
     * @return true if image was opened
     */
    bool switch_image(const ImageList::Dir dir, const ImageEntryPtr& entry);

    /**
     * Set current image.
     * @param img image instance to set as current
     */
    void set_image(const ImagePtr& img);

    /**
     * Switch to specified frame.
     * @param index frame index to activate.
     */
    void set_frame(const size_t index);

    /** Type of text update. */
    enum class TextUpdate : uint8_t {
        All,   ///< Update all info
        Frame, ///< Update only frame index and size
        Scale  ///< Update only scale info
    };

    /**
     * Update text layer.
     * @param what type of updated data
     */
    void update_text(const TextUpdate what) const;

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
        ImagePtr get(const ImageEntryPtr& entry);

        size_t capacity = 1;        ///< Cache capacity
        std::deque<ImagePtr> cache; ///< Cache container
    };

public:
    bool auto_center;    ///< Enable automatic image centering
    bool imagelist_loop; ///< Flag to loop image list

    std::variant<double, Scale> default_scale; ///< Default image scale
    Position default_pos;                      ///< Default image position

private:
    ImagePtr image; ///< Currently shown image
    Point position; ///< Image position on the window
    double scale;   ///< Current scale factor of the image
    Size previmg;   ///< Size of previous image (used for "keep" scale mode)

    Size window_size;                            ///< Window size in pixels
    std::variant<argb_t, Background> window_bkg; ///< Window background

    // Transparent image background
    bool tr_chessboard;   ///< Background mode (chessboard/solid color)
    size_t tr_cbsize;     ///< Size of grid tile
    argb_t tr_cbcolor[2]; ///< Grid colors
    argb_t tr_bgcolor;    ///< Solid color

    bool animation_enable;   ///< Flag to start animation automatically
    FdTimer animation_timer; ///< Animation timer
    size_t frame_index;      ///< Index of the currently displayed frame

    InputMouse drag; ///< Mouse state for dragging an image across the canvas

    /** Image pool. */
    struct ImagePool {
        Cache preload;          ///< Preloaded images (read ahead)
        Cache history;          ///< Recently viewed images
        std::thread thread;     ///< Preload thread
        std::atomic<bool> stop; ///< Stop signal for preload thread
        std::mutex mutex;       ///< Sync mutex for pool access
    } image_pool;
};

// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "appmode.hpp"
#include "image.hpp"
#include "layout.hpp"
#include "threadpool.hpp"

#include <deque>
#include <mutex>
#include <set>

class Gallery : public AppMode {
public:
    /** Thumbnail aspect ratio. */
    enum class Aspect : uint8_t {
        Fit,  ///< Fit image into a square thumbnail
        Fill, ///< Fill square thumbnail with the image
        Keep, ///< Adjust thumbnail size to the aspect ratio of the image
    };

    /**
     * Get global instance of the gallery.
     * @return gallery instance
     */
    static Gallery& self();

    Gallery();

    /**
     * Select another image file.
     * @param dir next entry direction
     * @return true if new image was selected
     */
    bool select(const Layout::Direction dir);

    /**
     * Set thumbnail aspect ratio.
     * @param ratio aspect ratio
     */
    void set_thumb_aspect(const Aspect ratio);

    /**
     * Set thumbnail size.
     * @param size new thumbnail size in pixels
     */
    [[nodiscard]] inline size_t get_thumb_size() const
    {
        return layout.get_thumb_size();
    }

    /**
     * Set thumbnail size.
     * @param size new thumbnail size in pixels
     */
    void set_thumb_size(const size_t size);

    /**
     * Set padding size.
     * @param size new padding size in pixels
     */
    void set_padding_size(const size_t size);

    /**
     * Set border size for selected thumbnail.
     * @param size border size in pixels
     */
    void set_border_size(const size_t size);

    /**
     * Set selected thumbnail border color.
     * @param color selected thumbnail border color
     */
    void set_border_color(const argb_t& color);

    /**
     * Set scale for selected thumbnail.
     * @param scale scale factor for selected thumbnail
     */
    void set_selected_scale(const double scale);

    /**
     * Set background color for selected thumbnail.
     * @param color selected thumbnail background color
     */
    void set_selected_color(const argb_t& color);

    /**
     * Set background color for unselected thumbnails.
     * @param color thumbnail background color
     */
    void set_background_color(const argb_t& color);

    /**
     * Set window color.
     * @param color window color
     */
    void set_window_color(const argb_t& color);

    /**
     * Set mark icon color.
     * @param color mark icon color
     */
    void set_mark_color(const argb_t& color);

    /**
     * Set number of thumbnails stored in memory cache.
     * @param size max number of thumbnails stored in memory cache
     */
    void set_cache_size(const size_t size);

    /**
     * Enable preloading invisible thumbnails.
     * @param enable flag to set
     */
    void enable_preload(const bool enable);

    /**
     * Enable/disable persistent storage.
     * @param enable flag to set
     */
    void enable_pstore(const bool enable);

    /**
     * Set path for persistent storage.
     * @param path directory for persistent storage
     */
    void set_pstore_path(const std::filesystem::path& path);

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
     * Draw thumbnail.
     * @param tlay thumbnail layout description
     * @param wnd target window
     */
    void draw(const Layout::Thumbnail& tlay, Pixmap& wnd);

    /**
     * Manage loading queue.
     */
    void load_thumbnails();

    /**
     * Remove unused thumbnails from the cache.
     */
    void clear_thumbnails();

    /**
     * Get thumbnail pixmap for specified image.
     * @param entry image entry
     * @return thumbnail pixmap or nullptr if thumbnail not yet loaded
     */
    const Pixmap* get_thumbnail(const ImageEntryPtr& entry);

    /**
     * Load image thumbnail.
     * @param entry image entry to load
     */
    void load_thumbnail(const ImageEntryPtr& entry);

    /**
     * Load thumbnail from persistent storage.
     * @param entry image entry for thumbnail
     * @return pixmap with thumbnail
     */
    [[nodiscard]] Pixmap pstore_load(const ImageEntryPtr& entry) const;

    /**
     * Save thumbnail on persistent storage.
     * @param entry image entry for thumbnail
     * @param thumb pixmap with thumbnail
     */
    void pstore_save(const ImageEntryPtr& entry, const Pixmap& thumb) const;

private:
    Layout layout;         ///< Thumbnail layout
    Aspect aspect;         ///< Thumbnail aspect ratio
    size_t border_size;    ///< Selected tile border size
    double selected_scale; ///< Selected tile scale

    argb_t clr_window;     ///< Window background
    argb_t clr_background; ///< Tile background
    argb_t clr_select;     ///< Selected tile background for transparent images
    argb_t clr_border;     ///< Selected tile border

    ThreadPool tpool; ///< Loading threads

    bool pstore_enable; ///< Use persistent storage for thumbnails
    std::filesystem::path pstore_path; ///< Persistent storage path

    /** Thumbnail entry. */
    struct ThumbEntry {
        ImageEntryPtr entry;
        Pixmap pm;
    };
    std::deque<ThumbEntry> cache;   ///< Thumbnails cache
    std::set<ImageEntryPtr> queue;  ///< Loading queue
    std::set<ImageEntryPtr> active; ///< Currently loading

    bool preload;      ///< Enable/disable preloading of invisible thumbnails
    size_t cache_size; ///< Max number of thumbnails in cache
    std::mutex mutex;  ///< Sync mutex for thumbnails cache access
};

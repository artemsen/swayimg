// SPDX-License-Identifier: MIT
// Thumbnail layout for gallery mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

/** Thumbnail layout. */
class Layout {
public:
    /** Selection direction. */
    enum Direction : uint8_t {
        First,
        Last,
        Up,
        Down,
        Left,
        Right,
        PgUp,
        PgDown,
    };

    /** Thumbnail instance with coordinates. */
    struct Thumbnail {
        ImageEntryPtr img; ///< Image entry
        size_t col, row;   ///< Column and row
        Point pos;         ///< Window coordinates
    };

    /**
     * Update layout: recalculate thumbnails scheme.
     */
    void update();

    /**
     * Set output window size (resize layout).
     * @param wnd size of output window
     */
    void set_window_size(const Size& size);

    /**
     * Set thumbnail size.
     * @param size new thumbnail size in pixels
     */
    void set_thumb_size(const size_t size);

    /**
     * Get thumbnail size.
     * @return thumbnail size in pixels
     */
    inline size_t get_thumb_size() const { return thumb_size; }

    /**
     * Set padding size.
     * @param padding new padding size in pixels
     */
    void set_padding(const size_t padding);

    /**
     * Get number of columns in layout scheme.
     * @return number of columns
     */
    [[nodiscard]] inline size_t get_columns() const { return columns; }

    /**
     * Get number of rows in layout scheme.
     * @return number of rows
     */
    [[nodiscard]] inline size_t get_rows() const { return rows; }

    /**
     * Set currently selected thumbnail.
     * @param image image entry to select
     */
    void select(const ImageEntryPtr& image);

    /**
     * Move selection to the next image.
     * @param dir movement direction
     * @return true if new image was selected
     */
    bool select(const Direction dir);

    /**
     * Set selection on thumbnail at specified coordinates.
     * @param pos position within the layout (window)
     * @return true if selection was changed
     */
    bool select(const Point& pos);

    /**
     * Get thumbnail description at specified coordinates.
     * @param pos position within the layout (window)
     * @return thumbnail description or nullptr if not thumbnail not exists
     */
    [[nodiscard]] const Thumbnail* at(const Point& pos) const;

    /**
     * Get currently selected thumbnail.
     * @return currently selected thumbnail
     */
    [[nodiscard]] ImageEntryPtr get_selected() const;

    /**
     * Get layout scheme.
     * @return array with thumbnails layout description
     */
    [[nodiscard]] const std::vector<Thumbnail>& get_scheme() const;

private:
    size_t thumb_size = 200;       ///< Size of thumbnail in pixels
    size_t thumb_padding = 5;      ///< Padding between thumbnails in pixels
    std::vector<Thumbnail> scheme; ///< Layout scheme of visible thumbnails

    Size window;          ///< Layout size (output window)
    size_t columns, rows; ///< Size of the layout in thumbnails

    ImageEntryPtr sel_entry = nullptr; ///< Currently selected entry
    size_t sel_col =
        std::numeric_limits<size_t>::max(); ///< Currently selected column
    size_t sel_row =
        std::numeric_limits<size_t>::max(); ///< Currently selected row
};

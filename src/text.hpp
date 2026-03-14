// SPDX-License-Identifier: MIT
// Text overlay.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"
#include "font.hpp"
#include "image.hpp"

#include <map>
#include <string>
#include <vector>

/** Text overlay. */
class Text {
public:
    /** Text block position. */
    enum Position : uint8_t {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    // Field IDs
    static constexpr const char* FIELD_FILE_PATH = "path";
    static constexpr const char* FIELD_FILE_DIR = "dir";
    static constexpr const char* FIELD_FILE_NAME = "name";
    static constexpr const char* FIELD_FILE_SIZE = "size";
    static constexpr const char* FIELD_FILE_SIZE_HR = "sizehr";
    static constexpr const char* FIELD_FILE_TIME = "time";
    static constexpr const char* FIELD_IMAGE_FORMAT = "format";
    static constexpr const char* FIELD_SCALE = "scale";
    static constexpr const char* FIELD_LIST_INDEX = "list.index";
    static constexpr const char* FIELD_LIST_TOTAL = "list.total";
    static constexpr const char* FIELD_FRAME_INDEX = "frame.index";
    static constexpr const char* FIELD_FRAME_TOTAL = "frame.total";
    static constexpr const char* FIELD_FRAME_WIDTH = "frame.width";
    static constexpr const char* FIELD_FRAME_HEIGHT = "frame.height";
    static constexpr const char* FIELD_META = "meta";

    Text();

    /**
     * Get global instance of text overlay.
     * @return text overlay instance
     */
    static Text& self();

    /**
     * Initialize text overlay.
     */
    void initialize();

    /**
     * Set text scheme for specified block.
     * @param pos block position
     * @param scheme scheme description
     */
    void set_scheme(const Position pos, const std::vector<std::string>& scheme);

    /**
     * Set font face.
     * @param name font face name
     */
    void set_font(const std::string& name);

    /**
     * Set font size.
     * @param size font size in pixels
     */
    void set_size(const size_t size);

    /**
     * Set font scale based on Wayland scale.
     * @param scale the scale factor to multiply by
     */
    void set_scale(const double scale);

    /**
     * Set text padding size.
     * @param pad new padding size
     */
    void set_padding(const size_t pad);

    /**
     * Set foreground text color.
     * @param color new color value
     */
    void set_foreground(const argb_t& color);

    /**
     * Set background text color.
     * @param color new color value
     */
    void set_background(const argb_t& color);

    /**
     * Set shadow text color.
     * @param color new color value
     */
    void set_shadow(const argb_t& color);

    /**
     * Set timer for overall text layer.
     * @param timeout duration in ms before hiding text
     */
    void set_overall_timer(const size_t timeout);

    /**
     * Set timer for status text.
     * @param timeout duration in ms before hiding status text
     */
    void set_status_timer(const size_t timeout);

    /**
     * Show text layer and stop timer.
     */
    void show();

    /**
     * Hide text layer and stop timer.
     */
    void hide();

    /**
     * Check if text layer currently displayed.
     * @return true if layer is visible
     */
    bool is_visible() const { return overall_tm.show; }

    /**
     * Set status text.
     * @param msg status message to display
     */
    void set_status(const std::string& msg);

    /**
     * Reset text overlay (remove all data).
     * @param image currently displayed image
     */
    void reset(const ImagePtr& image);

    /**
     * Reset text overlay (remove all data).
     * @param entry currently displayed image entry
     */
    void reset(const ImageEntryPtr& entry);

    /**
     * Set filed value.
     * @param field field name
     * @param value field value
     */
    void set_field(const std::string& field, const std::string& value);

    /**
     * Update text blocks.
     */
    void update();

    /**
     * Draw text overlay on pixmap.
     * @param target destination pixmap
     */
    void draw(Pixmap& target) const;

private:
    /** Rendered text line. */
    struct Line {
        Line(const std::string& scheme)
            : scheme(scheme)
        {
        }

        /**
         * Update line.
         * @param font font instance
         * @param fields fields values
         */
        void update(const Font& font,
                    const std::map<std::string, std::string>& fields);

        std::string scheme;  ///< Line scheme
        std::string display; ///< Displayed text
        Pixmap pm;           ///< Mask pixmap with rendered text
    };

    /** Key/value. */
    struct KeyVal {
        Line key;
        Line value;
    };

    /** Text block. */
    struct Block {
        /**
         * Get width of the block.
         * @return tuple with key and value width in pixels
         */
        std::tuple<size_t, size_t> width() const;

        /**
         * Get height of the block.
         * @return tuple with height of a single line and total height in pixels
         */
        std::tuple<size_t, size_t> height() const;

        std::vector<KeyVal> data; ///< Block text data
    };

    /**
     * Reinitialize pixmaps.
     */
    void refresh();

    /**
     * Draw text overlay on the window.
     * @param block block to draw
     * @param pos block position
     * @param target destination pixmap (window)
     */
    void draw(const Block& block, const Position pos, Pixmap& target) const;

    /**
     * Draw text line.
     * @param text text line to draw
     * @param target destination pixmap (window)
     * @param pos text position on target pixmap
     */
    void draw(const Pixmap& text, Pixmap& target, const Point& pos) const;

private:
    /** Text hide timeout. */
    struct HideTimeout {
        FdTimer fd;   ///< Timer FD
        size_t delay; ///< Timeout duration in ms
        bool show;    ///< Current state
    };

    bool enable; ///< Enable/disable text layer

    HideTimeout overall_tm; ///< Overall show timer
    HideTimeout status_tm;  ///< Status show timer

    Font font; ///< Font instance

    size_t padding; ///< Text padding

    argb_t foreground; ///< Text foreground color
    argb_t background; ///< Text background color
    argb_t shadow;     ///< Text shadow color

    Pixmap status; ///< Status message

    Block blocks[4]; ///< Four text blocks at window corners
    std::map<std::string, std::string> fields; ///< Data fields
};

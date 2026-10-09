// SPDX-License-Identifier: MIT
// Text overlay.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "fdevent.hpp"
#include "image.hpp"

#include <array>
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

    /** Block scheme description. */
    using Scheme = std::vector<std::string>;

    // Field IDs
    static constexpr const char* FIELD_FILE_PATH = "path";
    static constexpr const char* FIELD_FILE_DIR = "dir";
    static constexpr const char* FIELD_FILE_NAME = "name";
    static constexpr const wchar_t* FIELD_FILE_SIZE = L"size";
    static constexpr const wchar_t* FIELD_FILE_SIZE_HR = L"sizehr";
    static constexpr const wchar_t* FIELD_FILE_TIME = L"time";
    static constexpr const char* FIELD_IMAGE_FORMAT = "format";
    static constexpr const wchar_t* FIELD_SCALE = L"scale";
    static constexpr const wchar_t* FIELD_LIST_INDEX = L"list.index";
    static constexpr const wchar_t* FIELD_LIST_TOTAL = L"list.total";
    static constexpr const wchar_t* FIELD_FRAME_INDEX = L"frame.index";
    static constexpr const wchar_t* FIELD_FRAME_TOTAL = L"frame.total";
    static constexpr const wchar_t* FIELD_FRAME_WIDTH = L"frame.width";
    static constexpr const wchar_t* FIELD_FRAME_HEIGHT = L"frame.height";
    static constexpr const char* FIELD_META = "meta";

    /** Constructor. */
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
    void set_scheme(const Position pos, const Scheme& scheme);

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
     * Set line spacing.
     * @param size line spacing in pixels
     */
    void set_spacing(const ssize_t size);

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
     * Reset text overlay (remove all data except status message).
     */
    void clear();

    /**
     * Check if text layer currently displayed.
     * @return true if layer is visible
     */
    [[nodiscard]] bool is_visible() const { return overall_tm.show; }

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
     * Set filed value (wide char variant).
     * @param field field name
     * @param value field value
     */
    void set_field(const std::wstring& field, const std::wstring& value);

    /**
     * Set filed value (UTF8 variant).
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
    /** Data fields to substitute. */
    using Fields = std::map<std::wstring, std::wstring>;

    /** Single text line. */
    struct TextLine {
        /**
         * Get text width in pixels.
         * @return text width in pixels
         */
        [[nodiscard]] size_t width() const;

        /**
         * Get text height in pixels.
         * @return text height in pixels
         */
        [[nodiscard]] size_t height() const;

        /**
         * Get horizontal margin around text line.
         * @return text height in pixels
         */
        [[nodiscard]] size_t margin() const;

        /**
         * Draw text line.
         * @param target destination pixmap (window)
         * @param pos text position on target pixmap
         * @param color text color
         */
        void draw(Pixmap& target, const Point& pos, const argb_t color) const;

        std::wstring text; ///< Displayed text
    };

    /** Status message block. */
    struct StatusBlock {
        /**
         * Set status text.
         * @param msg status message text
         * @param spacing desired spacing between lines
         */
        void set(const std::string& msg);

        /**
         * Clear status message.
         */
        void clear();

        /**
         * Update block size/spacing parameters.
         * @param spacing desired spacing between lines
         */
        void recalc(const ssize_t spacing);

        std::vector<TextLine> lines; ///< Displayed lines
        size_t width;                ///< Total width in pixels
        size_t height;               ///< Total block height in pixels
        ssize_t line_height;         ///< Line height include spacing
    };

    struct KeyValBlock {
        /** Text line constructed from template. */
        struct TemplateLine : public TextLine {
            /**
             * Update displayed text by applying fields data to template.
             * @param fields fields values
             */
            void update(const Fields& fields);

            std::wstring templ; ///< Text line template
        };

        /**
         * Set text scheme for the block.
         * @param scheme scheme description
         */
        void set_scheme(const Scheme& scheme);

        /**
         * Update block text data.
         * @param fields map of fields
         */
        void update(const Fields& fields);

        /**
         * Update block size/spacing parameters.
         * @param spacing desired spacing between lines
         */
        void recalc(const ssize_t spacing);

        /** Block line with key/value text. */
        struct BlockLine {
            TemplateLine key;
            TemplateLine value;
        };
        std::vector<BlockLine> lines; ///< Displayed lines

        size_t key_width;    ///< Max key width in pixels
        size_t value_width;  ///< Max value width in pixels
        size_t total_width;  ///< Total block width in pixels
        size_t total_height; ///< Total block height in pixels
        ssize_t line_height; ///< Line height include spacing
    };

    /**
     * Recalculate text blocks sizes.
     */
    void recalc();

    /**
     * Draw text block of template lines on the window.
     * @param blkpos block position
     * @param target destination pixmap (window)
     */
    void draw(const Position blkpos, Pixmap& target) const;

    /**
     * Draw text line.
     * @param line text line to draw
     * @param target destination pixmap (window)
     * @param pos text position on target pixmap
     */
    void draw(const TextLine& line, Pixmap& target, const Point& pos) const;

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

    ssize_t spacing; ///< Line spacing in pixels
    size_t padding;  ///< Text padding

    argb_t foreground; ///< Text foreground color
    argb_t background; ///< Text background color
    argb_t shadow;     ///< Text shadow color

    Fields fields; ///< Data fields

    StatusBlock status;                ///< Status message block
    std::array<KeyValBlock, 4> blocks; ///< Four text blocks at window corners
};

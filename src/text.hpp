// SPDX-License-Identifier: MIT
// Text overlay.
// Copyright (C) 2023 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <map>
#include <string>
#include <vector>

/** Text overlay. */
class Text {
public:
    /** Text block position. */
    enum class Position : uint8_t {
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
    static constexpr const char* FIELD_STATUS = "status";

    Text();

    void set_scheme(const Position pos, const std::vector<std::string>& scheme);

    /**
     * Reset text overlay (remove all data).
     */
    void reset(const ImagePtr& image);
    void set_field(const std::string& field, const std::string& value);
    void update();

    /**
     * Reinitialize pixmaps.
     */
    void refresh();

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
        void update(const std::map<std::string, std::string>& fields);

        std::string scheme;
        std::string display;
        Pixmap pm;
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
     * Draw text overlay on the window.
     * @param wnd destination window
     */
    void draw(const Block& block, const Position pos, Pixmap& target) const;

public:
    size_t padding = 10; ///< Text padding

    argb_t foreground = { argb_t::max, 0xcc, 0xcc,
                          0xcc };       ///< Text foreground color
    argb_t background = { 0, 0, 0, 0 }; ///< Text background color
    argb_t shadow = { 0xd0, 0, 0, 0 };  ///< Text shadow color

private:
    Block blocks[4]; /// Four text blocks at window corners
    std::map<std::string, std::string> fields; // Data fields
};

// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

/** Image entry. */
struct ImageEntry {
    std::filesystem::path path; ///< Path to the image file
    std::time_t mtime;          ///< File modification time
    size_t size;                ///< Size of the image file
    size_t index;               ///< Image index in image list
    bool mark;                  ///< Marked image flag

    /**
     * Remove entry (mark it as invalid).
     */
    void remove();

    /**
     * Check if the entry is valid.
     * @return true if image entry valid
     */
    operator bool() const;

    // File name used for image, that is read from stdin through pipe
    static constexpr const char* SRC_STDIN = "stdin://";
    // Special prefix used to load images from external command output
    static constexpr const char* SRC_EXEC = "exec://";
};

using ImageEntryPtr = std::shared_ptr<ImageEntry>;

/** Image instance. */
class Image {
public:
    virtual ~Image() = default;

    /** Image source data. */
    struct Data {
        uint8_t* data = nullptr; ///< Data buffer
        size_t size = 0;         ///< Buffer size
    };

    /**
     * Load (decode) image from raw buffer.
     * @param data raw buffer with image data
     * @return true if image was loaded
     */
    virtual bool load(const Data& data) = 0;

    /**
     * Draw image on pixmap surface.
     * @param frame frame index to draw
     * @param target surface to draw on
     * @param scale image scale factor
     * @param x,y top-left coordinates on target surface
     */
    virtual void draw(const size_t frame, Pixmap& target, const double scale,
                      const ssize_t x, const ssize_t y);

    /**
     * Flip image vertically.
     */
    virtual void flip_vertical();

    /**
     * Flip image horizontally.
     */
    virtual void flip_horizontal();

    /**
     * Rotate image.
     * @param angle rotation angle (only 90, 180, or 270)
     */
    virtual void rotate(size_t angle);

public:
    /** Image frame. */
    struct Frame {
        Pixmap pm;           ///< Frame data
        size_t duration = 0; ///< Frame duration in milliseconds (animation)
    };

    std::vector<Frame> frames; ///< Image frames

    ImageEntryPtr entry; ///< Image entry

    std::string format;                      ///< Image format description
    std::map<std::string, std::string> meta; ///< Meta info
};

using ImagePtr = std::shared_ptr<Image>;

// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "imagelist.hpp"
#include "pixmap.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

/** Image instance. */
class Image {
public:
    virtual ~Image() = default;

    /**
     * Load (decode) image from raw buffer.
     * @param data raw buffer with image data
     * @return true if image was loaded
     */
    virtual bool load(const std::vector<uint8_t>& data) = 0;

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

    std::vector<Frame> frames;               ///< Frames
    std::string format;                      ///< Image format description
    std::map<std::string, std::string> meta; ///< Meta info
    ImageList::EntryPtr entry;               ///< Entry in the image list
};

using ImagePtr = std::shared_ptr<Image>;

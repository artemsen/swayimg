// SPDX-License-Identifier: MIT
// Image instance: pixel data and meta info.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "imagelist.hpp"
#include "pixmap.hpp"

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

/** Image loader and factory. */
class ImageLoader {
public:
    using Constructor = std::function<ImagePtr()>;

    /** Loader priorities: defines the order in loaders list. */
    enum class Priority : uint8_t {
        Highest,
        High,
        Normal,
        Low,
        Lowest,
    };

    /** Loader instance. */
    struct Instance {
        const char* name;   ///< Format name
        Priority priority;  ///< Priority
        Constructor create; ///< Function to create image instance
    };

    template <typename T> struct Registrator {
        Registrator(const char* name, Priority priority)
        {
            ImageLoader::register_format(name, priority, []() {
                return std::make_shared<T>();
            });
        }
    };

    /**
     * Register loader.
     * @param name format name
     * @param priority loader priority
     * @param creator function to create image instance
     */
    static void register_format(const char* name, Priority priority,
                                const Constructor& creator);

    /**
     * Get list of supported loaders.
     * @return list of loaders in priority order
     */
    static std::string format_list();

    /**
     * Load image.
     * @param entry image entry to load
     * @return image instance or nullptr if image wasn't loaded
     */
    static ImagePtr load(const ImageList::EntryPtr& entry);

private:
    /**
     * Get array with loaders.
     * @return loaders array
     */
    static std::vector<Instance>& get_registry();
};

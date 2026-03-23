// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <functional>
#include <memory>
#include <string>

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
            ImageLoader::self().register_format(name, priority, []() {
                return std::make_shared<T>();
            });
        }
    };

    /**
     * Get global instance of image loader.
     * @return image loader instance
     */
    static ImageLoader& self();

    /**
     * Get list of supported loaders.
     * @return list of loaders in priority order
     */
    [[nodiscard]] std::string format_list() const;

    /**
     * Load image.
     * @param entry image entry to load
     * @return image instance or nullptr if image wasn't loaded
     */
    [[nodiscard]] ImagePtr load(const ImageEntryPtr& entry) const;

private:
    /**
     * Register loader.
     * @param name format name
     * @param priority loader priority
     * @param creator function to create image instance
     */
    void register_format(const char* name, Priority priority,
                         const Constructor& creator);

public:
    bool fix_orientation = true; ///< Fix orientation by EXIF

private:
    std::vector<Instance> registry; ///< Loaders
};

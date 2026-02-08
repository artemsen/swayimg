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

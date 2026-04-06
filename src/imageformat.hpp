// SPDX-License-Identifier: MIT
// Image format interface.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <cstring>

class ImageFormat {
public:
    /** Format priorities: defines the order in loaders list. */
    enum class Priority : uint8_t {
        Highest,
        High,
        Normal,
        Low,
        Lowest,
    };

    /** Data buffer. */
    struct Data {
        uint8_t* data = nullptr; ///< Data buffer
        size_t size = 0;         ///< Buffer size
    };

    /**
     * Constructor.
     * @param load_priority format priority
     * @param format_name short format name
     */
    ImageFormat(const Priority load_priority, const char* format_name);

    /**
     * Decode raw image data.
     * @param data source data to decode
     * @return image ensance or nulptr on errors
     */
    virtual ImagePtr decode(const Data& data) = 0;

    /**
     * Encode pixel map.
     * @param pm source image instance
     * @return encoded image data, empty array on errors
     */
    virtual std::vector<uint8_t> encode(const Pixmap& /* pm */) { return {}; }

    // virtual Pixmap preview(const Data& /*data*/, const size_t /* sz */)
    // {
    //     return {};
    // }

protected:
    /**
     * Check signature existence in source data buffer.
     * @param data source data
     * @param signature signature data
     * @param offset signature offset
     * @return true if signature exists
     */
    template <size_t S>
    inline bool check_signature(const Data& data, const uint8_t (&signature)[S],
                                const size_t offset = 0) const
    {
        return data.size > offset + S &&
            std::memcmp(data.data + offset, signature, S) == 0;
    }

public:
    Priority priority; ///< Format priority
    const char* name;  ///< Short format name
};

/** Image format factory. */
class FormatFactory {
public:
    /**
     * Get global instance of image loader.
     * @return image loader instance
     */
    static FormatFactory& self();

    /**
     * Load image.
     * @param entry image entry to load
     * @return image instance or nullptr if image wasn't loaded
     */
    [[nodiscard]] ImagePtr load(const ImageEntryPtr& entry) const;

    /**
     * Register format.
     * @param fmt format decoder/encoder
     */
    void add(ImageFormat* fmt);

    /**
     * Get format handler.
     * @param name short format name
     * @return format handler instance or nullptr if not found
     */
    ImageFormat* get(const char* name);

    /**
     * Get list of supported loaders.
     * @return list of loaders in priority order
     */
    [[nodiscard]] std::string list() const;

public:
    bool fix_orientation = true; ///< Fix orientation by EXIF

private:
    std::vector<ImageFormat*> formats; ///< Format handlers
};

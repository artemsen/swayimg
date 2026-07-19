// SPDX-License-Identifier: MIT
// Image format interface.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

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
    ImageFormat(const Priority load_priority, const char* format_name) noexcept;

    /**
     * Set decode parameters for the format.
     * @param params format parameters
     * @return false if not supported
     */
    virtual bool
    set_params(const std::unordered_map<std::string, bool>& params);

    /**
     * Decode raw image data.
     * @param data source data to decode
     * @return image instance or nulptr on errors
     */
    [[nodiscard]] virtual ImagePtr decode(const Data& data) const = 0;

    /**
     * Encode pixel map.
     * @param pm pixmap to encode
     * @param meta meta data
     * @return encoded image data, empty array on errors
     */
    virtual std::vector<uint8_t>
    encode(const Pixmap& /*pm*/,
           const std::unordered_map<std::string, std::string>& /*meta*/)
    {
        return {};
    }

    /**
     * Get preview (thumbnail).
     * @param data source image data
     * @param sz thumbnail size
     * @param fill thumnail aspect ratio: true=fill, false=fit
     * @return encoded image data, empty array on errors
     */
    [[nodiscard]] virtual Pixmap preview(const Data& data, const size_t sz,
                                         const bool fill) const;

    /**
     * Fix orientation by EXIF data.
     * @param image source image to re-orient
     * @param orientation EXIF orientation, -1 to get from image meta data
     */
    virtual void fix_orientation(ImagePtr& image,
                                 const int orientation = -1) const;

    /**
     * Fix orientation by EXIF data.
     * @param pm pixmap to re-orient
     * @param orientation EXIF orientation
     */
    static void fix_orientation(Pixmap& pm, const int orientation);

    /**
     * Read meta data from image (EXIF, IPTC, XMP).
     * @param data source image data
     * @param image target image instance
     */
    static bool read_metadata(const Data& data, ImagePtr& image);

protected:
    /**
     * Check signature existence in source data buffer.
     * @param data source data
     * @param signature signature data
     * @param offset signature offset
     * @return true if signature exists
     */
    template <size_t S>
    bool check_signature(const Data& data, const uint8_t (&signature)[S],
                         const size_t offset = 0) const
    {
        return data.size > offset + S &&
            std::memcmp(data.data + offset, signature, S) == 0;
    }

    /**
     * Create thumbnail from full-size image.
     * @param pm origin image pixmap
     * @param sz thumbnail size
     * @param fill thumnail aspect ratio: true=fill, false=fit
     * @return thumbnail pixmap
     */
    static Pixmap make_thumb(const Pixmap& pm, const size_t sz,
                             const bool fill);

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

    /** Constructor. */
    FormatFactory();

    /**
     * Load image.
     * @param entry image entry to load
     * @return image instance or nullptr if image wasn't loaded
     */
    [[nodiscard]] ImagePtr load(const ImageEntryPtr& entry) const;

    /**
     * Save image in PNG format.
     * @param pm pixmap to encode
     * @param meta meta data
     * @param path path to write the file
     * @return true on success
     */
    static bool save(const Pixmap& pm,
                     const std::unordered_map<std::string, std::string>& meta,
                     const std::filesystem::path& path);

    /**
     * Decode raw image data.
     * @param data source data to decode
     * @return image instance or nullptr on errors
     */
    [[nodiscard]] ImagePtr decode(const ImageFormat::Data& data) const;

    /**
     * Get preview (thumbnail).
     * @param entry image entry to load
     * @param sz thumbnail size
     * @param fill thumnail aspect ratio: true=fill, false=fit
     * @return encoded image data, empty array on errors
     */
    [[nodiscard]] Pixmap preview(const ImageEntryPtr& entry, const size_t sz,
                                 const bool fill) const;

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
    bool fix_orientation; ///< Fix orientation by EXIF
    bool embedded_thumb;  ///< Use embedded thumbnails

private:
    std::vector<ImageFormat*> formats; ///< Format handlers
};

// SPDX-License-Identifier: MIT
// Image format factory.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "imageformat.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

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

// SPDX-License-Identifier: MIT
// Image format interface.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <variant>
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

    /** Format config. */
    class Config {
    public:
        /** Parameter status. */
        enum Status : uint8_t { Invalid, Handled, Unhandled };

        /**
         * Set configuration parameter.
         * @param name parameter name
         * @param value parameter value
         */
        template <typename T> void set(const std::string& name, const T& value)
        {
            params.insert({
                name, { .value = value, .status = Unhandled }
            });
        }

        /**
         * Get configuration parameter and change its status.
         * @param name parameter name
         * @param value parameter value to write
         */
        void get(const std::string& name, bool& value);

        /**
         * Get configuration parameter and change its status.
         * @param name parameter name
         * @param value parameter value to write
         */
        void get(const std::string& name, argb_t& value);

        /**
         * Get configuration parameter and change its status.
         * @param name parameter name
         * @param value parameter value to write
         * @param min_val, max_val valid value range
         */
        void get(const std::string& name, size_t& value, const size_t min_val,
                 const size_t max_val);

        /**
         * Get configuration parameter and change its status.
         * @param name parameter name
         * @param value parameter value to write
         * @param min_len min lenght of the value
         */
        void get(const std::string& name, std::string& value,
                 const size_t min_len);

        /**
         * Get configuration parameter with specified status.
         * @param status parameter status
         * @return parameters array with specified state
         */
        [[nodiscard]] std::vector<std::string> get(const Status status) const;

    private:
        /** Parameter value. */
        struct Value {
            std::variant<bool, size_t, std::string> value;
            Status status;
        };

        /** Config container. */
        std::unordered_map<std::string, Value> params;
    };

    /**
     * Constructor.
     * @param load_priority format priority
     * @param format_name short format name
     */
    ImageFormat(const Priority load_priority, const char* format_name) noexcept;

    /**
     * Set configuration for the format.
     * @param params format parameters
     */
    virtual void set_config(Config& params);

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
    // Name of "enable" decoder parameter
    static constexpr const std::string enable_param_name = "enable";

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
    bool enable;       ///< Enable/disable decoder
    const char* name;  ///< Short format name
};

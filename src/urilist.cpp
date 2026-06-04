// SPDX-License-Identifier: MIT
// URI list parser and composer (RFC 2483).
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "urilist.hpp"

#include <cctype>

constexpr const char* CRLF = "\r\n";
constexpr const size_t CRLF_LEN = 2;
constexpr const char* URI_FILE = "file://";
constexpr const size_t URI_FILE_LEN = 7;
constexpr const char UNSAFE[] = { ' ', '<', '>',  '"', '#', '%', '{',
                                  '}', '|', '\\', '^', '[', ']', '`' };

/**
 * Decode safe URI string to normal.
 * @param text safe URI text
 * @return decoded unsafe value
 */
static std::string decode_safe(const std::string& text)
{
    const size_t len = text.length();

    std::string decoded;
    decoded.reserve(len);

    for (size_t i = 0; i < len; ++i) {
        if (text[i] == '%' && i + 2 < len) {
            const std::string hex = text.substr(i + 1, 2);
            if (!std::isxdigit(static_cast<uint8_t>(hex[0])) ||
                !std::isxdigit(static_cast<uint8_t>(hex[1]))) {
                return {}; // bad format
            }
            const unsigned long val = std::stoul(hex, nullptr, 16);
            decoded += static_cast<char>(val);
            i += 2;
        } else {
            decoded += text[i];
        }
    }

    return decoded;
}

/**
 * Encode string to safe URI format.
 * @param text source unsafe text
 * @return safe URI text
 */
static std::string encode_safe(const std::string& text)
{
    const size_t len = text.length();

    std::string encoded;
    encoded.reserve(len);

    for (size_t i = 0; i < len; ++i) {
        bool is_unsafe = false;
        for (const char unsafe : UNSAFE) {
            is_unsafe = text[i] == unsafe;
            if (is_unsafe) {
                break;
            }
        }
        if (is_unsafe) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02x", text[i]);
            encoded += hex;
        } else {
            encoded += text[i];
        }
    }

    return encoded;
}

std::vector<std::filesystem::path> urilist_parse(const std::string& data)
{
    std::vector<std::filesystem::path> paths;

    size_t start = 0;
    while (true) {
        std::string line;
        const size_t end = data.find(CRLF, start);
        if (end == std::string::npos) {
            line = data.substr(start);
        } else {
            line = data.substr(start, end - start);
        }
        if (line.starts_with(URI_FILE)) {
            const std::string path = decode_safe(line.substr(URI_FILE_LEN));
            if (!path.empty()) {
                paths.emplace_back(path);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + CRLF_LEN;
    }

    return paths;
}

std::string urilist_create(const std::filesystem::path& path)
{
    const std::string safe_path = encode_safe(path);

    std::string uri;
    uri.reserve(URI_FILE_LEN + safe_path.length() + CRLF_LEN);
    uri = URI_FILE;
    uri += safe_path;
    uri += CRLF;

    return uri;
}

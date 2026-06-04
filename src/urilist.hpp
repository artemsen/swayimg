// SPDX-License-Identifier: MIT
// URI list parser and composer (RFC 2483).
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <filesystem>
#include <string>
#include <vector>

/** MIME type name for URI list. */
constexpr const char* URI_LIST_MIME = "text/uri-list";

/**
 * Parse URI list.
 * @param URI list conten
 * @return array with paths to the host files
 */
std::vector<std::filesystem::path> urilist_parse(const std::string& data);

/**
 * Create URI list with a single path.
 * @param path path to the host file
 * @return URI list content
 */
std::string urilist_create(const std::filesystem::path& path);

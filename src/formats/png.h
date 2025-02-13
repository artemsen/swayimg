// SPDX-License-Identifier: MIT
// PNG format encoder/decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../loader.h"

// PNG decoder implementation
enum loader_status decode_png(struct image* ctx, const uint8_t* data,
                              size_t size);

/**
 * Export pixel map to PNG file.
 * @param pm source image instance
 * @param info meta data to add to the file
 * @param path output file
 * @return true if image saved successfully
 */
bool export_png(const struct pixmap* pm, const struct image_info* info,
                const char* path);

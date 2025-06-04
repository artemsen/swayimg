// SPDX-License-Identifier: MIT
// PNG format encoder/decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "loader.h"

// PNG decoder implementation
enum image_status decode_png(struct imgdata* img, const uint8_t* data,
                             size_t size);

/**
 * Export pixel map to PNG file.
 * @param pm source image instance
 * @param info meta data to add to the file
 * @return array with encoded data, NULL on errors
 */
struct array* export_png(const struct pixmap* pm, const struct array* info);

// SPDX-License-Identifier: MIT
// JPEG format support.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../loader.h"

/**
 * Encode JPEG to memory buffer.
 * @param image source image instance
 * @param data JPEG data buffer, the caller should free it after use
 * @param size size of JPEG data buffer
 * @return true if image saved successfully
 */
bool encode_jpeg(const struct image* image, uint8_t** data, size_t* size);

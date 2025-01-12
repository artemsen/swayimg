// SPDX-License-Identifier: MIT
// PNG format encoder/decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../loader.h"

// PNG decoder implementation
enum loader_status decode_png(struct image* ctx, const uint8_t* data,
                              size_t size);
/**
 * Encode PNG to memory buffer.
 * @param image source image instance
 * @param data PNG data buffer, the caller should free it after use
 * @param size size of PNG data buffer
 * @return true if image saved successfully
 */
bool encode_png(const struct image* ctx, uint8_t** data, size_t* size);

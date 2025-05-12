// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../image.h"

/**
 * Image loader function prototype, implemented by decoders.
 * @param img image data container
 * @param data raw data to decode
 * @param size data size in bytes
 * @return loader status
 */
typedef enum image_status (*image_decoder)(struct imgdata* img,
                                           const uint8_t* data, size_t size);

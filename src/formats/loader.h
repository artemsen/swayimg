// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "../image.h"

/**
 * Image loader function prototype, implemented by decoders.
 * @param image target image instance
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
typedef enum loader_status (*image_decoder)(struct image* image,
                                            const uint8_t* data, size_t size);

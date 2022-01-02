// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Read and handle EXIF data.
 * @param[in] img target image instance
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 */
void read_exif(image_t* img, const uint8_t* data, size_t size);

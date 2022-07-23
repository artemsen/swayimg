// SPDX-License-Identifier: MIT
// EXIT reader.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/**
 * Read and handle EXIF data.
 * @param img target image context
 * @param data image file data
 * @param size size of image data in bytes
 */
void process_exif(struct image* img, const uint8_t* data, size_t size);

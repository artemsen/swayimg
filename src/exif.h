// SPDX-License-Identifier: MIT
// EXIF reader.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "image.h"

/**
 * Read and handle EXIF data.
 * @param img target image container
 * @param data image file data
 * @param size size of image data in bytes
 */
void process_exif(struct imgdata* img, const uint8_t* data, size_t size);

// SPDX-License-Identifier: MIT
// EXIF reader.
// Copyright (C) 2026 Josef Litoš <invisiblemancz@gmail.com>

#pragma once

#include "image.h"

/**
 * Read and handle EXIF data.
 * @param img target image container
 * @param arg_query string of arguments to be passed to exiftool
 * i.e. which properties to get -> "-all" or "-Aperture -Location"...
 */
void query_exiftool(const struct image* img, const char* arg_query);

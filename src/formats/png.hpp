// SPDX-License-Identifier: MIT
// PNG format encoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../pixmap.hpp"

namespace Png {

/**
 * Export pixel map to PNG format.
 * @param pm source image instance
 * @return image in PNG format
 */
std::vector<uint8_t> encode(const Pixmap& pm);

};

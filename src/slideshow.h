// SPDX-License-Identifier: MIT
// Slide show mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "mode.h"

/**
 * Initialize global slideshow context.
 * @param cfg config instance
 * @param handlers mode handlers
 */
void slideshow_init(const struct config* cfg, struct mode* handlers);

/**
 * Destroy global slideshow context.
 */
void slideshow_destroy(void);

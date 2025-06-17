// SPDX-License-Identifier: MIT
// Image viewer mode.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "mode.h"

/**
 * Initialize global viewer context.
 * @param cfg config instance
 * @param handlers mode handlers
 */
void viewer_init(const struct config* cfg, struct mode* handlers);

/**
 * Destroy global viewer context.
 */
void viewer_destroy(void);

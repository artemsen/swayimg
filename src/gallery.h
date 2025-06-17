// SPDX-License-Identifier: MIT
// Gallery mode.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "config.h"
#include "mode.h"

/**
 * Initialize global gallery context.
 * @param cfg config instance
 * @param handlers mode handlers
 */
void gallery_init(const struct config* cfg, struct mode* handlers);

/**
 * Destroy global gallery context.
 */
void gallery_destroy(void);

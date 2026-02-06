// SPDX-License-Identifier: MIT
// Logging.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "log.hpp"

namespace Log {

/** Logging level: allow debug messages. */
static bool verbose = false;

void enable_verbose()
{
    verbose = true;
}

bool verbose_enabled()
{
    return verbose;
}

}; // namespace Log

// SPDX-License-Identifier: MIT
// Inter Process Communication: application control via unix socket.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stdbool.h>

/**
 * Start IPC server.
 * @param path path to the socket file
 * @return false on error
 */
bool ipc_start(const char* path);

/**
 * Stop IPC server.
 */
void ipc_stop(void);

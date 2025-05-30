// SPDX-License-Identifier: MIT
// Shell command executor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "array.h"

// Special error code: child process timed out
#define SHELLCMD_TIMEOUT 256

/**
 * Execute command in shell.
 * @param cmd command to execute
 * @param out pointer to stdout buffer
 * @param err pointer to stderr buffer
 * @return child process exit code or negative number on errors
 */
int shellcmd_exec(const char* cmd, struct array** out, struct array** err);

/**
 * Compose command from expression.
 * @param expr command expression
 * @param path file path to substitute into expression
 * @return result command, caller should free the buffer
 */
char* shellcmd_expr(const char* expr, const char* path);

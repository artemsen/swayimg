// SPDX-License-Identifier: MIT
// Shell command executor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "array.h"

/**
 * Execute command in shell.
 * @param cmd command to execute
 * @param out pointer to stdout buffer
 * @return subprocess exit code
 */
int shellcmd_exec(const char* cmd, struct array** out);

/**
 * Compose command from expression.
 * @param expr command expression
 * @param path file path to substitute into expression
 * @return result command, caller should free the buffer
 */
char* shellcmd_expr(const char* expr, const char* path);

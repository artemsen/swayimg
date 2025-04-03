// SPDX-License-Identifier: MIT
// Shell command executor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Execute command in shell.
 * @param cmd command to execute
 * @param out pointer to stdout buffer, caller should free the buffer
 * @param sz size of output buffer
 * @return subprocess exit code
 */
int shellcmd_exec(const char* cmd, uint8_t** out, size_t* sz);

/**
 * Construct command from expression and execute it in shell.
 * @param expr command expression
 * @param path file path to substitute into expression
 * @param out pointer to stdout buffer, caller should free the buffer
 * @return subprocess exit code
 */
int shellcmd_expr(const char* expr, const char* path, char** out);

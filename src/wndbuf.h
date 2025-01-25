// SPDX-License-Identifier: MIT
// Wayland window surface buffer.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#pragma once

#include "pixmap.h"

#include <wayland-client-protocol.h>

/**
 * Create window buffer.
 * @param shm wayland shared memory interface
 * @param width,height buffer size in pixels
 * @return wayland buffer on NULL on errors
 */
struct wl_buffer* wndbuf_create(struct wl_shm* shm, size_t width,
                                size_t height);
/**
 * Get pixel map assiciated with the buffer.
 * @param buffer wayland buffer
 * @return pointer to pixel map assiciated with the buffer
 */
struct pixmap* wndbuf_pixmap(struct wl_buffer* buffer);

/**
 * Free window buffer.
 * @param buffer wayland buffer to free
 */
void wndbuf_free(struct wl_buffer* buffer);

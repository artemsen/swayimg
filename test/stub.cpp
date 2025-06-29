// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "application.h"

// stubs for linker (application and ui are not included to tests)
void app_watch(int, fd_callback, void*) { }
void app_reload() { }
void app_redraw() { }
void app_on_resize() { }
void app_on_keyboard(xkb_keysym_t, uint8_t) { }
void app_on_imglist(struct image*, enum fsevent) { }
void app_on_drag(int, int) { }
void app_exit(int) { }
void app_on_load(struct image*, size_t) { }

} // extern "C"

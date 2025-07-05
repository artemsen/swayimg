// SPDX-License-Identifier: MIT
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

extern "C" {
#include "application.h"

// stubs for linker (application and ui are not included to tests)
void app_on_imglist(struct image*, enum fsevent) { }

} // extern "C"

// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

config_t config = {
    .scale = scale_fit_or100,
    .background = BACKGROUND_GRID,
};

void load_config(void)
{
    // todo
}

bool set_scale(const char* value)
{
    if (strcmp(value, "default") == 0) {
        config.scale = scale_fit_or100;
    } else if (strcmp(value, "fit") == 0) {
        config.scale = scale_fit_window;
    } else if (strcmp(value, "real") == 0) {
        config.scale = scale_100;
    } else {
        return false;
    }
    return true;
}

bool set_background(const char* value)
{
    uint32_t bkg;

    if (strcmp(value, "grid") == 0) {
        bkg = BACKGROUND_GRID;
    } else {
        bkg = strtoul(value, NULL, 16);
        if (bkg > 0x00ffffff || errno == ERANGE ||
            (bkg == 0 && errno == EINVAL)) {
            return false;
        }
    }

    config.background = bkg;
    return true;
}

bool set_geometry(const char* value)
{
    rect_t window;
    int* nums[] = { &window.x, &window.y, &window.width, &window.height };
    const char* ptr = value;
    size_t idx;

    for (idx = 0; *ptr && idx < sizeof(nums) / sizeof(nums[0]); ++idx) {
        *nums[idx] = atoi(ptr);
        // skip digits
        while (isdigit(*ptr)) {
            ++ptr;
        }
        // skip delimiter
        while (*ptr && !isdigit(*ptr)) {
            ++ptr;
        }
    }

    if (window.width <= 0 || window.height <= 0 ||
        idx != sizeof(nums) / sizeof(nums[0])) {
        return false;
    }

    config.window = window;
    return true;
}

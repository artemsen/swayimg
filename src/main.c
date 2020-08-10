// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "viewer.h"
#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/**
 * Print help usage info.
 */
static void print_help(void)
{
    puts("Usage: " APP_NAME " [OPTION...] FILE");
    puts("  -a, --appid=NAME         Set application id");
    puts("  -g, --geometry=X,Y,W,H   Set window geometry");
    puts("  -s, --scale=PERCENT      Set initial image scale");
    puts("  -v, --version            Print version info and exit");
    puts("  -h, --help               Print this help and exit");
}

/**
 * Print version info.
 */
static void print_version(void)
{
    const char* formats = "Supported formats: png"
#ifdef HAVE_LIBJPEG
        ", jpeg"
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBGIF
        ", gif"
#endif // HAVE_LIBGIF
    ".";
    puts(APP_NAME " version " APP_VERSION ".");
    puts(formats);
}

/**
 * Parse geometry (position and size) from string "x,y,width,height".
 * @param[in] arg argument to parse
 * @return geometry instance or NULL on errors
 */
struct rect* parse_rect(const char* arg)
{
    struct rect* rect = malloc(sizeof(struct rect));
    if (!rect) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    int* nums[] = { &rect->x, &rect->y, &rect->width, &rect->height };
    size_t i;
    const char* ptr = arg;
    for (i = 0; i < sizeof(nums) / sizeof(nums[0]); ++i) {
        *nums[i] = atoi(ptr);
        ptr = strchr(ptr, ',');
        if (!ptr) {
            break;
        }
        ++ptr; // skip delimiter
    }

    if (i != sizeof(nums) / sizeof(nums[0]) - 1) {
        free(rect);
        rect = NULL;
        fprintf(stderr, "Invalid argument format: %s\n", arg);
        fprintf(stderr, "Expected geometry, e.g. \"0,100,200,1000\"\n");
    }

    return rect;
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    struct viewer view;
    memset(&view, 0, sizeof(view));

    const struct option long_opts[] = {
        { "appid",    required_argument, NULL, 'a' },
        { "geometry", required_argument, NULL, 'g' },
        { "scale",    required_argument, NULL, 's' },
        { "version",  no_argument,       NULL, 'v' },
        { "help",     no_argument,       NULL, 'h' },
        { NULL,       0,                 NULL,  0  }
    };
    const char* short_opts = "a:g:s:vh";

    opterr = 0; // prevent native error messages

    // parse arguments
    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 'a':
                view.app_id = optarg;
                break;
            case 'g':
                view.wnd = parse_rect(optarg);
                if (!view.wnd) {
                    return EXIT_FAILURE;
                }
                break;
            case 's':
                view.scale = atoi(optarg);
                break;
            case 'v':
                print_version();
                return EXIT_SUCCESS;
            case 'h':
                print_help();
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "Invalid argument: %s\n", argv[optind - 1]);
                return EXIT_FAILURE;
        }
    }

    if (optind == argc) {
        fprintf(stderr, "File name expected, use `" APP_NAME " --help`.\n");
        return EXIT_FAILURE;
    }
    view.file = argv[optind];
    if (!*view.file) {
        fprintf(stderr, "File name can not be empty\n");
        return EXIT_FAILURE;
    }

    const bool rc = show_image(&view);

    if (view.wnd) {
        free(view.wnd);
    }

    return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

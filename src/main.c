// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "config.h"
#include "loader.h"
#include "viewer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/**
 * Print help usage info.
 */
static void print_help(void)
{
    puts("Usage: " APP_NAME " [OPTION...] FILE...");
    puts("  -f, --fullscreen         Full screen mode");
    puts("  -g, --geometry=X,Y WxH   Set window geometry");
    puts("  -s, --scale=PERCENT      Set initial image scale");
    puts("  -i, --info               Show image properties");
    puts("  -v, --version            Print version info and exit");
    puts("  -h, --help               Print this help and exit");
}

/**
 * Print version info.
 */
static void print_version(void)
{
    const char* formats = "Supported formats: png, bmp"
#ifdef HAVE_LIBJPEG
        ", jpeg"
#endif // HAVE_LIBJPEG
#ifdef HAVE_LIBGIF
        ", gif"
#endif // HAVE_LIBGIF
#ifdef HAVE_LIBRSVG
        ", svg"
#endif // HAVE_LIBRSVG
#ifdef HAVE_LIBWEBP
        ", webp"
#endif // HAVE_LIBWEBP
#ifdef HAVE_LIBAVIF
        ", avif"
#endif // HAVE_LIBAVIF
    ".";
    puts(APP_NAME " version " APP_VERSION ".");
    puts(formats);
}

/**
 * Validate that geometry has been read correctly
 */
bool good_geometry(const char* ptr, const char* arg)
{
    if (!ptr) {
        fprintf(stderr, "Invalid window geometry: %s\n", arg);
        fprintf(stderr, "Expected geometry, e.g. \"0,100 200x1000\"\n");
        return false;
    }
    return true;
}

/**
 * Parse geometry (position and size) from string "X,Y WIDTHxHEIGHT".
 * @param[in] arg argument to parse
 * @param[out] rect parsed geometry
 * @return false if rect is invalid
 */
bool parse_rect(const char* arg, struct rect* rect)
{
    const char* ptr = arg;
    rect->x = atoi(ptr);
    ptr = strchr(ptr, ',');
    if (!good_geometry(ptr,arg)) {
        return false;
    }
    ++ptr;
    rect->y = atoi(ptr);
    ptr = strchr(ptr, ' ');
    if (!good_geometry(ptr,arg)) {
        return false;
    }
    ++ptr;
    rect->width = atoi(ptr);
    ptr = strchr(ptr, 'x');
    if (!good_geometry(ptr,arg)) {
        return false;
    }
    ++ptr;
    rect->height = atoi(ptr);

    if (rect->width <= 0 || rect->height <= 0) {
        fprintf(stderr, "Invalid window size: %s\n", arg);
        return false;
    }

    return true;
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    const struct option long_opts[] = {
        { "fullscreen", no_argument,       NULL, 'f' },
        { "geometry",   required_argument, NULL, 'g' },
        { "scale",      required_argument, NULL, 's' },
        { "info",       no_argument,       NULL, 'i' },
        { "version",    no_argument,       NULL, 'v' },
        { "help",       no_argument,       NULL, 'h' },
        { NULL,         0,                 NULL,  0  }
    };
    const char* short_opts = "fg:s:ivh";

    opterr = 0; // prevent native error messages

    // parse arguments
    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 'f':
                viewer.fullscreen = true;
                break;
            case 'g':
                if (!parse_rect(optarg, &viewer.wnd)) {
                    return EXIT_FAILURE;
                }
                break;
            case 's':
                viewer.scale = atoi(optarg);
                break;
            case 'i':
                viewer.show_info = true;
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

    if (viewer.fullscreen && viewer.wnd.width) {
        fprintf(stderr, "Incompatible arguments: "
                        "can not set geometry for full screen mode\n");
        return EXIT_FAILURE;
    }

    if (optind == argc) {
        fprintf(stderr, "No files specified for viewing, "
                        "use '-' to read image data from stdin.\n");
        return EXIT_FAILURE;
    }

    const int num_files = argc - optind;
    if (num_files == 1 && strcmp(argv[optind], "-") == 0) {
        loader_init(NULL, 0);
    } else {
        loader_init((const char**)&argv[optind], (size_t)num_files);
    }

    const bool rc = run_viewer();

    loader_free();

    return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

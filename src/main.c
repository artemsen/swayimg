// SPDX-License-Identifier: MIT
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "buildcfg.h"
#include "config.h"
#include "image.h"
#include "viewer.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Print help usage info.
 */
static void print_help(void)
{
    // clang-format off
    puts("Usage: " APP_NAME " [OPTION...] FILE...");
    puts("  -f, --fullscreen         Full screen mode");
    puts("  -s, --scale=TYPE         Set initial image scale: default, fit, or real");
    puts("  -b, --background=XXXXXX  Set background color as hex RGB");
    puts("  -g, --geometry=X,Y,W,H   Set window geometry");
    puts("  -i, --info               Show image properties");
    puts("  -v, --version            Print version info and exit");
    puts("  -h, --help               Print this help and exit");
    // clang-format on
}

/**
 * Print version info.
 */
static void print_version(void)
{
    puts(APP_NAME " version " APP_VERSION ".");
    printf("Supported formats: %s.\n", supported_formats());
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    const char** files = NULL;
    size_t files_num = 0;

    // clang-format off
    const struct option long_opts[] = {
        { "fullscreen", no_argument,       NULL, 'f' },
        { "scale",      required_argument, NULL, 's' },
        { "background", required_argument, NULL, 'b' },
        { "geometry",   required_argument, NULL, 'g' },
        { "info",       no_argument,       NULL, 'i' },
        { "version",    no_argument,       NULL, 'v' },
        { "help",       no_argument,       NULL, 'h' },
        { NULL,         0,                 NULL,  0  }
    };
    const char* short_opts = "fs:b:g:ivh";
    // clang-format on

    // default config
    load_config();

    opterr = 0; // prevent native error messages

    // parse arguments
    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 'f':
                config.fullscreen = true;
                break;
            case 's':
                if (!set_scale(optarg)) {
                    fprintf(stderr, "Invalid scale: %s\n", optarg);
                    fprintf(stderr, "Expected 'default', 'fit', or 'real'.\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'b':
                if (!set_background(optarg)) {
                    fprintf(stderr, "Invalid background: %s\n", optarg);
                    fprintf(stderr, "Expected 'grid' or RGB hex value.\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'g':
                if (!set_geometry(optarg)) {
                    fprintf(stderr, "Invalid window geometry: %s\n", optarg);
                    fprintf(stderr, "Expected X,Y,W,H format.\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'i':
                config.show_info = true;
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

    if (config.fullscreen && config.window.width) {
        fprintf(stderr,
                "Incompatible arguments: "
                "can not set geometry for full screen mode\n");
        return EXIT_FAILURE;
    }

    if (optind == argc) {
        fprintf(stderr,
                "No files specified for viewing, "
                "use '-' to read image data from stdin.\n");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[optind], "-") != 0) {
        files = (const char**)&argv[optind];
        files_num = (size_t)(argc - optind);
    }

    return run_viewer(files, files_num) ? EXIT_SUCCESS : EXIT_FAILURE;
}

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

struct cmdarg;
typedef struct cmdarg cmdarg_t;

/** Command line options. */
struct cmdarg {
    const char short_opt; ///< Short option character
    const char* long_opt; ///< Long option name
    const char* format;   ///< Format description
    const char* help;     ///< Help string
};

// clang-format off
static const cmdarg_t arguments[] = {
    { 'f', "fullscreen", NULL,      "Show image in full screen mode" },
    { 's', "scale",      "TYPE",    "Set initial image scale: default, fit, or real" },
    { 'b', "background", "XXXXXX",  "Set background color as hex RGB" },
    { 'g', "geometry",   "X,Y,W,H", "Set window geometry" },
    { 'i', "info",       NULL,      "Show image meta information (name, EXIF, etc)" },
    { 'c', "class",      "NAME",    "Set window class/app_id" },
    { 'n', "no-sway",    NULL,      "Disable integration with Sway WM" },
    { 'v', "version",    NULL,      "Print version info and exit" },
    { 'h', "help",       NULL,      "Print this help and exit" }
};
// clang-format on

/**
 * Print usage info.
 */
static void print_help(void)
{
    char buf_lopt[32];
    puts("Usage: " APP_NAME " [OPTION...] [FILE...]");
    for (size_t i = 0; i < sizeof(arguments) / sizeof(arguments[0]); ++i) {
        const cmdarg_t* arg = &arguments[i];
        strcpy(buf_lopt, arg->long_opt);
        if (arg->format) {
            strcat(buf_lopt, "=");
            strcat(buf_lopt, arg->format);
        }
        printf("  -%c, --%-18s %s\n", arg->short_opt, buf_lopt, arg->help);
    }
}

/**
 * Parse command line arguments into configuration instance.
 * @param[in] argc number of arguments to parse
 * @param[in] argv arguments array
 * @param[out] cfg target configuration instance
 * @return index of the first non option argument, or -1 if error, or 0 to exit
 */
static int parse_cmdargs(int argc, char* argv[], config_t* cfg)
{
    struct option options[1 + (sizeof(arguments) / sizeof(arguments[0]))];
    char short_opts[(sizeof(arguments) / sizeof(arguments[0])) * 2];
    char* short_opts_ptr = short_opts;
    int opt;

    for (size_t i = 0; i < sizeof(arguments) / sizeof(arguments[0]); ++i) {
        const cmdarg_t* arg = &arguments[i];
        // compose array of option structs
        options[i].name = arg->long_opt;
        options[i].has_arg = arg->format ? required_argument : no_argument;
        options[i].flag = NULL;
        options[i].val = arg->short_opt;
        // compose short options string
        *short_opts_ptr++ = arg->short_opt;
        if (arg->format) {
            *short_opts_ptr++ = ':';
        }
    }
    // add terminations
    *short_opts_ptr = 0;
    memset(&options[(sizeof(arguments) / sizeof(arguments[0])) - 1], 0,
           sizeof(struct option));

    opterr = 0; // prevent native error messages

    // parse arguments
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                cfg->fullscreen = true;
                cfg->sway_wm = false;
                break;
            case 's':
                if (!set_scale_config(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'b':
                if (!set_background_config(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'g':
                if (!set_geometry_config(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'i':
                cfg->show_info = true;
                break;
            case 'c':
                if (!set_appid_config(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'n':
                cfg->sway_wm = false;
                break;
            case 'v':
                puts(APP_NAME " version " APP_VERSION ".");
                printf("Supported formats: %s.\n", supported_formats());
                return 0;
            case 'h':
                print_help();
                return 0;
            default:
                fprintf(stderr, "Invalid argument: %s\n", argv[optind - 1]);
                return -1;
        }
    }

    return optind;
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    int rc;
    config_t* cfg = NULL;
    file_list_t* files = NULL;
    bool recursive = true;
    int num_files;
    int index;

    // initialize config with default values
    cfg = init_config();
    if (!cfg) {
        rc = EXIT_FAILURE;
        goto done;
    }

    // parse command arguments
    index = parse_cmdargs(argc, argv, cfg);
    if (index == 0) {
        rc = EXIT_SUCCESS;
        goto done;
    }
    if (index < 0) {
        rc = EXIT_FAILURE;
        goto done;
    }
    check_config(cfg);

    // compose file list
    num_files = argc - index;
    if (num_files == 0) {
        // not input files specified, use current directory
        const char* curr_dir = ".";
        files = init_file_list(&curr_dir, 1, recursive);
        if (!files) {
            fprintf(stderr, "No image files found in the current directory\n");
            rc = EXIT_FAILURE;
            goto done;
        }
    } else if (num_files == 1 && strcmp(argv[index], "-") == 0) {
        // reading from pipe
        files = NULL;
    } else {
        files = init_file_list((const char**)&argv[index], (size_t)(num_files),
                               recursive);
        if (!files) {
            fprintf(stderr, "Unable to compose file list from input args\n");
            rc = EXIT_FAILURE;
            goto done;
        }
    }

    // run viewer, finally
    rc = run_viewer(cfg, files) ? EXIT_SUCCESS : EXIT_FAILURE;

done:
    free_config(cfg);
    free_file_list(files);

    return rc;
}

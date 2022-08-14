// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "buildcfg.h"
#include "config.h"
#include "formats/loader.h"
#include "image.h"
#include "sway.h"
#include "viewer.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Command line options. */
struct cmdarg {
    const char short_opt; ///< Short option character
    const char* long_opt; ///< Long option name
    const char* format;   ///< Format description
    const char* help;     ///< Help string
};

// clang-format off
static const struct cmdarg arguments[] = {
    { 'o', "order",      "ORDER",   "set sort order for image list: none/[alpha]/random" },
    { 'r', "recursive",  NULL,      "read directories recursively" },
    { 'a', "all",        NULL,      "open all files from the same directory" },
    { 'm', "mark",       NULL,      "enable marking mode" },
    { 'f', "fullscreen", NULL,      "show image in full screen mode" },
    { 's', "scale",      "SCALE",   "set initial image scale: [optimal]/fit/real" },
    { 'b', "background", "XXXXXX",  "set image background color: none/[grid]/RGB" },
    { 'w', "window",     "XXXXXX",  "set window background color: [none]/RGB" },
    { 'g', "geometry",   "X,Y,W,H", "set window geometry" },
    { 'i', "info",       NULL,      "show image meta information (name, EXIF, etc)" },
    { 'c', "class",      "NAME",    "set window class/app_id" },
    { 'n', "no-sway",    NULL,      "disable integration with Sway WM" },
    { 'v', "version",    NULL,      "print version info and exit" },
    { 'h', "help",       NULL,      "print this help and exit" }
};
// clang-format on

/**
 * Print usage info.
 */
static void print_help(void)
{
    char buf_lopt[32];
    puts("Usage: " APP_NAME " [OPTION]... [FILE]...");
    puts("Show images from FILE(s).");
    puts("If FILE is -, read standard input.");
    puts("If no FILE specified - read all files from the current directory.\n");
    puts("Mandatory arguments to long options are mandatory for short options "
         "too.");
    for (size_t i = 0; i < sizeof(arguments) / sizeof(arguments[0]); ++i) {
        const struct cmdarg* arg = &arguments[i];
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
 * @param argc number of arguments to parse
 * @param argv arguments array
 * @param cfg configuration instance
 * @return index of the first non option argument, or -1 if error, or 0 to exit
 */
static int parse_cmdargs(int argc, char* argv[], struct config* cfg)
{
    struct option options[1 + (sizeof(arguments) / sizeof(arguments[0]))];
    char short_opts[(sizeof(arguments) / sizeof(arguments[0])) * 2];
    char* short_opts_ptr = short_opts;
    int opt;

    for (size_t i = 0; i < sizeof(arguments) / sizeof(arguments[0]); ++i) {
        const struct cmdarg* arg = &arguments[i];
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

    // parse arguments
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                if (!config_set_order(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'r':
                cfg->recursive = true;
                break;
            case 'a':
                cfg->all_files = true;
                break;
            case 'm':
                cfg->mark_mode = true;
                break;
            case 'f':
                cfg->fullscreen = true;
                break;
            case 's':
                if (!config_set_scale(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'b':
                if (!config_set_background(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'w':
                if (!config_set_window(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'g':
                if (!config_set_geometry(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'i':
                cfg->show_info = true;
                break;
            case 'c':
                if (!config_set_appid(cfg, optarg)) {
                    return -1;
                }
                break;
            case 'n':
                cfg->sway_wm = false;
                break;
            case 'v':
                puts(APP_NAME " version " APP_VERSION ".");
                puts("https://github.com/artemsen/swayimg");
                printf("Supported formats: %s.\n", supported_formats());
                return 0;
            case 'h':
                print_help();
                return 0;
            default:
                return -1;
        }
    }

    return optind;
}

/**
 * Setup window position via Sway IPC.
 * @param cfg configuration instance
 */
static void sway_setup(struct config* cfg)
{
    bool sway_fullscreen = false;
    const int ipc = sway_connect();
    if (ipc != -1) {
        if (!cfg->geometry.width) {
            // get currently focused window state
            sway_current(ipc, &cfg->geometry, &sway_fullscreen);
        }
        cfg->fullscreen |= sway_fullscreen;
        if (!cfg->fullscreen && cfg->geometry.width) {
            sway_add_rules(ipc, cfg->app_id, cfg->geometry.x, cfg->geometry.y);
        }
        sway_disconnect(ipc);
    }
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    int rc;
    struct config* cfg = NULL;
    struct file_list* files = NULL;
    int num_files;
    int index;

    cfg = config_init();
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

    // check configuration
    if (cfg->geometry.width) {
        if (!cfg->sway_wm) {
            fprintf(stderr,
                    "Warning: unable to set window geometry without sway\n");
        }
        if (cfg->fullscreen) {
            fprintf(stderr,
                    "Warning: window geometry used in fullscreen mode\n");
        }
    }

    // compose file list
    num_files = argc - index;
    if (num_files == 1 && strcmp(argv[index], "-") == 0) {
        // reading from pipe, skip file list composing
    } else {
        files = flist_init((const char**)&argv[index], num_files, cfg);
        if (!files) {
            fprintf(stderr, "Unable to compose file list from input args\n");
            rc = EXIT_FAILURE;
            goto done;
        }
    }

    if (cfg->sway_wm && !cfg->fullscreen) {
        sway_setup(cfg);
    }

    rc = run_viewer(cfg, files) ? EXIT_SUCCESS : EXIT_FAILURE;

    if (cfg->mark_mode && files && rc == EXIT_SUCCESS) {
        flist_mark_print(files);
    }

done:
    flist_free(files);
    config_free(cfg);

    return rc;
}

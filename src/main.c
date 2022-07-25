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
    { 'S', "sort",       NULL,      "sort input files alphabetically" },
    { 'R', "random",     NULL,      "shuffle input file list in random mode" },
    { 'r', "recursive",  NULL,      "read directories recursively" },
    { 'f', "fullscreen", NULL,      "show image in full screen mode" },
    { 's', "scale",      "TYPE",    "set initial image scale: [optimal]/fit/real" },
    { 'b', "background", "XXXXXX",  "set image background color: none/[grid]/RGB" },
    { 'w', "frame",      "XXXXXX",  "set window background color: [none]/RGB" },
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
 * @param cfg target configuration instance
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

    opterr = 0; // prevent native error messages

    // parse arguments
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        switch (opt) {
            case 'S':
                cfg->order = cfgord_alpha;
                break;
            case 'R':
                cfg->order = cfgord_random;
                break;
            case 'r':
                cfg->recursive = true;
                break;
            case 'f':
                cfg->fullscreen = true;
                cfg->sway_wm = false;
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
                if (!config_set_frame(cfg, optarg)) {
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
                fprintf(stderr, "Invalid argument: %s\n", argv[optind - 1]);
                return -1;
        }
    }

    return optind;
}

/**
 * Compose file list from command line arguments.
 * @param cfg configuration instance
 * @param args array with command line arguments
 * @param num_files number of files in array
 * @return file list, NULL if error
 */
static struct file_list* create_file_list(struct config* cfg,
                                          const char* args[], size_t num_files)
{
    struct file_list* flist = NULL;

    if (num_files == 0) {
        // not input files specified, use current directory
        const char* curr_dir = ".";
        flist = flist_init(&curr_dir, 1, cfg->recursive);
        if (!flist) {
            fprintf(stderr, "No image files found in the current directory\n");
            return NULL;
        }
    } else {
        flist = flist_init(args, num_files, cfg->recursive);
        if (!flist) {
            fprintf(stderr, "Unable to compose file list from input args\n");
            return NULL;
        }
    }
    if (cfg->order == cfgord_alpha) {
        flist_sort(flist);
    } else if (cfg->order == cfgord_random) {
        flist_shuffle(flist);
    }

    return flist;
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
        if (!cfg->window.width) {
            // get currently focused window state
            sway_current(ipc, &cfg->window, &sway_fullscreen);
        }
        cfg->fullscreen |= sway_fullscreen;
        if (!cfg->fullscreen && cfg->window.width) {
            sway_add_rules(ipc, cfg->app_id, cfg->window.x, cfg->window.y);
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

    // initialize config with default values
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
    config_check(cfg);

    // compose file list
    num_files = argc - index;
    if (num_files == 1 && strcmp(argv[index], "-") == 0) {
        // reading from pipe, skip file list composing
    } else {
        files = create_file_list(cfg, (const char**)&argv[index], num_files);
        if (!files) {
            rc = EXIT_FAILURE;
            goto done;
        }
    }

    // setup integration with Sway WM
    if (cfg->sway_wm) {
        sway_setup(cfg);
    }

    // run viewer, finally
    rc = run_viewer(cfg, files) ? EXIT_SUCCESS : EXIT_FAILURE;

done:
    config_free(cfg);
    flist_free(files);

    return rc;
}

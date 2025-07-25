// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "application.h"
#include "array.h"
#include "buildcfg.h"
#include "config.h"
#include "image.h"

#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Command line arguments. */
struct cmdarg {
    const char short_opt; ///< Short option character
    const char* long_opt; ///< Long option name
    const char* format;   ///< Format description
    const char* help;     ///< Help string
};

// clang-format off
static const struct cmdarg arguments[] = {
    { 'g', "gallery",     NULL,    "start in gallery mode" },
    { 'l', "slideshow",   NULL,    "start in slideshow mode" },
    { 'F', "from-file",   NULL,    "interpret input files as text lists of image files" },
    { 'r', "recursive",   NULL,    "read directories recursively" },
    { 'o', "order",       "ORDER", "set sort order for image list" },
    { 's', "scale",       "SCALE", "set initial image scale" },
#ifdef HAVE_COMPOSITOR
    { 'p', "position",    "POS",   "(Sway/Hyprland only) set window position" },
#endif
    { 'w', "size",        "SIZE",  "set window size" },
    { 'f', "fullscreen",  NULL,    "show image in full screen mode" },
    { 'a', "class",       "NAME",  "set window class/app_id" },
    { 'i', "ipc",         "FILE",  "enable IPC server on unix socket" },
    { 'c', "config",      "S.K=V", "set configuration parameter: section.key=value" },
    { 'C', "config-file", "FILE",  "load config from file" },
    { 'v', "version",     NULL,    "print version info and exit" },
    { 'h', "help",        NULL,    "print this help and exit" },
};
// clang-format on

/**
 * Print usage info.
 */
static void print_help(void)
{
    puts("Usage: " APP_NAME " [OPTION]... [FILE]...");
    puts("Show images from FILE(s).");
    puts("If FILE is -, read standard input.");
    puts("If no FILE specified - read all files from the current directory.\n");
    puts("Mandatory arguments to long options are mandatory for short options "
         "too.");

    for (size_t i = 0; i < ARRAY_SIZE(arguments); ++i) {
        const struct cmdarg* arg = &arguments[i];
        char lopt[32] = { 0 };
        if (arg->format) {
            snprintf(lopt, sizeof(lopt), "%s=%s", arg->long_opt, arg->format);
        } else {
            strncpy(lopt, arg->long_opt, sizeof(lopt) - 1);
        }
        printf("  -%c, --%-16s %s\n", arg->short_opt, lopt, arg->help);
    }
}

/**
 * Print version info.
 */
static void print_version(void)
{
    puts(APP_NAME " version " APP_VERSION ".");
    puts("https://github.com/artemsen/swayimg");
    printf("Supported formats: %s.\n", image_formats());
}

/**
 * Parse command line arguments.
 * @param argc number of arguments to parse
 * @param argv arguments array
 * @return index of the first non option argument
 */
static int parse_cmdargs(int argc, char* argv[], struct config* cfg)
{
    bool defconfig = true;
    struct option options[1 + ARRAY_SIZE(arguments)];
    char short_opts[ARRAY_SIZE(arguments) * 2];
    char* short_opts_ptr = short_opts;
    int opt;

    if (!cfg) {
        exit(EXIT_FAILURE);
    }

    // compose array of option structs
    for (size_t i = 0; i < ARRAY_SIZE(arguments); ++i) {
        const struct cmdarg* arg = &arguments[i];
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
    memset(&options[ARRAY_SIZE(arguments)], 0, sizeof(struct option));

    // load custom config
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        if (opt == 'C') {
            if (!config_load(cfg, optarg)) {
                fprintf(stderr, "Unable to load config file \"%s\"\n", optarg);
                exit(EXIT_FAILURE);
            }
            defconfig = false;
            break;
        }
    }
    if (defconfig) {
        config_load(cfg, CFG_DEF_FILE);
    }

    // parse arguments
    optind = 1;
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        switch (opt) {
            case 'g':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_MODE, CFG_GALLERY);
                break;
            case 'l':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_MODE, CFG_SLIDESHOW);
                break;
            case 'F':
                config_set(cfg, CFG_LIST, CFG_LIST_FROMFILE, CFG_YES);
                break;
            case 'r':
                config_set(cfg, CFG_LIST, CFG_LIST_RECURSIVE, CFG_YES);
                break;
            case 'o':
                config_set(cfg, CFG_LIST, CFG_LIST_ORDER, optarg);
                break;
            case 's':
                config_set(cfg, CFG_VIEWER, CFG_VIEW_SCALE, optarg);
                break;
#ifdef HAVE_COMPOSITOR
            case 'p':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_POSITION, optarg);
                break;
#endif
            case 'w':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_SIZE, optarg);
                break;
            case 'f':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_SIZE, CFG_FULLSCREEN);
                break;
            case 'a':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_APP_ID, optarg);
                break;
            case 'i':
                config_set(cfg, CFG_GENERAL, CFG_GNRL_IPC, optarg);
                break;
            case 'c':
                if (!config_set_arg(cfg, optarg)) {
                    exit(EXIT_FAILURE);
                }
                break;
            case 'C':
                break;
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    return optind;
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    bool rc;
    struct config* cfg;
    int argn;

    setlocale(LC_ALL, "");

    cfg = config_create();
    argn = parse_cmdargs(argc, argv, cfg);

    srand(getpid());

    rc = app_init(cfg, (const char**)&argv[argn], argc - argn);
    config_free(cfg);

    if (rc) {
        rc = app_run();
        app_destroy();
    }

    return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

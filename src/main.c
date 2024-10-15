// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "application.h"
#include "buildcfg.h"
#include "config.h"
#include "imagelist.h"
#include "loader.h"
#include "ui.h"
#include "viewer.h"

#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Command line arguments. */
struct cmdarg {
    const char short_opt; ///< Short option character
    const char* long_opt; ///< Long option name
    const char* format;   ///< Format description
    const char* help;     ///< Help string
    const char* value;    ///< Argument value
};

// clang-format off
static struct cmdarg arguments[] = {
    { 'g', "gallery",    NULL,    "start in gallery mode", NULL },
    { 'r', "recursive",  NULL,    "read directories recursively", NULL },
    { 'o', "order",      "ORDER", "set sort order for image list: none/[alpha]/reverse/random", NULL },
    { 's', "scale",      "SCALE", "set initial image scale: [optimal]/fit/width/height/fill/real", NULL },
    { 'l', "slideshow",  NULL,    "activate slideshow mode on startup", NULL },
    { 'p', "position",   "POS",   "set window position [parent]/X,Y", NULL },
    { 'w', "size",       "SIZE",  "set window size: fullscreen/[parent]/image/W,H", NULL },
    { 'f', "fullscreen", NULL,    "show image in full screen mode", NULL },
    { 'a', "class",      "NAME",  "set window class/app_id", NULL },
    { 'c', "config",     "S.K=V", "set configuration parameter: section.key=value", NULL },
    { 'v', "version",    NULL,    "print version info and exit", NULL },
    { 'h', "help",       NULL,    "print this help and exit", NULL },
};
// clang-format on

/**
 * Get argument description by it short opt.
 * @param short_opt short opt to find
 * @return pointer to the argument description or NULL if not found
 */
static struct cmdarg* get_arg(char short_opt)
{
    for (size_t i = 0; i < ARRAY_SIZE(arguments); ++i) {
        struct cmdarg* arg = &arguments[i];
        if (arg->short_opt == short_opt) {
            return arg;
        }
    }
    return NULL;
}

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
        char lopt[32];
        if (arg->format) {
            snprintf(lopt, sizeof(lopt), "%s=%s", arg->long_opt, arg->format);
        } else {
            strncpy(lopt, arg->long_opt, sizeof(lopt) - 1);
        }
        printf("  -%c, --%-14s %s\n", arg->short_opt, lopt, arg->help);
    }
}

/**
 * Print version info.
 */
static void print_version(void)
{
    puts(APP_NAME " version " APP_VERSION ".");
    puts("https://github.com/artemsen/swayimg");
    printf("Supported formats: %s.\n", supported_formats);
}

/**
 * Parse command line arguments.
 * @param argc number of arguments to parse
 * @param argv arguments array
 * @return index of the first non option argument, or -1 if error
 */
static int parse_cmdargs(int argc, char* argv[])
{
    struct option options[1 + ARRAY_SIZE(arguments)];
    char short_opts[ARRAY_SIZE(arguments) * 2];
    char* short_opts_ptr = short_opts;
    int opt;

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

    // parse arguments
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        struct cmdarg* arg;

        if (opt == '?') {
            return -1;
        }
        arg = get_arg(opt);
        if (!arg) {
            return -1;
        }

        arg->value = arg->format ? optarg : arg->long_opt;
    }

    return optind;
}

/**
 * Load configuration.
 * @return pointer to config instnce or NULL on error
 */
static struct config* load_config(void)
{
    struct config* cfg = config_load();

    for (size_t i = 0; i < ARRAY_SIZE(arguments); ++i) {
        const struct cmdarg* arg = &arguments[i];
        if (!arg->value) {
            continue;
        }
        switch (arg->short_opt) {
            case 'g':
                config_set(&cfg, APP_CFG_SECTION, APP_CFG_MODE,
                           APP_MODE_GALLERY);
                break;
            case 'r':
                config_set(&cfg, IMGLIST_SECTION, IMGLIST_RECURSIVE, "yes");
                break;
            case 'o':
                config_set(&cfg, IMGLIST_SECTION, IMGLIST_ORDER, arg->value);
                break;
            case 's':
                config_set(&cfg, VIEWER_SECTION, VIEWER_SCALE, arg->value);
                break;
            case 'l':
                config_set(&cfg, VIEWER_SECTION, VIEWER_SLIDESHOW, "yes");
                break;
            case 'p':
                config_set(&cfg, APP_CFG_SECTION, APP_CFG_POSITION, arg->value);
                break;
            case 'w':
                config_set(&cfg, APP_CFG_SECTION, APP_CFG_SIZE, arg->value);
                break;
            case 'f':
                config_set(&cfg, APP_CFG_SECTION, APP_CFG_SIZE, APP_FULLSCREEN);
                break;
            case 'a':
                config_set(&cfg, APP_CFG_SECTION, APP_CFG_APP_ID, arg->value);
                break;
            case 'c':
                if (!config_set_arg(&cfg, arg->value)) {
                    fprintf(stderr,
                            "WARNING: Invalid config agrument: \"%s\"\n",
                            arg->value);
                }
                break;
        }
    }

    return cfg;
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    bool rc = false;
    struct config* cfg;
    int argn;

    setlocale(LC_ALL, "");

    // parse command line arguments
    argn = parse_cmdargs(argc, argv);
    if (argn < 0) {
        return EXIT_FAILURE;
    }
    if (get_arg('v')->value) {
        print_version();
        return EXIT_SUCCESS;
    }
    if (get_arg('h')->value) {
        print_help();
        return EXIT_SUCCESS;
    }

    cfg = load_config();
    rc = app_init(cfg, (const char**)&argv[argn], argc - argn);

    if (rc) {
        config_check(cfg);
    }
    config_free(cfg);

    if (rc) {
        rc = app_run();
        app_destroy();
    }

    return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

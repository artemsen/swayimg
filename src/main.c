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

/** Command line options. */
struct cmdarg {
    const char short_opt; ///< Short option character
    const char* long_opt; ///< Long option name
    const char* format;   ///< Format description
    const char* help;     ///< Help string
    const char* section;  ///< Section name of the param
    const char* key;      ///< Key of the param
    const char* value;    ///< Static value to set
};

// clang-format off
static const struct cmdarg arguments[] = {
    { 'g', "gallery",    NULL,    "start in gallery mode",
                                  APP_CFG_SECTION, APP_CFG_MODE, APP_MODE_GALLERY },
    { 'r', "recursive",  NULL,    "read directories recursively",
                                  IMGLIST_SECTION, IMGLIST_RECURSIVE, "yes" },
    { 'o', "order",      "ORDER", "set sort order for image list: none/[alpha]/reverse/random",
                                  IMGLIST_SECTION, IMGLIST_ORDER, NULL },
    { 's', "scale",      "SCALE", "set initial image scale: [optimal]/fit/width/height/fill/real",
                                  VIEWER_SECTION, VIEWER_SCALE, NULL },
    { 'l', "slideshow",  NULL,    "activate slideshow mode on startup",
                                  VIEWER_SECTION, VIEWER_SLIDESHOW, "yes" },
    { 'p', "position",   "POS",   "set window position [parent]/X,Y",
                                  APP_CFG_SECTION, APP_CFG_POSITION, NULL },
    { 'w', "size",       "SIZE",  "set window size: fullscreen/[parent]/image/W,H",
                                  APP_CFG_SECTION, APP_CFG_SIZE, NULL },
    { 'f', "fullscreen", NULL,    "show image in full screen mode",
                                  APP_CFG_SECTION, APP_CFG_SIZE, APP_FULLSCREEN },
    { 'a', "class",      "NAME",  "set window class/app_id",
                                  APP_CFG_SECTION, APP_CFG_APP_ID, NULL },
    { 'c', "config",     "S.K=V", "set configuration parameter: section.key=value",
                                  NULL, NULL, NULL },
    { 'v', "version",    NULL,    "print version info and exit", NULL, NULL, NULL },
    { 'h', "help",       NULL,    "print this help and exit", NULL, NULL, NULL }
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

    for (size_t i = 0; i < sizeof(arguments) / sizeof(arguments[0]); ++i) {
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
 * Parse command line arguments into configuration instance.
 * @param cfg config instance
 * @param argc number of arguments to parse
 * @param argv arguments array
 * @return index of the first non option argument, or -1 if error, or 0 to exit
 */
static int parse_cmdargs(struct config** cfg, int argc, char* argv[])
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
    memset(&options[(sizeof(arguments) / sizeof(arguments[0]))], 0,
           sizeof(struct option));

    // parse arguments
    while ((opt = getopt_long(argc, argv, short_opts, options, NULL)) != -1) {
        const struct cmdarg* arg;
        if (opt == '?') {
            return -1;
        }
        // get argument description
        arg = arguments;
        while (arg->short_opt != opt) {
            ++arg;
        }
        if (arg->section) {
            config_set(cfg, arg->section, arg->key,
                       arg->value ? arg->value : optarg);
            continue;
        }
        switch (opt) {
            case 'c':
                if (!config_set_arg(cfg, optarg)) {
                    fprintf(stderr, "Invalid config: \"%s\"\n", optarg);
                    return -1;
                }
                break;
            case 'v':
                puts(APP_NAME " version " APP_VERSION ".");
                puts("https://github.com/artemsen/swayimg");
                printf("Supported formats: %s.\n", supported_formats);
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
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    bool rc = false;
    struct config* cfg;
    int argn;

    setlocale(LC_ALL, "");

    cfg = config_load();
    argn = parse_cmdargs(&cfg, argc, argv);

    if (argn <= 0) { // invalid args or version/help print
        config_free(cfg);
        return (argn == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

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

// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "buildcfg.h"
#include "config.h"
#include "font.h"
#include "formats/loader.h"
#include "imagelist.h"
#include "sway.h"
#include "ui.h"
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
    { 'l', "slideshow",  NULL,      "activate slideshow mode on startup" },
    { 'f', "fullscreen", NULL,      "show image in full screen mode" },
    { 's', "scale",      "SCALE",   "set initial image scale: [optimal]/fit/fill/real" },
    { 'b', "background", "COLOR",   "set image background color: none/[grid]/RGB" },
    { 'w', "wndbkg",     "COLOR",   "set window background color: [none]/RGB" },
    { 'p', "wndpos",     "POS",     "set window position [parent]/X,Y" },
    { 'g', "wndsize",    "SIZE",    "set window size: [parent]/image/W,H" },
    { 'i', "info",       NULL,      "show image meta information (name, EXIF, etc)" },
    { 'e', "exec",       "CMD",     "set execution command" },
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
 * @return index of the first non option argument, or -1 if error, or 0 to exit
 */
static int parse_cmdargs(int argc, char* argv[])
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
        switch (opt) {
            case 'o':
                if (!config_set_order(optarg)) {
                    return -1;
                }
                break;
            case 'r':
                config.recursive = true;
                break;
            case 'a':
                config.all_files = true;
                break;
            case 'l':
                config.slideshow = true;
                break;
            case 'f':
                config.fullscreen = true;
                break;
            case 's':
                if (!config_set_scale(optarg)) {
                    return -1;
                }
                break;
            case 'b':
                if (!config_set_background(optarg)) {
                    return -1;
                }
                break;
            case 'w':
                if (!config_set_wndbkg(optarg)) {
                    return -1;
                }
                break;
            case 'p':
                if (!config_set_wndpos(optarg)) {
                    return -1;
                }
                break;
            case 'g':
                if (!config_set_wndsize(optarg)) {
                    return -1;
                }
                break;
            case 'i':
                config.show_info = true;
                break;
            case 'e':
                if (!config_set_exec_cmd(optarg)) {
                    return -1;
                }
                break;
            case 'c':
                if (!config_set_appid(optarg)) {
                    return -1;
                }
                break;
            case 'n':
                config.sway_wm = false;
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
 * Setup window position via Sway IPC.
 */
static void sway_setup(void)
{
    int ipc;
    struct rect wnd_parent;
    bool wnd_fullscreen = false;
    const bool absolute = config.geometry.x != SAME_AS_PARENT &&
        config.geometry.y != SAME_AS_PARENT;

    ipc = sway_connect();
    if (ipc == INVALID_SWAY_IPC) {
        return;
    }

    if (sway_current(ipc, &wnd_parent, &wnd_fullscreen)) {
        config.fullscreen |= wnd_fullscreen;
        if (config.geometry.x == SAME_AS_PARENT &&
            config.geometry.y == SAME_AS_PARENT) {
            config.geometry.x = wnd_parent.x;
            config.geometry.y = wnd_parent.y;
        }
        if (config.geometry.width == SAME_AS_PARENT &&
            config.geometry.height == SAME_AS_PARENT) {
            config.geometry.width = wnd_parent.width;
            config.geometry.height = wnd_parent.height;
        }
    }

    if (!config.fullscreen) {
        sway_add_rules(ipc, config.app_id, config.geometry.x, config.geometry.y,
                       absolute);
    }

    sway_disconnect(ipc);
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    bool rc = false;
    int index;

    config_init();

    // parse command arguments
    index = parse_cmdargs(argc, argv);
    if (index == 0) {
        rc = true;
        goto done;
    }
    if (index < 0) {
        goto done;
    }

    // compose file list
    if (!image_list_init((const char**)&argv[index], argc - index)) {
        fprintf(stderr, "No images to view, exit\n");
        goto done;
    }

    // load font
    font_init();

    // set window size form the first image
    if (config.geometry.width == SAME_AS_IMAGE ||
        config.geometry.height == SAME_AS_IMAGE) {
        struct image_entry first = image_list_current();
        config.geometry.width = first.image->frames[0].width;
        config.geometry.height = first.image->frames[0].height;
    }

    if (config.sway_wm && !config.fullscreen) {
        sway_setup();
    }

    // no sway or fullscreen
    if (config.geometry.width == SAME_AS_PARENT &&
        config.geometry.height == SAME_AS_PARENT) {
        struct image_entry first = image_list_current();
        config.geometry.width = first.image->frames[0].width;
        config.geometry.height = first.image->frames[0].height;
    }

    // initialize ui
    if (!ui_init()) {
        goto done;
    }

    // create viewer
    viewer_init();

    // run main loop
    rc = ui_run();

done:
    ui_free();
    viewer_free();
    font_free();
    image_list_free();
    config_free();

    return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

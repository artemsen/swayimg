// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"
#include "buildconf.hpp"
#include "imageloader.hpp"
#include "log.hpp"

#include <getopt.h>
#include <unistd.h>

#include <clocale>
#include <cstdlib>
#include <format>

/** Command line arguments. */
struct cmdarg {
    const char short_opt; ///< Short option character
    const char* long_opt; ///< Long option name
    const char* format;   ///< Format description
    const char* help;     ///< Help string
};
static constexpr std::array arguments = std::to_array<cmdarg>({
    { 'g', "gallery",    nullptr, "start in gallery mode"                    },
    { 's', "slideshow",  nullptr, "start in slideshow mode"                  },
    { 'l', "from-file",  nullptr, "load file list from file"                 },
#ifdef HAVE_COMPOSITOR
    { 'p', "position",   "XxY",   "(Sway/Hyprland only) set window position" },
#endif
    { 'w', "size",       "WxH",   "set preferable window size"               },
    { 'f', "fullscreen", nullptr, "show image in full screen mode"           },
    { 'a', "class",      "NAME",  "set window class/app_id"                  },
    { 'c', "config",     "FILE",  "load config from file"                    },
    { 'V', "verbose",    nullptr, "verbose output"                           },
    { 'v', "version",    nullptr, "print version info and exit"              },
    { 'h', "help",       nullptr, "print this help and exit"                 },
});

/**
 * Get short and long options in getopt format.
 * @return short and long options in getopt format.
 */
static std::tuple<std::string, std::vector<option>> get_opts()
{
    std::string short_opts;
    short_opts.reserve(arguments.size() * 2);
    std::vector<option> long_opts;
    long_opts.reserve(arguments.size() + 1);

    // fill options
    for (auto& it : arguments) {
        short_opts += it.short_opt;
        if (it.format) {
            short_opts += ':';
        }
        long_opts.push_back({
            it.long_opt,
            it.format ? required_argument : no_argument,
            nullptr,
            it.short_opt,
        });
    }
    long_opts.push_back({});

    return std::make_tuple(short_opts, long_opts);
}

/**
 * Print usage info.
 */
static void print_help()
{
    puts("Usage: swayimg [OPTION]... [FILE]...");
    puts("Show images from FILE(s).");
    puts("If FILE is -, read standard input.");
    puts("If no FILE specified - read all files from the current directory.\n");
    puts("Mandatory arguments to long options are mandatory for short options "
         "too.");

    for (auto& it : arguments) {
        std::string lopt;
        if (it.format) {
            lopt = std::format("{}={}", it.long_opt, it.format);
        } else {
            lopt = it.long_opt;
        }
        printf("  -%c, --%-16s %s\n", it.short_opt, lopt.c_str(), it.help);
    }
}

/**
 * Print version info.
 */
static void print_version()
{
    puts("swayimg version " APP_VERSION ".");
    puts("https://github.com/artemsen/swayimg");
    printf("Supported formats: %s.\n", ImageLoader::format_list().c_str());
}

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    std::setlocale(LC_ALL, "");
    std::srand(getpid());

    ///////////////////////////////////////////////////////////////////////////
    Log::verbose_flag() = true;
    ///////////////////////////////////////////////////////////////////////////

    Application::StartupParams app_params;

    // parse options
    int opt;
    const auto [short_opts, long_opts] = get_opts();
    while ((opt = getopt_long(argc, argv, short_opts.c_str(), long_opts.data(),
                              nullptr)) != -1) {
        switch (opt) {
            case 'g':
                app_params.mode = Application::Mode::Gallery;
                break;
            case 's':
                app_params.mode = Application::Mode::Slideshow;
                break;
            case 'l':
                app_params.from_file = std::filesystem::absolute(optarg);
                break;
#ifdef HAVE_COMPOSITOR
            case 'p':
                // todo
                break;
#endif
            case 'w':
                // todo
                break;
            case 'f':
                app_params.fullscreen = true;
                break;
            case 'a':
                app_params.app_id = optarg;
                break;
            case 'c':
                app_params.config = std::filesystem::absolute(optarg);
                break;
            case 'V':
                Log::verbose_flag() = true;
                break;
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                return -1; // unreachabe
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
                return -1; // unreachabe
            default:
                exit(EXIT_FAILURE);
        }
    }

    for (int i = optind; i < argc; ++i) {
        app_params.sources.push_back(argv[i]);
    }

    return Application::self().run(app_params);
}

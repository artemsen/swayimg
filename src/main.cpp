// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"
#include "buildconf.hpp"
#include "imageformat.hpp"
#include "log.hpp"

#include <getopt.h>
#include <unistd.h>

#include <clocale>
#include <cstdlib>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

/** Command line arguments. */
class Args {
public:
    using Handler = std::function<void(const char*)>;

    /** Print arguments list. */
    void print() const
    {
        for (const auto& it : arguments) {
            std::string opt = it.long_opt;
            if (it.format) {
                opt += '=';
                opt += it.format;
            }
            std::string arg;
            if (it.short_opt) {
                arg = std::format("-{},", it.short_opt);
            }
            printf("  %3s --%-16s %s\n", arg.c_str(), opt.c_str(), it.help);
        }
    }

    /**
     * Add argument.
     * @param short_opt short option character
     * @param long_opt long option name
     * @param format format description
     * @param help help string
     * @param handler argument handler
     */
    void add(const char short_opt, const char* long_opt, const char* format,
             const char* help, const Handler& handler)
    {
        const Arg arg { .short_opt = short_opt,
                        .long_opt = long_opt,
                        .format = format,
                        .help = help,
                        .handler = handler };
        assert(std::find_if(arguments.begin(), arguments.end(),
                            [arg](const Arg& exist) {
                                return (arg.short_opt &&
                                        arg.short_opt == exist.short_opt) ||
                                    strcmp(arg.long_opt, exist.long_opt) == 0;
                            }) == arguments.end());
        arguments.emplace_back(arg);
    }

    /**
     * Process command line options.
     * @param argc number of arguments to parse
     * @param argv arguments array
     * @return index of the first non option argument
     */
    int process(const int argc, char* argv[])
    {
        // fill options
        std::string optstring;
        optstring.reserve(arguments.size() * 2);
        std::vector<option> longopts;
        longopts.reserve(arguments.size() + 1);
        int index = LONGOPT_OFFSET;
        for (const auto& it : arguments) {
            if (it.short_opt) {
                optstring += it.short_opt;
                if (it.format) {
                    optstring += ':';
                }
            }
            longopts.push_back({
                .name = it.long_opt,
                .has_arg = it.format ? required_argument : no_argument,
                .flag = nullptr,
                .val = index++,
            });
        }
        longopts.push_back({});

        // handle options
        int opt;
        while ((opt = getopt_long(argc, argv, optstring.c_str(),
                                  longopts.data(), nullptr)) != -1) {
            const Arg* arg = nullptr;
            if (opt >= LONGOPT_OFFSET) {
                arg = &arguments[opt - LONGOPT_OFFSET];
            } else {
                for (auto& it : arguments) {
                    if (opt == it.short_opt) {
                        arg = &it;
                        break;
                    }
                }
            }
            if (!arg) {
                return -1;
            }
            arg->handler(arg->format ? optarg : nullptr);
        }

        return optind;
    }

    /**
     * Parse pair of numbers.
     * @param text source text to parse
     * @return pair of numbers (ssize_t::min on errors)
     */
    static std::pair<ssize_t, ssize_t> parse_numpair(const std::string& text)
    {
        ssize_t first = std::numeric_limits<ssize_t>::min();
        ssize_t second = std::numeric_limits<ssize_t>::min();

        const std::string valid = "-0123456789";
        const size_t delim = text.find_first_not_of(valid, 0);
        if (delim != std::string::npos &&
            text.find_first_not_of(valid, delim + 1) == std::string::npos) {
            try {
                first = std::stol(text.substr(0, delim));
                second = std::stol(text.substr(delim + 1));
            } catch (...) {
                first = std::numeric_limits<ssize_t>::min();
                second = std::numeric_limits<ssize_t>::min();
            }
        }

        return std::make_pair(first, second);
    }

private:
    struct Arg {
        const char short_opt; ///< Short option character
        const char* long_opt; ///< Long option name
        const char* format;   ///< Format description
        const char* help;     ///< Help string
        Handler handler;      ///< Arg handler
    };

    std::vector<Arg> arguments; ///< Supported arguments

    static constexpr int LONGOPT_OFFSET = 1000;
};

} // anonymous namespace

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    std::setlocale(LC_ALL, "");
    std::srand(getpid());

    StartupParams& params = *Application::self().sparams;

    Args args;
    args.add('v', "viewer", nullptr, "start in viewer mode",
             [&params](const char*) {
                 params.mode.lock(AppMode::Viewer);
             });

    args.add('g', "gallery", nullptr, "start in gallery mode",
             [&params](const char*) {
                 params.mode.lock(AppMode::Gallery);
             });

    args.add('s', "slideshow", nullptr, "start in slideshow mode",
             [&params](const char*) {
                 params.mode.lock(AppMode::Slideshow);
             });

    args.add('f', "from-file", "FILE", "load file list from file",
             [&params](const char* arg) {
                 std::ifstream file(arg);
                 if (!file.is_open()) {
                     Log::error("Unable to open file {}", arg);
                     exit(EXIT_FAILURE);
                 }
                 std::string line;
                 while (std::getline(file, line)) {
                     if (!line.empty()) {
                         params.sources.emplace_back(line);
                     }
                 }
                 file.close();
                 if (params.sources.empty()) {
                     Log::error("File {} is empty", arg);
                     exit(EXIT_FAILURE);
                 }
             });

#ifdef HAVE_COMPOSITOR
    args.add('P', "position", "X,Y", "(Sway/Hyprland only) set window position",
             [&params](const char* arg) {
                 auto [x, y] = Args::parse_numpair(arg);
                 if (x == std::numeric_limits<ssize_t>::min() ||
                     y == std::numeric_limits<ssize_t>::min()) {
                     Log::error("Invalid window position: {}", arg);
                     exit(EXIT_FAILURE);
                 }
                 params.wnd_pos.lock(Point(x, y));
             });
#endif

    args.add('S', "size", "W,H", "set preferable window size",
             [&params](const char* arg) {
                 auto [w, h] = Args::parse_numpair(arg);
                 if (w <= 0 || h <= 0) {
                     Log::error("Invalid window size: {}", arg);
                     exit(EXIT_FAILURE);
                 }
                 params.wnd_size.lock(Size(w, h));
             });

    args.add('F', "fullscreen", nullptr, "open in full screen mode",
             [&params](const char*) {
                 params.fullscreen.lock(true);
             });

    args.add('c', "config", "FILE", "load config from FILE",
             [&params](const char* arg) {
                 params.config = arg;
             });

    args.add('e', "execute", "LUA", "execute Lua script on start",
             [&params](const char* arg) {
                 params.lua_exec = arg;
             });

    args.add('a', "appid", "ID", "set application id",
             [&params](const char* arg) {
                 if (!*arg) {
                     Log::error("Empty application id");
                     exit(EXIT_FAILURE);
                 }
                 params.app_id.lock(arg);
             });

    args.add(0, "verbose", nullptr, "enable verbose output", [](const char*) {
        Log::verbose_enable() = true;
    });

    args.add('V', "version", nullptr, "print version info and exit",
             [](const char*) {
                 puts("swayimg version " APP_VERSION ".");
                 puts("https://github.com/artemsen/swayimg");
                 printf("Supported formats: %s.\n",
                        FormatFactory::self().list().c_str());
                 exit(EXIT_SUCCESS);
             });

    args.add('h', "help", nullptr, "print this help and exit",
             [&args](const char*) {
                 puts("usage: swayimg [option]... [file]...");
                 puts("show images from file(s).");
                 puts("if file is -, read standard input.");
                 puts("if no file specified - read all files from the "
                      "current directory.\n");
                 puts("mandatory arguments to long options are mandatory for "
                      "short options too.");
                 args.print();
                 exit(EXIT_SUCCESS);
             });

    const int argn = args.process(argc, argv);
    if (argn < 0) {
        return EXIT_FAILURE;
    }

    for (int i = argn; i < argc; ++i) {
        params.sources.emplace_back(argv[i]);
    }
    if (params.sources.empty()) {
        params.sources.emplace_back("."); // all from the current dir by default
    }

    return Application::self().run();
}

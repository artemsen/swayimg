// SPDX-License-Identifier: MIT
// Program entry point.
// Copyright (C) 2020 Artem Senichev <artemsen@gmail.com>

#include "application.hpp"
#include "buildconf.hpp"
#include "imageloader.hpp"
#include "log.hpp"

#include <getopt.h>
#include <unistd.h>

#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <format>
#include <functional>
#include <limits>
#include <string>
#include <vector>

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
        const Arg arg { short_opt, long_opt, format, help, handler };
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
    int process(int argc, char* argv[])
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
                it.long_opt,
                it.format ? required_argument : no_argument,
                nullptr,
                index++,
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

/**
 * Application entry point.
 */
int main(int argc, char* argv[])
{
    std::setlocale(LC_ALL, "");
    std::srand(getpid());

    Args args;

    args.add('v', "viewer", nullptr, "start in viewer mode", [](const char*) {
        Application::self().sparams.mode = Application::Mode::Viewer;
    });

    args.add('g', "gallery", nullptr, "start in gallery mode", [](const char*) {
        Application::self().sparams.mode = Application::Mode::Gallery;
    });

    args.add(
        's', "slideshow", nullptr, "start in slideshow mode", [](const char*) {
            Application::self().sparams.mode = Application::Mode::Slideshow;
        });

    args.add('f', "from-file", "FILE", "load file list from file",
             [](const char* arg) {
                 try {
                     Application::self().sparams.from_file =
                         std::filesystem::absolute(arg);
                 } catch (const std::exception&) {
                     Log::error("Invalid file path \"{}\"", arg);
                     exit(EXIT_FAILURE);
                 }
             });

#ifdef HAVE_COMPOSITOR
    args.add('P', "position", "X,Y", "(Sway/Hyprland only) set window position",
             [](const char* arg) {
                 auto [x, y] = Args::parse_numpair(arg);
                 if (x == std::numeric_limits<ssize_t>::min() ||
                     y == std::numeric_limits<ssize_t>::min()) {
                     Log::error("Invalid window position: {}", arg);
                     exit(EXIT_FAILURE);
                 }
                 Application::self().sparams.window.x = x;
                 Application::self().sparams.window.y = y;
             });
#endif

    args.add('S', "size", "W,H", "set preferable window size",
             [](const char* arg) {
                 auto [w, h] = Args::parse_numpair(arg);
                 if (w <= 0 || h <= 0) {
                     Log::error("Invalid window size: {}", arg);
                     exit(EXIT_FAILURE);
                 }
                 Application::self().sparams.window.width = w;
                 Application::self().sparams.window.height = h;
             });

    args.add('F', "fullscreen", nullptr, "open in full screen mode",
             [](const char*) {
                 Application::self().sparams.fullscreen = true;
             });

    args.add('c', "config", "FILE", "load config from FILE",
             [](const char* arg) {
                 Application::self().sparams.config = arg;
             });

    args.add('e', "execute", "LUA", "execute Lua script on start",
             [](const char* arg) {
                 Application::self().sparams.lua_script = arg;
             });

    args.add(0, "class", "NAME", "set window class/app_id",
             [](const char* arg) {
                 if (!*arg) {
                     Log::error("Empty window class name");
                     exit(EXIT_FAILURE);
                 }
                 Application::self().sparams.app_id = arg;
             });

    args.add(0, "verbose", nullptr, "enable verbose output", [](const char*) {
        Log::verbose_enable() = true;
    });

    args.add('V', "version", nullptr, "print version info and exit",
             [](const char*) {
                 puts("swayimg version " APP_VERSION ".");
                 puts("https://github.com/artemsen/swayimg");
                 printf("Supported formats: %s.\n",
                        ImageLoader::self().format_list().c_str());
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
        Application::self().sparams.sources.emplace_back(argv[i]);
    }

    return Application::self().run();
}

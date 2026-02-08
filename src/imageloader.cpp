// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "imageloader.hpp"

#include "buildconf.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

#ifdef HAVE_LIBEXIV2
#include <exiv2/exiv2.hpp>
#endif

/**
 * Read data from stream file.
 * @param fd file descriptor for read
 * @return data array, empty on errors
 */
static std::vector<uint8_t> read_stream(int fd)
{
    std::vector<uint8_t> data;

    while (true) {
        uint8_t buffer[4096];
        const ssize_t rc = read(fd, buffer, sizeof(buffer));
        if (rc == 0) {
            break;
        }
        if (rc == -1 && errno != EAGAIN) {
            return {};
        }
        data.insert(data.end(), buffer, buffer + rc);
    }

    return data;
}

/**
 * Read data from file.
 * @param file path to the file to load
 * @return data array, empty on errors
 */
static std::vector<uint8_t> read_file(const std::filesystem::path& file)
{
    const int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        return {};
    }

    std::vector<uint8_t> data = read_stream(fd);

    close(fd);

    return data;
}

/**
 * Read data from stdout printed by external command.
 * @param cmd execution command to get stdout data
 * @return data array, empty on errors
 */
static std::vector<uint8_t> read_stdout(const std::string& cmd)
{
    std::vector<uint8_t> data;

    int fds_in[2], fds_out[2];
    if (pipe(fds_in) == -1) {
        return {};
    }
    if (pipe(fds_out) == -1) {
        close(fds_in[0]);
        close(fds_in[1]);
        return {};
    }

    const char* shell = getenv("SHELL");
    if (!shell || !*shell) {
        shell = "/bin/sh";
    }

    const pid_t pid = fork();
    switch (pid) {
        case -1:
            close(fds_in[0]);
            close(fds_in[1]);
            close(fds_out[0]);
            close(fds_out[1]);
            break;
        case 0: // child process
            close(fds_in[1]);
            close(fds_out[0]);
            // redirect stdio
            dup2(fds_in[0], STDIN_FILENO);
            close(fds_in[0]);
            dup2(fds_out[1], STDOUT_FILENO);
            close(fds_out[1]);

            // skip clang-tidy check: we trust users's command
            // NOLINTNEXTLINE(clang-analyzer-optin.taint.GenericTaint)
            execlp(shell, shell, "-c", cmd.c_str(), nullptr);

            exit(1); // unreachable
            break;
        default: // parent process
            close(fds_in[0]);
            close(fds_in[1]);
            close(fds_out[1]);
            data = read_stream(fds_out[0]);
            close(fds_out[0]);
            break;
    }

    return data;
}

#ifdef HAVE_LIBEXIV2
/**
 * Read and handle EXIF data.
 * @param image target image instance
 * @param data image file data
 */
static void read_exif(ImagePtr& image, const std::vector<uint8_t>& data)
{
    try {
        // read EXIF data
        Exiv2::Image::UniquePtr eimg =
            Exiv2::ImageFactory::open(data.data(), data.size());
        if (eimg.get() == 0) {
            return;
        }
        eimg->readMetadata();
        const Exiv2::ExifData& exif = eimg->exifData();
        if (exif.empty()) {
            return;
        }

        // import EXIF to meta container
        for (const auto& it : exif) {
            image->meta.insert(std::make_pair(it.key(), it.value().toString()));
        }

        // fix orientation
        const auto& orient =
            exif.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
        if (orient != exif.end()) {
            switch (orient->toInt64()) {
                case 2: // flipped back-to-front
                    image->flip_horizontal();
                    break;
                case 3: // upside down
                    image->rotate(180);
                    break;
                case 4: // flipped back-to-front and upside down
                    image->flip_vertical();
                    break;
                case 5: // flipped back-to-front and on its side
                    image->flip_horizontal();
                    image->rotate(90);
                    break;
                case 6: // on its side
                    image->rotate(90);
                    break;
                case 7: // flipped back-to-front and on its far side
                    image->flip_vertical();
                    image->rotate(270);
                    break;
                case 8: // on its far side
                    image->rotate(270);
                    break;
                default:
                    break;
            }
        }
    } catch (Exiv2::Error&) {
    }
}
#endif // HAVE_LIBEXIV2

void ImageLoader::register_format(const char* name, Priority priority,
                                  const Constructor& creator)
{
    std::vector<ImageLoader::Instance>& registry = get_registry();
    const auto it = std::find_if(registry.begin(), registry.end(),
                                 [priority](const auto& it) {
                                     return priority < it.priority;
                                 });
    registry.insert(it, { name, priority, creator });
}

std::string ImageLoader::format_list()
{
    std::string formats;

    for (const auto& it : get_registry()) {
        if (!formats.empty()) {
            formats += ", ";
        }
        formats += it.name;
    }

    return formats;
}

ImagePtr ImageLoader::load(const ImageList::EntryPtr& entry)
{
    // read file data
    std::vector<uint8_t> data;
    const std::string full_path = entry->path.string();
    if (full_path.starts_with(ImageList::Entry::SRC_STDIN)) {
        data = read_stream(STDIN_FILENO);
    } else if (full_path.starts_with(ImageList::Entry::SRC_EXEC)) {
        data =
            read_stdout(full_path.substr(strlen(ImageList::Entry::SRC_EXEC)));
    } else {
        data = read_file(entry->path);
    }
    if (data.empty()) {
        return nullptr;
    }

    // decode file
    for (const auto& it : get_registry()) {
        ImagePtr image = it.create();
        if (image->load(data)) {
            image->entry = entry;
#ifdef HAVE_LIBEXIV2
            read_exif(image, data);
#endif // HAVE_LIBEXIV2
            return image;
        }
    }

    return nullptr;
}

std::vector<ImageLoader::Instance>& ImageLoader::get_registry()
{
    static std::vector<ImageLoader::Instance> singleton;
    return singleton;
}

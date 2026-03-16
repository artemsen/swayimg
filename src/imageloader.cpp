// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "imageloader.hpp"

#include "buildconf.hpp"
#include "log.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

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
        Log::error(errno, "Unable to open file {}", file.string());
        return {};
    }

    std::vector<uint8_t> data = read_stream(fd);
    if (data.empty()) {
        Log::error(errno, "Unable to read file {}", file.string());
    }

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

    if (data.empty()) {
        Log::error("Unable to read stdout from command {}", cmd);
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
        if (!eimg) {
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

ImagePtr ImageLoader::load(const ImageEntryPtr& entry)
{
    // read file data
    std::vector<uint8_t> data;
    const std::string full_path = entry->path.string();
    if (full_path.starts_with(ImageEntry::SRC_STDIN)) {
        data = read_stream(STDIN_FILENO);
        if (data.empty()) {
            Log::error(errno, "Unable to read stdout");
        }
    } else if (full_path.starts_with(ImageEntry::SRC_EXEC)) {
        data = read_stdout(full_path.substr(strlen(ImageEntry::SRC_EXEC)));
    } else {
        data = read_file(entry->path);
    }
    if (data.empty()) {
        return nullptr;
    }

    // decode file
    for (const auto& it : get_registry()) {
        ImagePtr image = it.create();
        const Log::PerfTimer timer;
        if (image->load(data)) {
            if (Log::verbose_enable()) {
                Log::verbose("Image {} loaded in {:.6f} sec",
                             entry->path.filename().string(), timer.time());
            }
            if (full_path.starts_with(ImageEntry::SRC_STDIN) ||
                full_path.starts_with(ImageEntry::SRC_EXEC)) {
                entry->mtime = time(nullptr);
            }
            entry->size = data.size();

            image->entry = entry;
#ifdef HAVE_LIBEXIV2
            read_exif(image, data);
#endif // HAVE_LIBEXIV2
            return image;
        }
    }

    Log::verbose("Unsupported image format in {}", entry->path.string());
    return nullptr;
}

bool ImageLoader::check_header(const std::filesystem::path& path)
{
    // DICOM has the largest signature offset (128 bytes + 4 byte signature)
    static constexpr size_t header_size = 132;

    const int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    uint8_t header[header_size];
    const ssize_t bytes_read = read(fd, header, header_size);
    close(fd);

    if (bytes_read < 2) {
        return false;
    }
    const size_t len = static_cast<size_t>(bytes_read);

    // PNG: 8-byte signature starting with 0x89 0x50 0x4e 0x47
    if (len >= 4 && header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' &&
        header[3] == 'G') {
        return true;
    }

    // JPEG: starts with 0xff 0xd8
    if (len >= 2 && header[0] == 0xff && header[1] == 0xd8) {
        return true;
    }

    // BMP: starts with "BM"
    if (len >= 2 && header[0] == 'B' && header[1] == 'M') {
        return true;
    }

    // GIF: starts with "GIF"
    if (len >= 3 && header[0] == 'G' && header[1] == 'I' && header[2] == 'F') {
        return true;
    }

    // WebP: starts with "RIFF"
    if (len >= 4 && header[0] == 'R' && header[1] == 'I' && header[2] == 'F' &&
        header[3] == 'F') {
        return true;
    }

    // TIFF: little-endian (II*\0) or big-endian (MM\0*)
    if (len >= 4 &&
        ((header[0] == 0x49 && header[1] == 0x49 && header[2] == 0x2a &&
          header[3] == 0x00) ||
         (header[0] == 0x4d && header[1] == 0x4d && header[2] == 0x00 &&
          header[3] == 0x2a))) {
        return true;
    }

    // QOI: starts with "qoif"
    if (len >= 4 && header[0] == 'q' && header[1] == 'o' && header[2] == 'i' &&
        header[3] == 'f') {
        return true;
    }

    // Farbfeld: starts with "farbfeld"
    if (len >= 8 && std::memcmp(header, "farbfeld", 8) == 0) {
        return true;
    }

    // EXR: starts with 0x76 0x2f 0x31 0x01
    if (len >= 4 && header[0] == 0x76 && header[1] == 0x2f &&
        header[2] == 0x31 && header[3] == 0x01) {
        return true;
    }

    // PNM: starts with 'P' followed by '1'-'6'
    if (len >= 2 && header[0] == 'P' && header[1] >= '1' && header[1] <= '6') {
        return true;
    }

    // DICOM: "DICM" at offset 128
    if (len >= 132 && std::memcmp(header + 128, "DICM", 4) == 0) {
        return true;
    }

    // Sixel: starts with ESC (0x1b)
    if (len >= 1 && header[0] == 0x1b) {
        return true;
    }

    // SVG: search for "<svg" within the header
    if (len >= 4) {
        for (size_t i = 0; i + 4 <= len; ++i) {
            if (std::memcmp(header + i, "<svg", 4) == 0) {
                return true;
            }
        }
    }

    // AVIF/HEIF: "ftyp" at offset 4
    if (len >= 8 && std::memcmp(header + 4, "ftyp", 4) == 0) {
        return true;
    }

    // JPEG XL: starts with 0xff 0x0a (codestream) or
    //          0x00 0x00 0x00 0x0c 0x4a 0x58 0x4c 0x20 (container)
    if (len >= 2 && header[0] == 0xff && header[1] == 0x0a) {
        return true;
    }
    if (len >= 8 && header[0] == 0x00 && header[1] == 0x00 &&
        header[2] == 0x00 && header[3] == 0x0c && header[4] == 0x4a &&
        header[5] == 0x58 && header[6] == 0x4c && header[7] == 0x20) {
        return true;
    }

    // TGA: no magic bytes, check valid image type and bpp fields
    if (len >= 18) {
        const uint8_t image_type = header[2];
        const uint8_t bpp = header[16];
        if ((image_type == 1 || image_type == 2 || image_type == 3 ||
             image_type == 9 || image_type == 10 || image_type == 11) &&
            (bpp == 8 || bpp == 15 || bpp == 16 || bpp == 24 || bpp == 32)) {
            return true;
        }
    }

    return false;
}

std::vector<ImageLoader::Instance>& ImageLoader::get_registry()
{
    static std::vector<ImageLoader::Instance> singleton;
    return singleton;
}

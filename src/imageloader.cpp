// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "imageloader.hpp"

#include "buildconf.hpp"
#include "log.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

#ifdef HAVE_LIBEXIV2
#include <exiv2/exiv2.hpp>
/**
 * Read and handle EXIF data.
 * @param image target image instance
 * @param data image file data
 */
static void read_exif(ImagePtr& image, const Image::Data& data)
{
    try {
        // read EXIF data
        Exiv2::Image::UniquePtr eimg =
            Exiv2::ImageFactory::open(data.data, data.size);
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
    } catch (Exiv2::Error&) {
    }
}
#endif // HAVE_LIBEXIV2

/** Image data reader. */
struct DataBuffer : public Image::Data {
    ~DataBuffer()
    {
        if (container.empty() && data) {
            munmap(data, size);
        }
    }

    /**
     * Read data from stream file.
     * @param fd file descriptor for read
     * @return true if data loaded
     */
    bool read_stream(int fd)
    {
        assert(!data && !size);

        while (true) {
            uint8_t buffer[4096];
            const ssize_t rc = read(fd, buffer, sizeof(buffer));
            if (rc == 0) {
                break;
            }
            if (rc == -1 && errno != EAGAIN) {
                Log::error(errno, "Unable to read stream");
                container.clear();
                return false;
            }
            container.insert(container.end(), buffer, buffer + rc);
        }

        data = container.data();
        size = container.size();

        return true;
    }

    /**
     * Read data from file.
     * @param file path to the file to load
     * @return true if file loaded
     */
    bool read_file(const std::filesystem::path& file)
    {
        assert(!data && !size);

        // get file size and checktype
        struct stat st;
        if (stat(file.c_str(), &st) == -1) {
            Log::error(errno, "Unable to get stat for file {}", file.string());
            return false;
        }
        if (!S_ISREG(st.st_mode)) {
            Log::error("Not a regular file {}", file.string());
            return false;
        }

        // open file and map it to memory
        const int fd = open(file.c_str(), O_RDONLY);
        if (fd == -1) {
            Log::error(errno, "Unable to open file {}", file.string());
            return false;
        }
        void* mdata = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mdata == MAP_FAILED) {
            Log::error(errno, "Unable to map file {}", file.string());
            close(fd);
            return false;
        }

        close(fd);

        data = reinterpret_cast<uint8_t*>(mdata);
        size = st.st_size;

        return true;
    }

    /**
     * Read data from stdout printed by external command.
     * @param cmd execution command to get stdout data
     * @return true if data loaded
     */
    bool read_stdout(const std::string& cmd)
    {
        int fds_in[2], fds_out[2];
        if (pipe(fds_in) == -1) {
            Log::error(errno, "Unable to create pipes");
            return false;
        }
        if (pipe(fds_out) == -1) {
            Log::error(errno, "Unable to create pipes");
            close(fds_in[0]);
            close(fds_in[1]);
            return false;
        }

        const char* shell = getenv("SHELL");
        if (!shell || !*shell) {
            shell = "/bin/sh";
        }

        bool rc = false;

        const pid_t pid = fork();
        switch (pid) {
            case -1:
                Log::error(errno, "Unable to fork");
                close(fds_in[0]);
                close(fds_in[1]);
                close(fds_out[0]);
                close(fds_out[1]);
                return false;
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
                Log::error(errno, "Unable to call exec {}", cmd);
                _exit(1);
                break;
            default: // parent process
                close(fds_in[0]);
                close(fds_in[1]);
                close(fds_out[1]);
                rc = read_stream(fds_out[0]);
                close(fds_out[0]);
                waitpid(pid, nullptr, 0);
                break;
        }

        return rc;
    }

private:
    std::vector<uint8_t> container;
};

ImageLoader& ImageLoader::self()
{
    static ImageLoader singleton;
    return singleton;
}

void ImageLoader::register_format(const char* name, Priority priority,
                                  const Constructor& creator)
{
    const auto it = std::find_if(registry.begin(), registry.end(),
                                 [priority](const auto& it) {
                                     return priority < it.priority;
                                 });
    registry.insert(it, { name, priority, creator });
}

std::string ImageLoader::format_list() const
{
    std::string formats;

    for (const auto& it : registry) {
        if (!formats.empty()) {
            formats += ", ";
        }
        formats += it.name;
    }

    return formats;
}

ImagePtr ImageLoader::load(const ImageEntryPtr& entry) const
{
    const Log::PerfTimer timer;

    // read file data
    bool rc;
    DataBuffer data;
    const std::string full_path = entry->path.string();
    if (full_path.starts_with(ImageEntry::SRC_STDIN)) {
        rc = data.read_stream(STDIN_FILENO);
    } else if (full_path.starts_with(ImageEntry::SRC_EXEC)) {
        rc = data.read_stdout(full_path.substr(strlen(ImageEntry::SRC_EXEC)));
    } else {
        rc = data.read_file(entry->path);
    }
    if (!rc) {
        return nullptr;
    }
    if (data.size == 0) {
        Log::error("No image data in {}", full_path);
        return nullptr;
    }

    // decode file
    for (const auto& it : registry) {
        ImagePtr image = it.create();
        if (image->load(data)) {
            if (Log::verbose_enable()) {
                Log::verbose("Image {} loaded in {:.6f} sec",
                             entry->path.filename().string(), timer.time());
            }
            if (full_path.starts_with(ImageEntry::SRC_STDIN) ||
                full_path.starts_with(ImageEntry::SRC_EXEC)) {
                entry->mtime = time(nullptr);
            }
            entry->size = data.size;

            image->entry = entry;

#ifdef HAVE_LIBEXIV2
            read_exif(image, data);
            if (fix_orientation) {
                image->fix_orientation();
            }
#endif // HAVE_LIBEXIV2

            return image;
        }
    }

    Log::verbose("Unsupported image format in {}", entry->path.string());
    return nullptr;
}

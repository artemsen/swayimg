// SPDX-License-Identifier: MIT
// Image format interface.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "imageformat.hpp"

#include "buildconf.hpp"
#include "log.hpp"
#include "render.hpp"

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
static void read_exif(ImagePtr& image, const ImageFormat::Data& data)
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

        // export EXIF data to meta container
        for (const auto& it : exif) {
            image->meta.insert(std::make_pair(it.key(), it.value().toString()));
        }
    } catch (Exiv2::Error&) {
    }
}
#endif // HAVE_LIBEXIV2

/** Image data reader. */
struct DataBuffer : public ImageFormat::Data {
    ~DataBuffer()
    {
        if (container.empty() && data) {
            munmap(data, size);
        }
    }

    /**
     * Load source data.
     * @param entry image entry to load
     * @return true if data was loaded
     */
    bool load(const ImageEntryPtr& entry)
    {
        bool rc;

        const std::string full_path = entry->path.string();
        if (full_path.starts_with(ImageEntry::SRC_STDIN)) {
            rc = read_stream(STDIN_FILENO);
        } else if (full_path.starts_with(ImageEntry::SRC_EXEC)) {
            rc = read_stdout(full_path.substr(strlen(ImageEntry::SRC_EXEC)));
        } else {
            rc = read_file(entry->path);
        }

        if (rc && size == 0) {
            Log::error("No image data in {}", full_path);
            rc = false;
        }

        return rc;
    }

private:
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

ImageFormat::ImageFormat(const Priority load_priority, const char* format_name)
    : priority(load_priority)
    , name(format_name)
{
    FormatFactory::self().add(this);
}

Pixmap ImageFormat::preview(const Data& data, const size_t sz,
                            const bool max_sz)
{
    ImagePtr image = decode(data);
    if (!image) {
        return {};
    }
#ifdef HAVE_LIBEXIV2
    read_exif(image, data);
    if (FormatFactory::self().fix_orientation) {
        image->fix_orientation();
    }
#endif // HAVE_LIBEXIV2

    const Pixmap& origin = image->frames[0].pm;

    // get target scale
    const double scale_w = static_cast<double>(sz) / origin.width();
    const double scale_h = static_cast<double>(sz) / origin.height();
    const double scale =
        max_sz ? std::max(scale_w, scale_h) : std::min(scale_w, scale_h);

    // create thumbnail
    Pixmap thumb;
    thumb.create(origin.format(), scale * origin.width(),
                 scale * origin.height());
    Render::self().draw(thumb, origin, { 0, 0 }, scale);

    return thumb;
}

FormatFactory& FormatFactory::self()
{
    static FormatFactory singleton;
    return singleton;
}

void FormatFactory::add(ImageFormat* fmt)
{
    const auto it =
        std::find_if(formats.begin(), formats.end(), [fmt](const auto& it) {
            return fmt->priority < it->priority;
        });
    formats.insert(it, fmt);
}

ImageFormat* FormatFactory::get(const char* name)
{
    auto it =
        std::find_if(formats.begin(), formats.end(), [name](const auto& it) {
            return strcmp(name, it->name) == 0;
        });
    return it == formats.end() ? nullptr : *it;
}

std::string FormatFactory::list() const
{
    std::string fmt;

    for (const auto& it : formats) {
        if (!fmt.empty()) {
            fmt += ", ";
        }
        fmt += it->name;
    }

    return fmt;
}

ImagePtr FormatFactory::load(const ImageEntryPtr& entry) const
{
    const Log::PerfTimer timer;

    // read file data
    DataBuffer data;
    if (!data.load(entry)) {
        return nullptr;
    }

    // decode file
    for (const auto& it : formats) {
        ImagePtr image = it->decode(data);
        if (!image) {
            continue;
        }

        if (Log::verbose_enable()) {
            Log::verbose("Image {} loaded in {:.6f} sec",
                         entry->path.filename().string(), timer.time());
        }

        const std::string path = entry->path.string();
        if (path.starts_with(ImageEntry::SRC_STDIN) ||
            path.starts_with(ImageEntry::SRC_EXEC)) {
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

    Log::verbose("Unsupported image format in {}", entry->path.string());
    return nullptr;
}

Pixmap FormatFactory::preview(const ImageEntryPtr& entry, const size_t sz,
                              const bool max_sz) const
{
    DataBuffer data;
    if (!data.load(entry)) {
        return {};
    }

    for (const auto& it : formats) {
        Pixmap pm = it->preview(data, sz, max_sz);
        if (pm) {
            return pm;
        }
    }

    return {};
}

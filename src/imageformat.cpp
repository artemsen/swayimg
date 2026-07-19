// SPDX-License-Identifier: MIT
// Image format interface.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "imageformat.hpp"

#include "buildconf.hpp"
#include "defaults.hpp"
#include "log.hpp"
#include "render.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>

#ifdef HAVE_LIBEXIV2
#include <exiv2/exiv2.hpp>
#endif // HAVE_LIBEXIV2

namespace {

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
        int fds_in[2];
        int fds_out[2];
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

} // anonymous namespace

ImageFormat::ImageFormat(const Priority load_priority,
                         const char* format_name) noexcept
    : priority(load_priority)
    , name(format_name)
{
    FormatFactory::self().add(this);
}

bool ImageFormat::set_params(const std::unordered_map<std::string, bool>&)
{
    return false;
}

Pixmap ImageFormat::preview(const Data& data, const size_t sz,
                            const bool fill) const
{
    ImagePtr image = decode(data);
    if (!image) {
        return {};
    }

    if (read_metadata(data, image) && FormatFactory::self().fix_orientation) {
        fix_orientation(image);
    }

    return make_thumb(image->frames[0].pm, sz, fill);
}

Pixmap ImageFormat::make_thumb(const Pixmap& pm, const size_t sz,
                               const bool fill)
{
    // get target scale
    const double scale_w = static_cast<double>(sz) / pm.width();
    const double scale_h = static_cast<double>(sz) / pm.height();
    const double scale =
        fill ? std::max(scale_w, scale_h) : std::min(scale_w, scale_h);

    // get fully scaled thumbnail size
    const size_t thumb_width = scale * pm.width();
    const size_t thumb_height = scale * pm.height();

    // get thumbnail offsets
    const ssize_t half_sz = sz / 2;
    const ssize_t x = fill ? half_sz - thumb_width / 2 : 0;
    const ssize_t y = fill ? half_sz - thumb_height / 2 : 0;

    // create thumbnail
    Pixmap thumb;
    thumb.create(pm.format(),
                 std::clamp(thumb_width, static_cast<size_t>(1), sz),
                 std::clamp(thumb_height, static_cast<size_t>(1), sz));
    Render::self().draw(thumb, pm, { .x = x, .y = y }, scale);

    return thumb;
}

void ImageFormat::fix_orientation(ImagePtr& image, const int orientation) const
{
    int exif_orient = orientation;
    if (exif_orient < 0) {
        const auto& it = image->meta.find("Exif.Image.Orientation");
        if (it != image->meta.end()) {
            exif_orient = std::strtol(it->second.c_str(), nullptr, 10);
        }
    }
    if (exif_orient > 0) {
        for (auto& it : image->frames) {
            fix_orientation(it.pm, exif_orient);
        }
    }
}

void ImageFormat::fix_orientation(Pixmap& pm, const int orientation)
{
    switch (orientation) {
        case 2: // flipped back-to-front
            pm.flip_horizontal();
            break;
        case 3: // upside down
            pm.rotate(180);
            break;
        case 4: // flipped back-to-front and upside down
            pm.flip_vertical();
            break;
        case 5: // flipped back-to-front and on its side
            pm.rotate(90);
            pm.flip_horizontal();
            break;
        case 6: // on its side
            pm.rotate(90);
            break;
        case 7: // flipped back-to-front and on its far side
            pm.rotate(90);
            pm.flip_vertical();
            break;
        case 8: // on its far side
            pm.rotate(270);
            break;
        default:
            break;
    }
}

bool ImageFormat::read_metadata(const Data& data, ImagePtr& image)
{
#ifdef HAVE_LIBEXIV2
    try {
        // read EXIF data
        Exiv2::Image::UniquePtr exiv2 =
            Exiv2::ImageFactory::open(data.data, data.size);
        if (!exiv2) {
            return false;
        }
        exiv2->readMetadata();

        // put EXIF data to meta container
        const Exiv2::ExifData& exif_data = exiv2->exifData();
        for (const auto& it : exif_data) {
            image->meta.insert(std::make_pair(it.key(), it.value().toString()));
        }

        // put IPTC data to meta container
        const Exiv2::IptcData& iptc_data = exiv2->iptcData();
        for (const auto& it : iptc_data) {
            image->meta.insert(std::make_pair(it.key(), it.value().toString()));
        }

        // put XMP data to meta container
        const Exiv2::XmpData& xmp_data = exiv2->xmpData();
        for (const auto& it : xmp_data) {
            image->meta.insert(std::make_pair(it.key(), it.value().toString()));
        }

        return !exif_data.empty() || !iptc_data.empty() || !xmp_data.empty();
    } catch (Exiv2::Error&) {
    }
#else
    (void)data;
    (void)image;
#endif // HAVE_LIBEXIV2
    return false;
}

FormatFactory& FormatFactory::self()
{
    static FormatFactory singleton;
    return singleton;
}

FormatFactory::FormatFactory()
    : fix_orientation(Defaults::img::fix_orientation)
    , embedded_thumb(Defaults::img::embedded_thumb)
{
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

    DataBuffer data;
    if (!data.load(entry)) {
        return nullptr;
    }

    ImagePtr image = decode(data);
    if (!image) {
        Log::verbose("Unsupported image format in {}", entry->path.string());
    } else {
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
    }

    return image;
}

bool FormatFactory::save(
    const Pixmap& pm, const std::unordered_map<std::string, std::string>& meta,
    const std::filesystem::path& path)
{
    ImageFormat* png = FormatFactory::self().get("png");
    if (!png) {
        Log::error("Unable to export pixmap, PNG not supported");
        return false;
    }

    const std::vector<uint8_t> data = png->encode(pm, meta);
    if (data.empty()) {
        Log::error("Unable to export pixmap, PNG encode failed");
        return false;
    }

    // open file
    const int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        Log::error(errno, "Unable to create file {}", path.string());
        return false;
    }

    // write file
    const uint8_t* ptr = data.data();
    size_t size = data.size();
    while (size) {
        const ssize_t written = write(fd, ptr, size);
        if (written == -1) {
            if (errno != EINTR) {
                Log::error(errno, "Unable to write file {}", path.string());
                close(fd);
                return false;
            }
            continue;
        }
        size -= written;
        ptr += written;
    }

    if (close(fd) == -1) {
        Log::error(errno, "Failed to close file {}", path.string());
        return false;
    }

    return true;
}

ImagePtr FormatFactory::decode(const ImageFormat::Data& data) const
{
    for (const auto& it : formats) {
        ImagePtr image = it->decode(data);
        if (!image) {
            continue;
        }

        if (ImageFormat::read_metadata(data, image) && fix_orientation) {
            it->fix_orientation(image);
        }

        return image;
    }

    return nullptr;
}

Pixmap FormatFactory::preview(const ImageEntryPtr& entry, const size_t sz,
                              const bool fill) const
{
    DataBuffer data;
    if (!data.load(entry)) {
        return {};
    }

    for (const auto& it : formats) {
        Pixmap pm = it->preview(data, sz, fill);
        if (pm) {
            return pm;
        }
    }

    return {};
}

// SPDX-License-Identifier: MIT
// Image format interface.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "imageformat.hpp"

#include "buildconf.hpp"
#include "formatfactory.hpp"
#include "render.hpp"

#ifdef HAVE_LIBEXIV2
#include <exiv2/exiv2.hpp>
#endif // HAVE_LIBEXIV2

ImageFormat::ImageFormat(const Priority load_priority,
                         const char* format_name) noexcept
    : priority(load_priority)
    , enable(true)
    , name(format_name)
{
    FormatFactory::self().add(this);
}

void ImageFormat::set_config(Config& params)
{
    params.get("enable", enable);
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

void ImageFormat::Config::get(const std::string& name, bool& value)
{
    auto it = params.find(name);
    if (it != params.end()) {
        Value& pval = it->second;
        if (std::holds_alternative<bool>(pval.value)) {
            value = std::get<bool>(pval.value);
            pval.status = Handled;
        } else {
            pval.status = Invalid;
        }
    }
}

void ImageFormat::Config::get(const std::string& name, argb_t& value)
{
    auto it = params.find(name);
    if (it != params.end()) {
        Value& pval = it->second;
        pval.status = Invalid;
        if (std::holds_alternative<size_t>(pval.value)) {
            const size_t nval = std::get<size_t>(pval.value);
            if (nval <= 0xffffffff) {
                value = nval;
                pval.status = Handled;
            }
        }
    }
}

void ImageFormat::Config::get(const std::string& name, size_t& value,
                              const size_t min_val, const size_t max_val)
{
    auto it = params.find(name);
    if (it != params.end()) {
        Value& pval = it->second;
        pval.status = Invalid;
        if (std::holds_alternative<size_t>(pval.value)) {
            const size_t nval = std::get<size_t>(pval.value);
            if (nval >= min_val && nval <= max_val) {
                value = nval;
                pval.status = Handled;
            }
        }
    }
}

void ImageFormat::Config::get(const std::string& name, std::string& value,
                              const size_t min_len)
{
    auto it = params.find(name);
    if (it != params.end()) {
        Value& pval = it->second;
        pval.status = Invalid;
        if (std::holds_alternative<std::string>(pval.value)) {
            const std::string& sval = std::get<std::string>(pval.value);
            if (sval.length() >= min_len) {
                value = sval;
                pval.status = Handled;
            }
        }
    }
}

std::vector<std::string> ImageFormat::Config::get(const Status status) const
{
    std::vector<std::string> keys;
    for (const auto& [name, value] : params) {
        if (value.status == status) {
            keys.push_back(name);
        }
    }
    return keys;
}

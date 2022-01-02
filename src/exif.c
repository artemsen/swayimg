// SPDX-License-Identifier: MIT
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "exif.h"

#include <libexif/exif-data.h>
#include <string.h>

/**
 * Set orientation from EXIF data.
 * @param[in] img target image instance
 * @param[in] exif instance of EXIF reader
 */
static void read_orientation(image_t* img, ExifData* exif)
{
    const ExifEntry* entry = exif_data_get_entry(exif, EXIF_TAG_ORIENTATION);
    if (entry) {
        const ExifByteOrder byte_order = exif_data_get_byte_order(exif);
        const ExifShort orientation = exif_get_short(entry->data, byte_order);
        if (orientation >= 1 && orientation <= 8) {
            img->orientation = (orientation_t)orientation;
        }
    }
}

/**
 * Add meta info from EXIF tag.
 * @param[in] img target image instance
 * @param[in] exif instance of EXIF reader
 * @param[in] tag EXIF tag
 * @param[in] name EXIF tag name
 */
static void add_meta(image_t* img, ExifData* exif, ExifTag tag,
                     const char* name)
{
    char value[64];
    ExifEntry* entry = exif_data_get_entry(exif, tag);
    if (entry) {
        exif_entry_get_value(entry, value, sizeof(value));
        if (*value) {
            add_image_meta(img, name, value);
        }
    }
}

/**
 * Append string.
 * @param[in] buffer destination buffer
 * @param[in] buffer_max size of the buffer
 * @param[in] value value to append
 */
static void append(char* buffer, size_t buffer_max, const char* value)
{
    const size_t current_len = strlen(buffer);
    const size_t value_len = strlen(value);
    if (current_len + value_len + 1 <= buffer_max) {
        memcpy(buffer + current_len, value, value_len + 1);
    }
}

/**
 * Read coordinate to string buffer.
 * @param[in] exif instance of EXIF reader
 * @param[in] tag location tag
 * @param[in] ref location reference
 * @param[in] buffer destination buffer
 * @param[in] buffer_sz size of the buffer
 * @return number of bytes written to the buffer
 */
static size_t read_coordinate(ExifData* exif, ExifTag tag, ExifTag ref,
                              char* buffer, size_t buffer_sz)
{
    const char* delimiters = ", ";
    ExifEntry* entry;
    char value[32];
    char* token;
    size_t index = 0;

    entry = exif_content_get_entry(exif->ifd[EXIF_IFD_GPS], tag);
    if (!entry) {
        return 0;
    }
    exif_entry_get_value(entry, value, sizeof(value));
    if (!*value) {
        return 0;
    }

    buffer[0] = 0;
    token = strtok(value, delimiters);
    while (token) {
        append(buffer, buffer_sz, token);
        switch (index++) {
            case 0:
                append(buffer, buffer_sz, "°");
                break;
            case 1:
                append(buffer, buffer_sz, "'");
                break;
            case 2:
                append(buffer, buffer_sz, "\"");
                break;
        }
        token = strtok(NULL, delimiters);
    }

    entry = exif_content_get_entry(exif->ifd[EXIF_IFD_GPS], ref);
    if (entry) {
        exif_entry_get_value(entry, value, sizeof(value));
        if (*value) {
            append(buffer, buffer_sz, value);
        }
    }

    return strlen(buffer);
}

/**
 * Read GPS location and add it to meta.
 * @param[in] img target image instance
 * @param[in] exif instance of EXIF reader
 */
static void read_location(image_t* img, ExifData* exif)
{
    size_t pos;
    char location[64];

    pos =
        read_coordinate(exif, EXIF_TAG_GPS_LATITUDE, EXIF_TAG_GPS_LATITUDE_REF,
                        location, sizeof(location));
    if (pos) {
        strcat(location, ", ");
        pos = strlen(location);
        if (read_coordinate(exif, EXIF_TAG_GPS_LONGITUDE,
                            EXIF_TAG_GPS_LONGITUDE_REF, location + pos,
                            sizeof(location) - pos)) {
            add_image_meta(img, "Location", location);
        }
    }
}

void read_exif(image_t* img, const uint8_t* data, size_t size)
{
    ExifData* exif = exif_data_new_from_data(data, (unsigned int)size);
    if (exif) {
        read_orientation(img, exif);

        add_meta(img, exif, EXIF_TAG_DATE_TIME, "DateTime");
        add_meta(img, exif, EXIF_TAG_MAKE, "Camera");
        add_meta(img, exif, EXIF_TAG_MODEL, "Model");
        add_meta(img, exif, EXIF_TAG_SOFTWARE, "Software");
        add_meta(img, exif, EXIF_TAG_EXPOSURE_TIME, "Exposure");
        add_meta(img, exif, EXIF_TAG_FNUMBER, "F Number");

        read_location(img, exif);

        exif_data_unref(exif);
    }
}

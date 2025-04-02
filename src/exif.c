// SPDX-License-Identifier: MIT
// EXIT reader.
// Copyright (C) 2022 Artem Senichev <artemsen@gmail.com>

#include "exif.h"

#include "formats/loader.h"

#include <libexif/exif-data.h>
#include <string.h>

/**
 * Fix orientation from EXIF data.
 * @param img target image instance
 * @param exif instance of EXIF reader
 */
static void fix_orientation(struct image* img, ExifData* exif)
{
    const ExifEntry* entry = exif_data_get_entry(exif, EXIF_TAG_ORIENTATION);
    if (entry) {
        const ExifByteOrder byte_order = exif_data_get_byte_order(exif);
        switch (exif_get_short(entry->data, byte_order)) {
            case 2: // flipped back-to-front
                image_flip_horizontal(img);
                break;
            case 3: // upside down
                image_rotate(img, 180);
                break;
            case 4: // flipped back-to-front and upside down
                image_flip_vertical(img);
                break;
            case 5: // flipped back-to-front and on its side
                image_flip_horizontal(img);
                image_rotate(img, 90);
                break;
            case 6: // on its side
                image_rotate(img, 90);
                break;
            case 7: // flipped back-to-front and on its far side
                image_flip_vertical(img);
                image_rotate(img, 270);
                break;
            case 8: // on its far side
                image_rotate(img, 270);
                break;
            default:
                break;
        }
    }
}

/**
 * Add meta info from EXIF tag.
 * @param img target image instance
 * @param exif instance of EXIF reader
 * @param tag EXIF tag
 * @param name EXIF tag name
 */
static void add_meta(struct image* img, ExifData* exif, ExifTag tag,
                     const char* name)
{
    char value[64];
    ExifEntry* entry = exif_data_get_entry(exif, tag);
    if (entry) {
        exif_entry_get_value(entry, value, sizeof(value));
        if (*value) {
            image_add_meta(img, name, "%s", value);
        }
    }
}

/**
 * Append string.
 * @param buffer destination buffer
 * @param buffer_max size of the buffer
 * @param value value to append
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
 * @param exif instance of EXIF reader
 * @param tag location tag
 * @param ref location reference
 * @param buffer destination buffer
 * @param buffer_sz size of the buffer
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
 * @param img target image instance
 * @param exif instance of EXIF reader
 */
static void read_location(struct image* img, ExifData* exif)
{
    char latitude[32], longitude[32];

    // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
    if (read_coordinate(exif, EXIF_TAG_GPS_LATITUDE, EXIF_TAG_GPS_LATITUDE_REF,
                        latitude, sizeof(latitude)) &&
        read_coordinate(exif, EXIF_TAG_GPS_LONGITUDE,
                        EXIF_TAG_GPS_LONGITUDE_REF, longitude,
                        sizeof(longitude))) {
        image_add_meta(img, "Location", "%s, %s", latitude, longitude);
    }
    // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
}

void process_exif(struct image* img, const uint8_t* data, size_t size)
{
    ExifData* exif = exif_data_new_from_data(data, (unsigned int)size);
    if (exif) {
        fix_orientation(img, exif);

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

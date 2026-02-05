// SPDX-License-Identifier: MIT
// EXIF reader.
// Copyright (C) 2026 Josef Litoš <invisiblemancz@gmail.com>

#include "exiftool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void add_if_empty(struct imgdata* data, const char* text)
{
    if (!data->info) {
        image_add_info(data, "Exif", "%s", text);
    }
}

void query_exiftool(const struct image* img, const char* arg_query)
{
    if (!img->data || (img->data->info && img->data->used_exiftool)) {
        return;
    }
    img->data->used_exiftool = true;

    FILE* fp;
    char command[512];
    const int buf_size = 512;
    char buffer[buf_size];
    size_t leftover_size = 0;

    // Skip if image is not a local file
    if (access(img->source, F_OK)) {
        add_if_empty(img->data, "Cannot run exiftool on URI links");
        return;
    }
    // Construct exiftool command
    snprintf(command, buf_size, "exiftool %s '%s'", arg_query, img->source);

    // Execute command
    fp = popen(command, "r");
    if (!fp) {
        fprintf(stderr, "Failed to run exiftool\n");
        add_if_empty(img->data, "Failed to run exiftool");
        return;
    }

    // when libexif ran first, the metadata is firmly set, we replace it with
    // user's choice
    bool needs_clear = true;

    while (!feof(fp)) {
        leftover_size +=
            fread(buffer + leftover_size, 1, buf_size - leftover_size - 1, fp);
        buffer[leftover_size] = '\0'; // Null-terminate the buffer

        char* start = buffer;
        char* newline;

        // Process each complete line (tag : value)
        while (start < buffer + leftover_size &&
               (newline = strchr(start, '\n'))) {
            *newline = '\0'; // terminate the end of the value
            char* tagEnd = strchr(start, ':');

            if (tagEnd) {
                if (needs_clear) {
                    needs_clear = false;
                    image_clear_info(img->data);
                }

                char* value = tagEnd;
                // trim spaces from both sides
                while (*start == ' ') {
                    start++;
                }
                while (*--tagEnd == ' ') { } // go before the ':' and trim
                while (*++value == ' ') { }  // go after the ':' and trimm
                *++tagEnd = '\0';            // terminate end of the tag
                image_add_info(img->data, start, "%s", value);
            }

            start = newline + 1;
        }
        if (start == buffer) {
            if (!needs_clear) {
                fprintf(stderr,
                        "warning: encountered an exif line longer"
                        " than %dB on '%s' - skipping all remaining tags\n",
                        buf_size, img->source);
            }
            break;
        }

        // Shift leftover data to the beginning of the buffer
        leftover_size = strlen(start);
        memmove(buffer, start, leftover_size);
    }

    // Put in a note to leave a mark to avoid retries
    add_if_empty(img->data, "No tags found");

    pclose(fp);
}

// SPDX-License-Identifier: MIT
// Copyright (C) 2021 Artem Senichev <artemsen@gmail.com>

#include "image.h"

#include "buildcfg.h"
#include "exif.h"
#include "formats/loader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Convert file size to human readable text.
 * @param[in] bytes file size in bytes
 * @param[out] text output text
 * @param[in] len size of output buffer
 */
static void human_size(uint64_t bytes, char* text, size_t len)
{
    const size_t kib = 1024;
    const size_t mib = kib * 1024;
    const size_t gib = mib * 1024;
    const size_t tib = gib * 1024;

    size_t multiplier;
    char prefix;
    if (bytes > tib) {
        multiplier = tib;
        prefix = 'T';
    } else if (bytes >= gib) {
        multiplier = gib;
        prefix = 'G';
    } else if (bytes >= mib) {
        multiplier = mib;
        prefix = 'M';
    } else {
        multiplier = kib;
        prefix = 'K';
    }

    snprintf(text, len, "%.02f %ciB", (double)bytes / multiplier, prefix);
}

/**
 * Create image instance from memory buffer.
 * @param[in] path path to the image
 * @param[in] data raw image data
 * @param[in] size size of image data in bytes
 * @return image instance or NULL on errors
 */
static image_t* image_create(const char* path, const uint8_t* data, size_t size)
{
    image_t* img;
    char meta[32];

    // create image instance
    img = calloc(1, sizeof(image_t));
    if (!img) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    img->path = path;

    // add general meta info
    add_image_info(img, "File", path);

    // decode image
    if (!image_decode(img, data, size)) {
        image_free(img);
        return NULL;
    }

    // add general meta info
    human_size(size, meta, sizeof(meta));
    add_image_info(img, "File size", meta);
    add_image_info(img, "Image size", "%lux%lu", img->width, img->height);

#ifdef HAVE_LIBEXIF
    // handle EXIF data
    read_exif(img, data, size);
#endif // HAVE_LIBEXIF

    return img;
}

image_t* image_from_file(const char* file)
{
    image_t* img = NULL;
    void* data = MAP_FAILED;
    struct stat st;
    int fd;

    // open file
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open file %s: %s\n", file, strerror(errno));
        goto done;
    }
    // get file size
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "Unable to get file stat for %s: %s\n", file,
                strerror(errno));
        goto done;
    }
    // map file to memory
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Unable to map shared file: [%i] %s\n", errno,
                strerror(errno));
        goto done;
    }

    img = image_create(file, data, st.st_size);
    if (!img) {
        fprintf(stderr, "Unsupported file format: %s\n", file);
    }

done:
    if (data != MAP_FAILED) {
        munmap(data, st.st_size);
    }
    if (fd != -1) {
        close(fd);
    }
    return img;
}

image_t* image_from_stdin(void)
{
    image_t* img = NULL;
    uint8_t* data = NULL;
    size_t size = 0;
    size_t capacity = 0;

    while (1) {
        if (size == capacity) {
            const size_t new_capacity = capacity + 256 * 1024;
            uint8_t* new_buf = realloc(data, new_capacity);
            if (!new_buf) {
                fprintf(stderr, "Not enough memory\n");
                goto done;
            }
            data = new_buf;
            capacity = new_capacity;
        }

        const ssize_t rc = read(STDIN_FILENO, data + size, capacity - size);
        if (rc == 0) {
            break;
        }
        if (rc == -1 && errno != EAGAIN) {
            perror("Error reading stdin");
            goto done;
        }
        size += rc;
    }

    if (data) {
        img = image_create("{STDIN}", data, size);
        if (!img) {
            fprintf(stderr, "Unsupported file format\n");
        }
    }

done:
    if (data) {
        free(data);
    }
    return img;
}

void image_free(image_t* img)
{
    if (img) {
        free(img->data);
        free((void*)img->info);
        free(img);
    }
}

void image_flip(image_t* img)
{
    const size_t stride = img->width * sizeof(img->data[0]);
    void* buffer = malloc(stride);
    if (buffer) {
        for (size_t y = 0; y < img->height / 2; ++y) {
            void* src = &img->data[y * img->width];
            void* dst = &img->data[(img->height - y - 1) * img->width];
            memcpy(buffer, dst, stride);
            memcpy(dst, src, stride);
            memcpy(src, buffer, stride);
        }
        free(buffer);
    }
}

static void add_image_meta(image_t* img, const char* key, const char* value)
{
    char* buffer = (char*)img->info;
    const char* delim = ":\t";
    const size_t cur_len = img->info ? strlen(img->info) + 1 : 0;
    const size_t add_len = strlen(key) + strlen(value) + strlen(delim) + 1;

    buffer = realloc(buffer, cur_len + add_len);
    if (buffer) {
        if (cur_len == 0) {
            buffer[0] = 0;
        } else {
            strcat(buffer, "\n");
        }
        strcat(buffer, key);
        strcat(buffer, delim);
        strcat(buffer, value);
        img->info = buffer;
    }
}

void add_image_info(image_t* img, const char* key, const char* fmt, ...)
{
    int len;
    va_list args;
    char* text;

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len > 0) {
        ++len; // last null
        text = malloc(len);
        if (text) {
            va_start(args, fmt);
            len = vsnprintf(text, len, fmt, args);
            va_end(args);
            if (len > 0) {
                add_image_meta(img, key, text);
            }
            free(text);
        }
    }
}

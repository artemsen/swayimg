// SPDX-License-Identifier: MIT
// Image loader.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "application.h"
#include "buildcfg.h"
#include "exif.h"
#include "imagelist.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Construct function name of loader
#define LOADER_FUNCTION(name) decode_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                     \
    enum loader_status LOADER_FUNCTION(name)(struct image * ctx, \
                                             const uint8_t* data, size_t size)

const char* supported_formats = "bmp, pnm, farbfeld, tga, dicom"
#ifdef HAVE_LIBJPEG
                                ", jpeg"
#endif
#ifdef HAVE_LIBPNG
                                ", png"
#endif
#ifdef HAVE_LIBGIF
                                ", gif"
#endif
#ifdef HAVE_LIBWEBP
                                ", webp"
#endif
#ifdef HAVE_LIBRSVG
                                ", svg"
#endif
#ifdef HAVE_LIBHEIF
                                ", heif, avif"
#endif
#ifdef HAVE_LIBAVIF
#ifndef HAVE_LIBHEIF
                                ", avif"
#endif
                                ", avifs"
#endif
#ifdef HAVE_LIBJXL
                                ", jxl"
#endif
#ifdef HAVE_LIBEXR
                                ", exr"
#endif
#ifdef HAVE_LIBTIFF
                                ", tiff"
#endif
#ifdef HAVE_LIBSIXEL
                                ", sixel"
#endif
    ;

// declaration of loaders
LOADER_DECLARE(bmp);
LOADER_DECLARE(dicom);
LOADER_DECLARE(farbfeld);
LOADER_DECLARE(pnm);
LOADER_DECLARE(qoi);
LOADER_DECLARE(tga);
#ifdef HAVE_LIBEXR
LOADER_DECLARE(exr);
#endif
#ifdef HAVE_LIBGIF
LOADER_DECLARE(gif);
#endif
#ifdef HAVE_LIBHEIF
LOADER_DECLARE(heif);
#endif
#ifdef HAVE_LIBAVIF
LOADER_DECLARE(avif);
#endif
#ifdef HAVE_LIBJPEG
LOADER_DECLARE(jpeg);
#endif
#ifdef HAVE_LIBJXL
LOADER_DECLARE(jxl);
#endif
#ifdef HAVE_LIBPNG
LOADER_DECLARE(png);
#endif
#ifdef HAVE_LIBRSVG
LOADER_DECLARE(svg);
#endif
#ifdef HAVE_LIBTIFF
LOADER_DECLARE(tiff);
#endif
#ifdef HAVE_LIBSIXEL
LOADER_DECLARE(sixel);
#endif
#ifdef HAVE_LIBWEBP
LOADER_DECLARE(webp);
#endif

// clang-format off
// list of available decoders
static const image_decoder decoders[] = {
#ifdef HAVE_LIBJPEG
    &LOADER_FUNCTION(jpeg),
#endif
#ifdef HAVE_LIBPNG
    &LOADER_FUNCTION(png),
#endif
#ifdef HAVE_LIBGIF
    &LOADER_FUNCTION(gif),
#endif
    &LOADER_FUNCTION(bmp),
    &LOADER_FUNCTION(pnm),
    &LOADER_FUNCTION(dicom),
#ifdef HAVE_LIBWEBP
    &LOADER_FUNCTION(webp),
#endif
#ifdef HAVE_LIBHEIF
    &LOADER_FUNCTION(heif),
#endif
#ifdef HAVE_LIBAVIF
    &LOADER_FUNCTION(avif),
#endif
#ifdef HAVE_LIBRSVG
    &LOADER_FUNCTION(svg),
#endif
#ifdef HAVE_LIBJXL
    &LOADER_FUNCTION(jxl),
#endif
#ifdef HAVE_LIBEXR
    &LOADER_FUNCTION(exr),
#endif
#ifdef HAVE_LIBTIFF
    &LOADER_FUNCTION(tiff),
#endif
#ifdef HAVE_LIBSIXEL
    &LOADER_FUNCTION(sixel),
#endif
    &LOADER_FUNCTION(qoi),
    &LOADER_FUNCTION(farbfeld),
    &LOADER_FUNCTION(tga) // should be the last one
};
// clang-format on

/** Background thread loader queue. */
struct loader_queue {
    struct list list; ///< Links to prev/next entry
    size_t index;     ///< Index of the image to load
};

/** Loader context. */
struct loader {
    pthread_t tid;              ///< Background loader thread id
    struct loader_queue* queue; ///< Background thread loader queue
    pthread_mutex_t lock;       ///< Queue access lock
    pthread_cond_t signal;      ///< Queue notification
    pthread_cond_t ready;       ///< Thread ready signal
};

/** Global loader context instance. */
static struct loader ctx;

/**
 * Load image from memory buffer.
 * @param img destination image
 * @param data raw image data
 * @param size size of image data in bytes
 * @return loader status
 */
static enum loader_status image_from_memory(struct image* img,
                                            const uint8_t* data, size_t size)
{
    enum loader_status status = ldr_unsupported;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(decoders) && status == ldr_unsupported; ++i) {
        status = decoders[i](img, data, size);
    }

    img->file_size = size;

#ifdef HAVE_LIBEXIF
    process_exif(img, data, size);
#endif

    return status;
}

/**
 * Load image from file.
 * @param img destination image
 * @param file path to the file to load
 * @return loader status
 */
static enum loader_status image_from_file(struct image* img, const char* file)
{
    enum loader_status status = ldr_ioerror;
    void* data = MAP_FAILED;
    struct stat st;
    int fd;

    // check file type
    if (stat(file, &st) == -1 || !S_ISREG(st.st_mode)) {
        return ldr_ioerror;
    }

    // open file and map it to memory
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        return ldr_ioerror;
    }
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return ldr_ioerror;
    }

    // load from mapped memory
    status = image_from_memory(img, data, st.st_size);

    munmap(data, st.st_size);
    close(fd);

    return status;
}

/**
 * Load image from stream file (stdin).
 * @param img destination image
 * @param fd file descriptor for read
 * @return loader status
 */
static enum loader_status image_from_stream(struct image* img, int fd)
{
    enum loader_status status = ldr_ioerror;
    uint8_t* data = NULL;
    size_t size = 0;
    size_t capacity = 0;

    while (true) {
        ssize_t rc;

        if (size == capacity) {
            const size_t new_capacity = capacity + 256 * 1024;
            uint8_t* new_buf = realloc(data, new_capacity);
            if (!new_buf) {
                break;
            }
            data = new_buf;
            capacity = new_capacity;
        }

        rc = read(fd, data + size, capacity - size);
        if (rc == 0) {
            status = image_from_memory(img, data, size);
            break;
        }
        if (rc == -1 && errno != EAGAIN) {
            break;
        }
        size += rc;
    }

    free(data);
    return status;
}

/**
 * Load image from stdout printed by external command.
 * @param img destination image
 * @param cmd execution command to get stdout data
 * @return loader status
 */
static enum loader_status image_from_exec(struct image* img, const char* cmd)
{
    enum loader_status status = ldr_ioerror;
    int pfd[2];
    pid_t pid;

    if (pipe(pfd) == -1) {
        return ldr_ioerror;
    }

    pid = fork();
    if (pid == -1) {
        close(pfd[1]);
        close(pfd[0]);
        return ldr_ioerror;
    }

    if (pid) { // parent
        close(pfd[1]);
        status = image_from_stream(img, pfd[0]);
        close(pfd[0]);
        waitpid(pid, NULL, 0);
    } else { // child
        const char* shell = getenv("SHELL");
        if (!shell || !*shell) {
            shell = "/bin/sh";
        }
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        close(pfd[0]);
        execlp(shell, shell, "-c", cmd, NULL);
        exit(1);
    }

    return status;
}

enum loader_status loader_from_source(const char* source, struct image** image)
{
    enum loader_status status;
    struct image* img;

    // create image instance
    img = image_alloc();
    if (!img) {
        return ldr_ioerror;
    }

    // decode image
    if (strcmp(source, LDRSRC_STDIN) == 0) {
        status = image_from_stream(img, STDIN_FILENO);
    } else if (strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        status = image_from_exec(img, source + LDRSRC_EXEC_LEN);
    } else {
        status = image_from_file(img, source);
    }

    if (status == ldr_success) {
        image_set_source(img, source);
        *image = img;
    } else {
        image_free(img);
    }

    return status;
}

enum loader_status loader_from_index(size_t index, struct image** image)
{
    enum loader_status status = ldr_ioerror;
    const char* source = image_list_get(index);

    if (source) {
        status = loader_from_source(source, image);
        if (status == ldr_success) {
            (*image)->index = index;
        }
    }

    return status;
}

/** Image loader executed in background thread. */
static void* loading_thread(__attribute__((unused)) void* data)
{
    struct loader_queue* entry;
    struct image* image;

    do {
        pthread_mutex_lock(&ctx.lock);
        pthread_cond_signal(&ctx.ready);
        while (!ctx.queue) {
            pthread_cond_wait(&ctx.signal, &ctx.lock);
            if (!ctx.queue) {
                pthread_cond_signal(&ctx.ready);
            }
        }
        entry = ctx.queue;
        ctx.queue = list_remove(entry);
        pthread_mutex_unlock(&ctx.lock);

        if (entry->index == IMGLIST_INVALID) {
            free(entry);
            return NULL;
        }

        image = NULL;
        loader_from_index(entry->index, &image);
        app_on_load(image, entry->index);
        free(entry);
    } while (true);

    return NULL;
}

void loader_init(void)
{
    pthread_cond_init(&ctx.signal, NULL);
    pthread_cond_init(&ctx.ready, NULL);

    pthread_mutex_init(&ctx.lock, NULL);
    pthread_mutex_lock(&ctx.lock);

    pthread_create(&ctx.tid, NULL, loading_thread, NULL);

    pthread_cond_wait(&ctx.ready, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);
}

void loader_destroy(void)
{
    if (ctx.tid) {
        loader_queue_reset();
        loader_queue_append(IMGLIST_INVALID); // send stop signal
        pthread_join(ctx.tid, NULL);

        pthread_mutex_destroy(&ctx.lock);
        pthread_cond_destroy(&ctx.signal);
        pthread_cond_destroy(&ctx.ready);
    }
}

void loader_queue_append(size_t index)
{
    struct loader_queue* entry = malloc(sizeof(*entry));
    if (entry) {
        entry->index = index;
        pthread_mutex_lock(&ctx.lock);
        ctx.queue = list_append(ctx.queue, entry);
        pthread_cond_signal(&ctx.signal);
        pthread_mutex_unlock(&ctx.lock);
    }
}

void loader_queue_reset(void)
{
    pthread_mutex_lock(&ctx.lock);
    list_for_each(ctx.queue, struct loader_queue, it) {
        free(it);
    }
    ctx.queue = NULL;
    pthread_cond_signal(&ctx.signal);
    pthread_cond_wait(&ctx.ready, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);
}

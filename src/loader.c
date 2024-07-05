// SPDX-License-Identifier: MIT
// Image loader and cache.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "loader.h"

#include "buildcfg.h"
#include "cache.h"
#include "config.h"
#include "exif.h"
#include "imagelist.h"
#include "str.h"
#include "ui.h"
#include "viewer.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

// Construct function name of loader
#define LOADER_FUNCTION(name) decode_##name
// Declaration of loader function
#define LOADER_DECLARE(name)                                     \
    enum loader_status LOADER_FUNCTION(name)(struct image * ctx, \
                                             const uint8_t* data, size_t size)

const char* supported_formats = "bmp, pnm, tga"
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
    ;

// declaration of loaders
LOADER_DECLARE(bmp);
LOADER_DECLARE(pnm);
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
#ifdef HAVE_LIBWEBP
LOADER_DECLARE(webp);
#endif

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
    &LOADER_FUNCTION(bmp),  &LOADER_FUNCTION(pnm),
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
    &LOADER_FUNCTION(tga),
};

/** Loader context. */
struct loader {
    struct image* cur_img;       ///< Current image handle
    size_t cur_idx;              ///< Index of the current image in image list
    struct cache_queue previous; ///< Cache of previously viewed images
    size_t previous_num;         ///< Max number of cached images
    struct cache_queue preload;  ///< Queue of preloaded images
    size_t preload_num;          ///< Max number of images to preload
    pthread_t preloader;         ///< Preload thread
#ifdef HAVE_INOTIFY
    int notify; ///< inotify file handler
    int watch;  ///< Current file watcher
#endif
};
static struct loader ctx = {
    .cur_idx = IMGLIST_INVALID,
    .previous_num = 1,
    .preload_num = 1,
#ifdef HAVE_INOTIFY
    .notify = -1,
    .watch = -1,
#endif
};

/** Preload operation. */
enum preloader_op {
    preloader_stop,
    preloader_start,
};

/** Image preloader executed in background thread. */
static void* preloader_thread(__attribute__((unused)) void* data)
{
    // size_t index = ctx.cur_idx;
    size_t index = image_list_next_file(ctx.cur_idx);
    const char* source;
    struct image* img;

    while (index != IMGLIST_INVALID) {
        if (index == ctx.cur_idx) {
            break;
        }
        // check previously viewed cache
        img = cache_get(&ctx.previous, index);
        if (!img) {
            // check preload cache
            img = cache_get(&ctx.preload, index);
        }
        if (img) {
            cache_put(&ctx.preload, img, index);
            index = image_list_next_file(index);
        } else {
            // get next source
            source = image_list_get(index);
            while (index != IMGLIST_INVALID && !source) {
                index = image_list_next_file(index);
                source = image_list_get(index);
            }
            if (!source) {
                break;
            }

            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

            // load image
            img = loader_load_image(source, NULL);
            if (!img) {
                index = image_list_skip(index);
            } else {
                cache_put(&ctx.preload, img, index);
                index = image_list_next_file(index);
            }

            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        }
        if (cache_full(&ctx.preload)) {
            break;
        }
    }

    return NULL;
}

/**
 * Stop or restart background thread to preload adjacent images.
 * @param op preloader operation
 */
static void preloader_ctl(enum preloader_op op)
{
    if (ctx.preload_num == 0) {
        return; // preload disabled
    }

    if (ctx.preloader) {
        pthread_cancel(ctx.preloader);
        pthread_join(ctx.preloader, NULL);
        ctx.preloader = 0;
    }

    if (op == preloader_start) {
        pthread_create(&ctx.preloader, NULL, preloader_thread, NULL);
    }
}

/**
 * Load first (initial) image.
 * @param start initial index of image in the image list
 * @param force mandatory image index flag
 * @return loader status
 */
static enum loader_status load_first(size_t start, bool force)
{
    enum loader_status status = ldr_ioerror;

    if (force && start != IMGLIST_INVALID) {
        const char* source = image_list_get(start);
        if (source) {
            struct image* img = loader_load_image(source, &status);
            if (img) {
                ctx.cur_img = img;
                ctx.cur_idx = start;
            }
        }
    } else {
        if (start == IMGLIST_INVALID) {
            start = 0;
        }
        while (start != IMGLIST_INVALID) {
            const char* source = image_list_get(start);
            if (source) {
                struct image* img = loader_load_image(source, &status);
                if (img) {
                    ctx.cur_img = img;
                    ctx.cur_idx = start;
                    break;
                }
            }
            start = image_list_skip(start);
        }
    }

    return status;
}

#ifdef HAVE_INOTIFY
/**
 * Register watcher for current file.
 */
static void watch_current(void)
{
    if (ctx.notify >= 0) {
        if (ctx.watch != -1) {
            inotify_rm_watch(ctx.notify, ctx.watch);
            ctx.watch = -1;
        }
        ctx.watch = inotify_add_watch(ctx.notify, ctx.cur_img->source,
                                      IN_CLOSE_WRITE | IN_MOVE_SELF);
    }
}

/**
 * Notify handler.
 */
static void on_notify(void)
{
    while (true) {
        bool updated = false;
        uint8_t buffer[1024];
        ssize_t pos = 0;
        const ssize_t len = read(ctx.notify, buffer, sizeof(buffer));

        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // something went wrong
        }

        while (pos + sizeof(struct inotify_event) <= (size_t)len) {
            const struct inotify_event* event =
                (struct inotify_event*)&buffer[pos];
            if (event->mask & IN_IGNORED) {
                ctx.watch = -1;
            } else {
                updated = true;
            }
            pos += sizeof(struct inotify_event) + event->len;
        }
        if (updated) {
            viewer_reload();
        }
    }
}
#endif // HAVE_INOTIFY

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

    // open file and get its size
    fd = open(file, O_RDONLY);
    if (fd == -1) {
        return ldr_ioerror;
    }
    if (fstat(fd, &st) == -1) {
        close(fd);
        return ldr_ioerror;
    }

    // map file to memory
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
        return ldr_ioerror;
    }

    if (pid == 0) { // child
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        close(pfd[0]);
        execlp("sh", "/bin/sh", "-c", cmd, NULL);
        exit(1);
    } else { // parent
        close(pfd[1]);
        status = image_from_stream(img, pfd[0]);
        close(pfd[0]);
        waitpid(pid, NULL, 0);
    }

    return status;
}

struct image* loader_load_image(const char* source, enum loader_status* status)
{
    struct image* img = NULL;
    enum loader_status rc = ldr_ioerror;

    img = image_create();
    if (!img) {
        return NULL;
    }

    // save image source info
    img->source = str_dup(source, NULL);
    img->name = strrchr(img->source, '/');
    if (!img->name || strcmp(img->name, "/") == 0) {
        img->name = img->source;
    } else {
        ++img->name; // skip slash
    }

    // decode image
    if (strcmp(source, LDRSRC_STDIN) == 0) {
        rc = image_from_stream(img, STDIN_FILENO);
    } else if (strncmp(source, LDRSRC_EXEC, LDRSRC_EXEC_LEN) == 0) {
        rc = image_from_exec(img, source + LDRSRC_EXEC_LEN);
    } else {
        rc = image_from_file(img, source);
    }

    if (rc != ldr_success) {
        image_free(img);
        img = NULL;
    }

    if (status) {
        *status = rc;
    }

    return img;
}

struct image* loader_get_image(size_t index)
{
    struct image* img = NULL;

    if (ctx.cur_img && ctx.cur_idx == index) {
        return ctx.cur_img;
    }

    // search in cache
    img = cache_get(&ctx.previous, index);

    // search in preload
    if (!img) {
        preloader_ctl(preloader_stop);
        img = cache_get(&ctx.preload, index);
    }

    // load
    if (!img) {
        const char* source = image_list_get(index);
        if (source) {
            img = loader_load_image(source, NULL);
        }
    }

    if (img) {
        // don't cache skipped images
        if (image_list_get(ctx.cur_idx)) {
            cache_put(&ctx.previous, ctx.cur_img, ctx.cur_idx);
        } else {
            image_free(ctx.cur_img);
        }

        ctx.cur_img = img;
        ctx.cur_idx = index;

        preloader_ctl(preloader_start);

#ifdef HAVE_INOTIFY
        watch_current();
#endif
    }

    return img;
}

/**
 * Custom section loader, see `config_loader` for details.
 */
static enum config_status load_config(const char* key, const char* value)
{
    enum config_status status = cfgst_invalid_value;

    if (strcmp(key, IMGLIST_CFG_CACHE) == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num > 0 && num < 1024) {
            ctx.previous_num = num;
            status = cfgst_ok;
        }
    } else if (strcmp(key, IMGLIST_CFG_PRELOAD) == 0) {
        ssize_t num;
        if (str_to_num(value, 0, &num, 0) && num > 0 && num < 1024) {
            ctx.preload_num = num;
            status = cfgst_ok;
        }
    } else {
        status = cfgst_invalid_key;
    }

    return status;
}

void loader_create(void)
{
    // register configuration loader
    config_add_loader(IMGLIST_CFG_SECTION, load_config);
}

bool loader_init(size_t start, bool force)
{
    enum loader_status status;

    cache_init(&ctx.previous, ctx.previous_num);
    cache_init(&ctx.preload, ctx.preload_num);

#ifdef HAVE_INOTIFY
    ctx.notify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (ctx.notify >= 0) {
        ui_add_event(ctx.notify, on_notify);
    }
#endif

    // load the first image
    status = load_first(start, force);
    if (status == ldr_success) {
        preloader_ctl(preloader_start);
#ifdef HAVE_INOTIFY
        watch_current();
#endif
    } else if (!force) {
        fprintf(stderr, "No image files found to view, exit\n");
    } else {
        const char* reason = "Unknown error";
        switch (status) {
            case ldr_success:
                break;
            case ldr_unsupported:
                reason = "Unsupported format";
                break;
            case ldr_fmterror:
                reason = "Invalid format";
                break;
            case ldr_ioerror:
                reason = "I/O error";
                break;
        }
        fprintf(stderr, "%s: %s\n", image_list_get(start), reason);
    }

    return (status == ldr_success);
}

void loader_free(void)
{
    preloader_ctl(preloader_stop);
    cache_free(&ctx.previous);
    cache_free(&ctx.preload);
}

bool loader_reset(void)
{
    struct image* img;

    preloader_ctl(preloader_stop);
    cache_reset(&ctx.previous);
    cache_reset(&ctx.preload);

    img = loader_load_image(ctx.cur_img->source, NULL);
    if (img) {
        image_free(ctx.cur_img);
        ctx.cur_img = img;
        preloader_ctl(preloader_start);
        return true;
    }

    return false;
}

struct image* loader_current_image(void)
{
    return ctx.cur_img;
}

size_t loader_current_index(void)
{
    return ctx.cur_idx;
}

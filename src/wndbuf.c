// SPDX-License-Identifier: MIT
// Wayland window surface buffer.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "wndbuf.h"

#include "buildcfg.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct wl_buffer* wndbuf_create(struct wl_shm* shm, size_t width, size_t height)
{
    assert(shm);
    assert(width > 0);
    assert(height > 0);

    const size_t stride = width * sizeof(argb_t);
    const size_t data_sz = stride * height;
    const size_t buffer_sz = data_sz + sizeof(struct pixmap);

    struct pixmap* pm;
    struct wl_buffer* buffer;
    struct wl_shm_pool* pool;

    static size_t counter = 0;
    int fd = -1;
    char path[64];
    void* data;

    // generate unique file name
    snprintf(path, sizeof(path), "/" APP_NAME "_%x_%zx", getpid(), ++counter);

    // open shared mem
    fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd == -1) {
        const int err = errno;
        fprintf(stderr, "Unable to create shared file %s: [%i] %s\n", path, err,
                strerror(err));
        return NULL;
    }
    shm_unlink(path);

    // set shared memory size
    if (ftruncate(fd, buffer_sz) == -1) {
        const int err = errno;
        fprintf(stderr, "Unable to truncate shared file %s: [%i] %s\n", path,
                err, strerror(err));
        close(fd);
        return NULL;
    }

    // get data pointer of the shared mem
    data = mmap(NULL, buffer_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        const int err = errno;
        fprintf(stderr, "Unable to map shared file %s: [%i] %s\n", path, err,
                strerror(err));
        close(fd);
        return NULL;
    }

    // fill buffer user data
    pm = (struct pixmap*)((uint8_t*)data + data_sz);
    pm->width = width;
    pm->height = height;
    pm->data = data;

    // create wayland buffer
    pool = wl_shm_create_pool(shm, fd, buffer_sz);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                       WL_SHM_FORMAT_ARGB8888);
    wl_buffer_set_user_data(buffer, pm);

    wl_shm_pool_destroy(pool);
    close(fd);

    return buffer;
}

struct pixmap* wndbuf_pixmap(struct wl_buffer* buffer)
{
    assert(buffer);
    return wl_buffer_get_user_data(buffer);
}

void wndbuf_free(struct wl_buffer* buffer)
{
    if (buffer) {
        struct pixmap* pm = wl_buffer_get_user_data(buffer);
        const size_t sz =
            pm->width * pm->height * sizeof(argb_t) + sizeof(struct pixmap);
        munmap(pm->data, sz);
        wl_buffer_destroy(buffer);
    }
}

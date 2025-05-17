// SPDX-License-Identifier: MIT
// Thumbnail layout for gallery mode.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "layout.h"

#include "imglist.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Minimal space between thumbnails */
#define MIN_PADDING 5

/**
 * Recalculate thumbnails scheme.
 * @param lo pointer to the thumbnail layout
 * @param visible total number of visible thumbnails
 * @return the first visible image (top left corner of the window)
 */
static struct image* rearrange(struct layout* lo, size_t* visible)
{
    assert(lo->current);

    const size_t max_thumb = lo->rows * lo->columns;
    struct image* first;
    struct image* last;
    ssize_t distance;

    // set preliminary position for the currently selected image
    distance = imglist_distance(imglist_first(), lo->current);
    lo->current_col = distance % lo->columns;
    if (lo->current_row == SIZE_MAX) {
        lo->current_row = lo->rows / 2;
    } else if (lo->current_row >= lo->rows - 1) {
        lo->current_row = lo->rows >= 2 ? lo->rows - 2 : 0;
    }

    // get the first visible images
    distance = lo->current_row * lo->columns + lo->current_col;
    first = imglist_jump(lo->current, -distance);
    if (!first) {
        first = imglist_first();
    }

    // get the last visible images
    last = imglist_jump(first, max_thumb - 1);
    if (!last) {
        last = imglist_last();
        if (first != imglist_first()) {
            // scroll to fill the entire window
            const size_t last_col = (imglist_size() - 1) % lo->columns;
            distance = max_thumb - (lo->columns - last_col);
            first = imglist_jump(imglist_last(), -distance);
            if (!first) {
                first = imglist_first();
            }
        }
    }

    lo->current_row = imglist_distance(first, lo->current) / lo->columns;

    // don't use the last row (usually it's only partially visible)
    if (lo->current_row >= lo->rows - 1) {
        lo->current_row = lo->rows >= 2 ? lo->rows - 2 : 0;
        distance = lo->current_row * lo->columns + lo->current_col;
        first = imglist_jump(lo->current, -distance);
    }

    assert(first);
    assert(last);

    if (visible) {
        *visible = imglist_distance(first, last) + 1;
    }
    return first;
}

void layout_init(struct layout* lo, size_t thumb_size)
{
    memset(lo, 0, sizeof(*lo));
    lo->thumb_size = thumb_size;
    lo->current_row = SIZE_MAX;
}

void layout_free(struct layout* lo)
{
    if (lo->thumbs) {
        free(lo->thumbs);
        lo->thumbs = NULL;
        lo->thumb_total = 0;
    }
}

void layout_update(struct layout* lo)
{
    assert(imglist_is_locked());

    size_t total;
    struct image* img;

    img = rearrange(lo, &total);

    // realloc thumbnails map
    if (total > lo->thumb_total) {
        struct layout_thumb* thumbs;
        thumbs = realloc(lo->thumbs, total * sizeof(struct layout_thumb));
        if (!thumbs) {
            return;
        }
        lo->thumbs = thumbs;
    }

    // fill thumbnails map
    lo->thumb_total = total;
    for (size_t i = 0; i < lo->thumb_total; ++i) {
        struct layout_thumb* thumb = &lo->thumbs[i];
        const size_t col = i % lo->columns;
        const size_t row = i / lo->columns;
        thumb->x = col * lo->thumb_size + lo->padding * (col + 1);
        thumb->y = row * lo->thumb_size + lo->padding * (row + 1);
        thumb->img = img;
        img = imglist_next(img);
    }

    assert(lo->current ==
           lo->thumbs[lo->current_row * lo->columns + lo->current_col].img);
}

void layout_resize(struct layout* lo, size_t width, size_t height)
{
    // calculate number of columns/rows and padding size between thumbnails
    lo->columns = width / (lo->thumb_size + MIN_PADDING);
    if (lo->columns == 0) {
        lo->columns = 1;
    }
    lo->padding = (width - (lo->columns * lo->thumb_size)) / (lo->columns + 1);
    lo->rows = (height - lo->padding) / (lo->thumb_size + lo->padding);
    ++lo->rows; // partially visible row

    layout_update(lo);
}

bool layout_select(struct layout* lo, enum layout_dir dir)
{
    assert(imglist_is_locked());

    ssize_t col = lo->current_col;
    ssize_t row = lo->current_row;
    struct image* next = NULL;

    switch (dir) {
        case layout_up:
            next = imglist_jump(lo->current, -(ssize_t)lo->columns);
            --row;
            break;
        case layout_down:
            next = imglist_jump(lo->current, lo->columns);
            ++row;
            break;
        case layout_left:
            next = imglist_prev(lo->current);
            --col;
            break;
        case layout_right:
            next = imglist_next(lo->current);
            ++col;
            break;
        case layout_first:
            next = imglist_first();
            row = 0;
            break;
        case layout_last:
            next = imglist_last();
            row = 0;
            break;
        case layout_pgup:
            next = imglist_jump(lo->current,
                                -(ssize_t)lo->columns * (lo->rows - 1));
            if (!next) {
                next = imglist_jump(imglist_first(), lo->current_col);
                row = 0;
            }
            break;
        case layout_pgdown:
            next = imglist_jump(lo->current, lo->columns * (lo->rows - 1));
            break;
    }

    if (next == lo->current) {
        next = NULL;
    }

    if (next) {
        if (col < 0) {
            --row;
        } else if (col >= (ssize_t)lo->columns) {
            ++row;
        }
        if (row < 0) {
            lo->current_row = 0;
        } else {
            lo->current_row = row;
        }
        lo->current = next;
        layout_update(lo);
    }

    return next;
}

struct layout_thumb* layout_current(struct layout* lo)
{
    const size_t idx = lo->current_row * lo->columns + lo->current_col;
    assert(idx < lo->thumb_total && lo->thumbs[idx].img);
    assert(lo->thumbs[idx].img == lo->current);
    return &lo->thumbs[idx];
}

struct image* layout_ldqueue(struct layout* lo, size_t preload)
{
    assert(imglist_is_locked());

    struct image* queue = NULL;
    struct image* first = lo->thumbs[0].img;
    struct image* last = lo->thumbs[lo->thumb_total - 1].img;
    struct image* fwd = layout_current(lo)->img;
    struct image* back = imglist_prev(fwd);
    bool fwd_visible = true;
    bool back_visible = true;
    bool forward = true;

    while (fwd || back) {
        const struct image* next = NULL;

        if (forward && !fwd) {
            forward = false;
        } else if (!forward && !back) {
            forward = true;
        }
        next = (forward ? fwd : back);
        assert(next);

        if (!image_thumb_get(next)) {
            queue = list_append(queue, image_create(next->source));
        }

        if (forward && fwd) {
            if (fwd_visible) {
                fwd_visible = (fwd != last);
            }
            fwd = imglist_next(fwd);
            if (fwd && !fwd_visible) {
                if (preload) {
                    --preload;
                } else {
                    fwd = NULL;
                }
            }
        } else if (!forward && back) {
            if (back_visible) {
                back_visible = (back != first);
            }
            back = imglist_prev(back);
            if (back && !fwd_visible) {
                if (preload) {
                    --preload;
                } else {
                    back = NULL;
                }
            }
        }

        forward = !forward;
    }

    return queue;
}

void layout_clear(struct layout* lo, size_t preserve)
{
    assert(imglist_is_locked());

    struct image* fwd;
    struct image* back;
    bool forward = true;

    fwd = imglist_next(lo->thumbs[lo->thumb_total - 1].img);
    back = imglist_prev(lo->thumbs[0].img);

    // get iterators out of cached range
    while (preserve && (fwd || back)) {
        if (forward && !fwd) {
            forward = false;
        } else if (!forward && !back) {
            forward = true;
        }

        if (forward && fwd) {
            fwd = imglist_next(fwd);
            if (fwd) {
                --preserve;
            }
        } else if (!forward && back) {
            back = imglist_prev(back);
            if (back) {
                --preserve;
            }
        }

        forward = !forward;
    }

    // free thumbnails
    while (fwd) {
        image_free(fwd, IMGDATA_THUMB);
        fwd = imglist_next(fwd);
    }
    while (back) {
        image_free(back, IMGDATA_THUMB);
        back = imglist_prev(back);
    }
}

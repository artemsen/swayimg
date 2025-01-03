// SPDX-License-Identifier: MIT
// Scaling pixmaps.
// Copyright (C) 2024 Abe Wieland <abe.wieland@gmail.com>

#include "loader.h"
#include "pixmap_internal.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

// ========== KERNELS ==========
// 1D convolution kernels: each output is the weighted sum of a set of inputs.
// The weights are stored in fixed point to limit memory consumption and improve
// performance when applying. Every scale but the nearest neighbor scale is
// implemented using a horizontal then vertical pass of a 1D kernel.

// 14-bit fixed point means we still comfortably fit within a 16-bit signed
// integer, including those weights which are slightly negative or a little
// over 1
#define FIXED_BITS 14

struct output {
    size_t first; ///< First input for this output
    size_t n;     ///< Number of inputs for this output
    size_t index; ///< Index of first weight in weights array
};

struct kernel {
    size_t start_out; ///< First output
    size_t n_out;     ///< Number of outputs
    size_t start_in;  ///< First input
    size_t n_in;      ///< Number of inputs
    struct output* o; ///< Outputs
    int16_t* weights; ///< Weights
};

// Each kernel is defined by a window (outside of which it's zero) and a
// symmetric window function defining its weight within that window
typedef double (*window_fn)(double);

struct bounds {
    ssize_t first;
    ssize_t last;
};

// Get the first and last input for a given output
static inline struct bounds get_bounds(size_t out, double scale, double window)
{
    // Adjust by 0.5 to ensure sampling from the centers of pixels,
    // not their edges
    double c = (out + 0.5) / scale - 0.5;
    double d = window / fmin(scale, 1.0);
    ssize_t f = floor(c - d);
    ssize_t l = ceil(c + d);
    struct bounds b = { f, l };
    return b;
}

// Get the weight for a given input/output pair
static inline double get_weight(size_t in, size_t out, double scale,
                                double window, window_fn f)
{
    if (scale >= 1.0) {
        double c = (out + 0.5) / scale - 0.5;
        double x = fabs(in - c);
        return x > window ? 0.0 : f(x);
    } else {
        double c = (in + 0.5) * scale - 0.5;
        double x = fabs(out - c);
        return x > window ? 0.0 : f(x);
    }
}

// Build a new fixed point kernel, given the window and window function
static inline void new_kernel(struct kernel* k, size_t nin, size_t nout,
                              ssize_t offset, double scale, double window,
                              window_fn f)
{
    size_t start = max(0, offset);
    size_t end = min(nout, (size_t)(offset + nin * scale));
    k->start_out = start;
    k->n_out = end - start;
    k->o = malloc(k->n_out * sizeof(*k->o));
    struct bounds b = get_bounds(0, scale, window);
    // due to floor and ceil, we need at least 2 extra to be safe, so 3
    // certainly suffices
    size_t n_per = b.last - b.first + 3;
    double* w = malloc(n_per * sizeof(*w));
    int16_t* iw = malloc(n_per * sizeof(*iw));
    // this means we do overallocate here, but kernels are only live for a short
    // time anyway
    k->weights = malloc(n_per * k->n_out * sizeof(*k->weights));
    size_t min_in = SIZE_MAX;
    size_t max_in = 0;
    size_t index = 0;
    for (size_t out = start; out < end; ++out) {
        struct output* o = &k->o[out - start];
        struct bounds b = get_bounds(out - offset, scale, window);
        size_t first = max(0, b.first);
        size_t last = min(nin - 1, (size_t)b.last);
        double sum = 0;
        for (size_t in = first; in <= last; ++in) {
            w[in - first] = get_weight(in, out - offset, scale, window, f);
            sum += w[in - first];
        }
        double norm = 1.0 / sum;
        for (size_t in = first; in <= last; ++in) {
            // TODO if more accuracy is needed, round may help
            iw[in - first] = w[in - first] * norm * (1 << FIXED_BITS);
        }
        size_t tfirst, tlast;
        for (tfirst = first; tfirst < last && iw[tfirst - first] == 0; ++tfirst)
            ;
        for (tlast = last; tlast > tfirst && iw[tlast - first] == 0; --tlast)
            ;
        if (tfirst < min_in) {
            min_in = tfirst;
        }
        if (tlast > max_in) {
            max_in = tlast;
        }
        o->n = tlast - tfirst + 1;
        o->first = tfirst;
        o->index = index;
        memcpy(&k->weights[index], &iw[tfirst - first],
               o->n * sizeof(*k->weights));
        index += o->n;
    }
    k->start_in = min_in;
    k->n_in = max_in - min_in + 1;
    free(w);
    free(iw);
}

static inline void free_kernel(struct kernel* k)
{
    free(k->o);
    free(k->weights);
}

// ========== KERNEL DEFINITIONS ==========
// Windows and window functions for each (named) kernel

static inline double box(__attribute__((unused)) double x)
{
    return 1.0;
}

static inline double lin(double x)
{
    return 1.0 - x;
}

static inline double cub(double x)
{
    if (x <= 1.0) {
        return 3.0 / 2.0 * x * x * x - 5.0 / 2.0 * x * x + 1.0;
    } else {
        return -1.0 / 2.0 * x * x * x + 5.0 / 2.0 * x * x - 4.0 * x + 2.0;
    }
}

static inline double mks13(double x)
{
    if (x <= 0.5) {
        return 17.0 / 16.0 - 7.0 / 4.0 * x * x;
    } else if (x <= 1.5) {
        return x * x - 11.0 / 4.0 * x + 7.0 / 4.0;
    } else {
        return -1.0 / 8.0 * x * x + 5.0 / 8.0 * x - 25.0 / 32.0;
    }
}

static inline void new_named_kernel(enum pixmap_scale scaler, struct kernel* k,
                                    size_t in, size_t out, ssize_t offset,
                                    double scale)
{
    switch (scaler) {
        case pixmap_box:
            new_kernel(k, in, out, offset, scale, 0.5, box);
            break;
        case pixmap_bilinear:
            new_kernel(k, in, out, offset, scale, 1.0, lin);
            break;
        case pixmap_bicubic:
            new_kernel(k, in, out, offset, scale, 2.0, cub);
            break;
        case pixmap_mks13:
            new_kernel(k, in, out, offset, scale, 2.5, mks13);
            break;
        default:
            break;
    }
}

// ========== MULTITHREADING ==========

/** Values shared by all threads in a nearest-neighbor scale. */
struct nn_shared {
    const struct pixmap* src; ///< Source pixmap
    struct pixmap* dst;       ///< Destination pixmap
    size_t x_low;             ///< x start (left)
    size_t x_high;            ///< x end (right)
    size_t num;               ///< Numerator in fixed-point
    uint8_t den_bits;         ///< Amount to shift for denominator
    ssize_t x;                ///< x offset in destination
    ssize_t y;                ///< y offset in destination
    bool alpha;               ///< Use alpha blending?
};

/** Per-thread information for a nearest-neighbor scale. */
struct nn_priv {
    struct nn_shared* s; ///< Shared information
    size_t y_low;        ///< Start row
    size_t y_high;       ///< One beyond end row
    pthread_t id;        ///< Thread id
};

/** Values shared by all threads in all other scales. */
struct sc_shared {
    const struct pixmap* src; ///< Source pixmap
    struct pixmap in;         ///< Intermediate pixmap
    struct pixmap* dst;       ///< Destination pixmap
    struct kernel hk;         ///< Horizontal kernel
    struct kernel vk;         ///< Vertical kernel
    size_t yoff;              ///< y offset (for horizontal kernel)
    size_t xoff;              ///< x offset (for vertical kernel)
    bool alpha;               ///< Use alpha blending?
    size_t threads;           ///< Total number of threads
    size_t sync;              ///< Number of threads done with horizontal pass
    pthread_mutex_t m;        ///< Mutex to protect sync
    pthread_cond_t cv;        ///< CV for the same
};

/** Per-thread information for all other scales. */
struct sc_priv {
    struct sc_shared* s; ///< Shared information
    size_t hy_low;       ///< Start row for horizontal pass
    size_t hy_high;      ///< One beyond end row for horizontal pass
    size_t vy_low;       ///< Start row for vertical pass
    size_t vy_high;      ///< One beyond end row for vertical pass
    pthread_t id;        ///< Thread id
};

// ========== APPLICATION ==========

// Apply a horizontal kernel; the output pixmap is assumed to be only as tall as
// needed by the vertical pass - yoff indicates where it begins in the source
static inline void apply_hk(const struct pixmap* src, struct pixmap* dst,
                            const struct kernel* k, size_t y_low, size_t y_high,
                            size_t yoff, bool alpha)
{
    for (size_t y = y_low; y < y_high; ++y) {
        argb_t* dst_line = &dst->data[y * dst->width];
        for (size_t x = 0; x < dst->width; ++x) {
            const struct output* o = &k->o[x];
            int64_t a = 0;
            int64_t r = 0;
            int64_t g = 0;
            int64_t b = 0;
            for (size_t i = 0; i < o->n; ++i) {
                const argb_t c =
                    src->data[(y + yoff) * src->width + o->first + i];
                const int64_t wa =
                    (int64_t)ARGB_GET_A(c) * k->weights[o->index + i];
                a += wa;
                r += ARGB_GET_R(c) * wa;
                g += ARGB_GET_G(c) * wa;
                b += ARGB_GET_B(c) * wa;
            }

            // TODO the result would likely be more accurate (without
            // sacrificing speed) if we saved more than 8 bits between
            // the passes
            const uint8_t ua = clamp(a >> FIXED_BITS, 0, 255);
            if (a == 0) {
                a = (1 << FIXED_BITS);
            }
            // TODO irrespective of the above, saving the intermediate with
            // premultiplied alpha would almost certainly improve performance
            const uint8_t ur = clamp(r / a, 0, 255);
            const uint8_t ug = clamp(g / a, 0, 255);
            const uint8_t ub = clamp(b / a, 0, 255);
            const argb_t color = ARGB(ua, ur, ug, ub);
            if (alpha) {
                alpha_blend(color, &dst_line[x]);
            } else {
                dst_line[x] = color;
            }
        }
    }
}

// Apply a vertical kernel; the input pixmap is assumed to be only as tall as
// needed - xoff indicates where it should go in the destination
static inline void apply_vk(const struct pixmap* src, struct pixmap* dst,
                            const struct kernel* k, size_t y_low, size_t y_high,
                            size_t xoff, bool alpha)
{
    for (size_t y = y_low; y < y_high; ++y) {
        argb_t* dst_line = &dst->data[(y + k->start_out) * dst->width];
        for (size_t x = 0; x < src->width; ++x) {
            const struct output* o = &k->o[y];
            int64_t a = 0;
            int64_t r = 0;
            int64_t g = 0;
            int64_t b = 0;
            for (size_t i = 0; i < o->n; ++i) {
                const argb_t c =
                    src->data[(o->first + i - k->start_in) * src->width + x];
                const int64_t wa =
                    (int64_t)ARGB_GET_A(c) * k->weights[o->index + i];
                a += wa;
                r += ARGB_GET_R(c) * wa;
                g += ARGB_GET_G(c) * wa;
                b += ARGB_GET_B(c) * wa;
            }

            const uint8_t ua = clamp(a >> FIXED_BITS, 0, 255);
            if (a == 0) {
                a = (1 << FIXED_BITS);
            }
            const uint8_t ur = clamp(r / a, 0, 255);
            const uint8_t ug = clamp(g / a, 0, 255);
            const uint8_t ub = clamp(b / a, 0, 255);
            const argb_t color = ARGB(ua, ur, ug, ub);
            if (alpha) {
                alpha_blend(color, &dst_line[x + xoff]);
            } else {
                dst_line[x + xoff] = color;
            }
        }
    }
}

// ========== NEAREST NEIGHBOR ==========

// See pixmap_scale for more details (also uses fixed point arithmetic)
static inline void scale_nearest(const struct pixmap* src, struct pixmap* dst,
                                 size_t y_low, size_t y_high, size_t x_low,
                                 size_t x_high, size_t num, uint8_t den_bits,
                                 ssize_t x, ssize_t y, bool alpha)
{
    for (size_t dst_y = y_low; dst_y < y_high; ++dst_y) {
        const size_t src_y = ((dst_y - y) * num) >> den_bits;
        const argb_t* src_line = &src->data[src_y * src->width];
        argb_t* dst_line = &dst->data[dst_y * dst->width];

        for (size_t dst_x = x_low; dst_x < x_high; ++dst_x) {
            const size_t src_x = ((dst_x - x) * num) >> den_bits;
            const argb_t color = src_line[src_x];

            if (alpha) {
                alpha_blend(color, &dst_line[dst_x]);
            } else {
                dst_line[dst_x] = ARGB_SET_A(0xff) | color;
            }
        }
    }
}

// ========== THREAD TASKS ==========

static void* nn_task(void* arg)
{
    // Each thread simply handles a consecutive block of rows
    struct nn_priv* p = arg;
    scale_nearest(p->s->src, p->s->dst, p->y_low, p->y_high, p->s->x_low,
                  p->s->x_high, p->s->num, p->s->den_bits, p->s->x, p->s->y,
                  p->s->alpha);
    return NULL;
}

static void* sc_task(void* arg)
{
    // Each thread first handles a consecutive block of rows for the horizontal
    // scale, then they all wait until that work is finished and move on to
    // handling a consecutive block of rows for the vertical scale
    struct sc_priv* p = arg;
    apply_hk(p->s->src, &p->s->in, &p->s->hk, p->hy_low, p->hy_high, p->s->yoff,
             false);
    pthread_mutex_lock(&p->s->m);
    ++p->s->sync;
    pthread_cond_broadcast(&p->s->cv);
    while (p->s->sync != p->s->threads) {
        pthread_cond_wait(&p->s->cv, &p->s->m);
    }
    pthread_mutex_unlock(&p->s->m);
    apply_vk(&p->s->in, p->s->dst, &p->s->vk, p->vy_low, p->vy_high, p->s->xoff,
             p->s->alpha);
    return NULL;
}

void pixmap_scale(enum pixmap_scale scaler, const struct pixmap* src,
                  struct pixmap* dst, ssize_t x, ssize_t y, float scale,
                  bool alpha)
{
    // TODO in some cases (especially when scaling to small outputs), using
    // threads is actually slower - it may be worth implementing some better
    // heuristics here to avoid using as many (or any) threads in those cases.
    // This is especially an issue with the gallery, which produces a lot of
    // small images - it would probably be more efficient to spin up threads
    // which each handle some thumbnails (on their own), rather than using
    // multiple threads for each thumbnail

    // get active CPUs
#ifdef __FreeBSD__
    uint32_t cpus = 0;
    size_t cpus_len = sizeof(cpus);
    sysctlbyname("hw.ncpu", &cpus, &cpus_len, 0, 0);
#else
    const long cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    // but limit background threads to at most 15
    const size_t bthreads = clamp(cpus, 1, 16) - 1;

    if (scaler == pixmap_nearest) {
        // TODO perhaps look into 0.5 offsets in the fixed-point representation
        // to avoid "jitter" when switching from nearest to box and back again
        const size_t left = max(0, x);
        const size_t top = max(0, y);
        const size_t right = min(dst->width, (size_t)(x + scale * src->width));
        const size_t bottom =
            min(dst->height, (size_t)(y + scale * src->height));
        // Ensure that regardless of scale factor, we can just do one integer
        // multiplication and a shift, rather than a floating-point division.
        // The choices 32 (for upscale) and 5 (for downscale) ensure we can
        // represent a large range of scales with relatively good precision
        const uint8_t den_bits = scale > 1.0 ? 32 : 5;
        const size_t num = (1.0 / scale) * (1UL << den_bits);
        struct nn_shared s = {
            .src = src,
            .dst = dst,
            .x_low = left,
            .x_high = right,
            .num = num,
            .den_bits = den_bits,
            .x = x,
            .y = y,
            .alpha = alpha,
        };

        struct nn_priv* p = NULL;
        if (bthreads){
            p = malloc(bthreads * sizeof(*p));
        }
        const size_t len = (bottom - top) / (bthreads + 1);
        size_t row = top;
        for (size_t i = 0; i < bthreads; ++i) {
            p[i].s = &s;
            p[i].y_low = row;
            row += len;
            p[i].y_high = row;
            pthread_create(&p[i].id, NULL, nn_task, &p[i]);
        }
        struct nn_priv local = {
            .s = &s,
            .y_low = row,
            .y_high = bottom,
        };

        nn_task(&local);

        for (size_t i = 0; i < bthreads; ++i) {
            pthread_join(p[i].id, NULL);
        }

        free(p);
    } else {
        struct sc_shared s = {
            .src = src,
            .dst = dst,
            .alpha = alpha,
            .threads = bthreads + 1,
            .sync = 0,
            .m = PTHREAD_MUTEX_INITIALIZER,
            .cv = PTHREAD_COND_INITIALIZER,
        };
        new_named_kernel(scaler, &s.hk, src->width, dst->width, x, scale);
        new_named_kernel(scaler, &s.vk, src->height, dst->height, y, scale);
        pixmap_create(&s.in, s.hk.n_out, s.vk.n_in);
        s.yoff = s.vk.start_in;
        s.xoff = s.hk.start_out;

        struct sc_priv* p = NULL;
        if (bthreads) {
            p = malloc(bthreads * sizeof(*p));
        }
        const size_t hlen = s.vk.n_in / (bthreads + 1);
        const size_t vlen = s.vk.n_out / (bthreads + 1);
        size_t hrow = 0;
        size_t vrow = 0;
        for (size_t i = 0; i < bthreads; ++i) {
            p[i].s = &s;
            p[i].hy_low = hrow;
            p[i].vy_low = vrow;
            hrow += hlen;
            vrow += vlen;
            p[i].hy_high = hrow;
            p[i].vy_high = vrow;
            pthread_create(&p[i].id, NULL, sc_task, &p[i]);
        }
        struct sc_priv local = {
            .s = &s,
            .hy_low = hrow,
            .vy_low = vrow,
            .hy_high = s.vk.n_in,
            .vy_high = s.vk.n_out,
        };

        sc_task(&local);

        for (size_t i = 0; i < bthreads; ++i) {
            pthread_join(p[i].id, NULL);
        }

        free(p);
        free_kernel(&s.hk);
        free_kernel(&s.vk);
        pixmap_free(&s.in);
    }
}

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

// Except for nearest-neighbor, scaling is done via 1D convolution kernels, in
// which each output is the weighted sum of a set of inputs. Weights are
// stored contiguously in fixed point to limit memory consumption and improve
// performance when applying. Outside of nearest-neighbor, scales are
// implemented using a horizontal then vertical pass of a 1D kernel. Each
// kernel is defined mathematically by a window (beyond which it's zero) and a
// symmetric window function defining its weight within that window.

// 14-bit fixed point means we still comfortably fit within a 16-bit signed
// integer, including those weights which are slightly negative or a little
// over 1
#define FIXED_BITS 14

/** The description of a single output in a kernel. */
struct output {
    size_t first; ///< First input for this output
    size_t n;     ///< Number of inputs for this output
    size_t index; ///< Index of first weight in weights array
};

/** A 1D convolution kernel. */
struct kernel {
    size_t start_out;       ///< First output
    size_t n_out;           ///< Number of outputs
    size_t start_in;        ///< First input
    size_t n_in;            ///< Number of inputs
    struct output* outputs; ///< Outputs
    int16_t* weights;       ///< Weights
};

/** Window function type. */
typedef double (*window_fn)(double);

/** Input bounds for a given output. */
struct bounds {
    ssize_t first;
    ssize_t last;
};

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

// Get the first and last input for a given output
static inline struct bounds get_bounds(size_t out, double scale, double window)
{
    // Adjust by 0.5 to ensure sampling from the centers of pixels,
    // not their edges
    const double c = (out + 0.5) / scale - 0.5;
    const double d = window / fmin(scale, 1.0);
    const ssize_t f = floor(c - d);
    const ssize_t l = ceil(c + d);
    const struct bounds b = { f, l };
    return b;
}

// Get the weight for a given input/output pair
static inline double get_weight(size_t in, size_t out, double scale,
                                double window, window_fn f)
{
    if (scale >= 1.0) {
        const double c = (out + 0.5) / scale - 0.5;
        const double x = fabs(in - c);
        return x > window ? 0.0 : f(x);
    } else {
        const double c = (in + 0.5) * scale - 0.5;
        const double x = fabs(out - c);
        return x > window ? 0.0 : f(x);
    }
}

// Build a new fixed point kernel from its mathematical description
static void new_kernel(struct kernel* k, size_t nin, size_t nout,
                       ssize_t offset, double scale, double window, window_fn f)
{
    // Store weights locally first, for normalization and zero detection
    double* weights;
    int16_t* int_weights;

    // Output bounds
    const size_t start = max(0, offset);
    const size_t end = min(nout, (size_t)(offset + nin * scale));
    k->start_out = start;
    k->n_out = end - start;

    // Estimate space needed for weights
    const struct bounds b = get_bounds(0, scale, window);
    // Due to floor and ceil, we need at least 2 extra to be safe, so 3
    // certainly suffices
    const size_t n_per = b.last - b.first + 3;
    // The estimation overallocates, but kernels are only live for a short time
    weights = malloc(n_per * sizeof(*weights));
    int_weights = malloc(n_per * sizeof(*int_weights));
    k->weights = malloc(n_per * k->n_out * sizeof(*k->weights));
    k->outputs = malloc(k->n_out * sizeof(*k->outputs));

    // Track min and max input across all outputs
    size_t min_in = SIZE_MAX;
    size_t max_in = 0;
    size_t index = 0;
    for (size_t out = start; out < end; ++out) {
        double sum, norm;
        size_t tfirst, tlast;

        // Input bounds for this output
        struct output* o = &k->outputs[out - start];
        const struct bounds b = get_bounds(out - offset, scale, window);
        const size_t first = max(0, b.first);
        const size_t last = min(nin - 1, (size_t)b.last);

        sum = 0;
        for (size_t in = first; in <= last; ++in) {
            double w = get_weight(in, out - offset, scale, window, f);
            weights[in - first] = w;
            sum += w;
        }
        norm = 1.0 / sum;
        for (size_t in = first; in <= last; ++in) {
            // TODO if more accuracy is needed, round may help
            int_weights[in - first] =
                weights[in - first] * norm * (1 << FIXED_BITS);
        }

        // Ignore leading or trailing zeros
        for (tfirst = first; tfirst < last && int_weights[tfirst - first] == 0;
             ++tfirst)
            ;
        for (tlast = last; tlast > tfirst && int_weights[tlast - first] == 0;
             --tlast)
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
        memcpy(&k->weights[index], &int_weights[tfirst - first],
               o->n * sizeof(*k->weights));
        index += o->n;
    }

    k->start_in = min_in;
    k->n_in = max_in - min_in + 1;

    free(weights);
    free(int_weights);
}

static inline void free_kernel(struct kernel* k)
{
    free(k->outputs);
    free(k->weights);
}

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

static void new_named_kernel(enum pixmap_scale scaler, struct kernel* k,
                             size_t in, size_t out, ssize_t offset,
                             double scale)
{
    switch (scaler) {
        case pixmap_nearest:
            // We shouldn't ever get here
            break;
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
    }
}

// Apply a horizontal kernel; the output pixmap is assumed to be only as tall as
// needed by the vertical pass - yoff indicates where it begins in the source
static void apply_hk(const struct pixmap* src, struct pixmap* dst,
                     const struct kernel* k, size_t y_low, size_t y_high,
                     size_t yoff, bool alpha)
{
    for (size_t y = y_low; y < y_high; ++y) {
        argb_t* dst_line = &dst->data[y * dst->width];
        for (size_t x = 0; x < dst->width; ++x) {
            const struct output* o = &k->outputs[x];
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
static void apply_vk(const struct pixmap* src, struct pixmap* dst,
                     const struct kernel* k, size_t y_low, size_t y_high,
                     size_t xoff, bool alpha)
{
    for (size_t y = y_low; y < y_high; ++y) {
        argb_t* dst_line = &dst->data[(y + k->start_out) * dst->width];
        for (size_t x = 0; x < src->width; ++x) {
            const struct output* o = &k->outputs[y];
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

static void* nn_task(void* arg)
{
    // Each thread simply handles a consecutive block of rows
    struct nn_priv* p = arg;
    struct nn_shared* s = p->s;
    scale_nearest(s->src, s->dst, p->y_low, p->y_high, s->x_low, s->x_high,
                  s->num, s->den_bits, s->x, s->y, s->alpha);
    return NULL;
}

static void* sc_task(void* arg)
{
    // Each thread first handles a consecutive block of rows for the horizontal
    // scale, synchronizes with the others, then handles a consecutive block of
    // rows for the vertical scale
    struct sc_priv* p = arg;
    struct sc_shared* s = p->s;
    apply_hk(s->src, &s->in, &s->hk, p->hy_low, p->hy_high, s->yoff, false);
    pthread_mutex_lock(&s->m);
    ++s->sync;
    pthread_cond_broadcast(&s->cv);
    while (s->sync != s->threads) {
        pthread_cond_wait(&s->cv, &s->m);
    }
    pthread_mutex_unlock(&s->m);
    apply_vk(&s->in, s->dst, &s->vk, p->vy_low, p->vy_high, s->xoff, s->alpha);
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
        const size_t left = max(0, x);
        const size_t top = max(0, y);
        const size_t right = min(dst->width, (size_t)(x + scale * src->width));
        const size_t bottom =
            min(dst->height, (size_t)(y + scale * src->height));
        // Use fixed-point for efficiency (floating-point division becomes an
        // addition and a shift, since it's used in a loop anyway). The choices
        // (32 and 25) ensure we have as much precision as floats, but still
        // support large downscales of large images (the largest supported image
        // at minimum scale would need 2^48 bytes of memory)
        const uint8_t den_bits = scale > 1.0 ? 32 : 25;
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
        if (bthreads) {
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

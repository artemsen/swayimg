// SPDX-License-Identifier: MIT
// Multithreaded software renderer for raster images.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "render.hpp"

#include "defaults.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

/** Minimal number of pixel per thread. */
constexpr size_t MIN_PIXELS_PER_THREAD = 300UL * 300UL;

namespace {

namespace NN { // nearest-neighbor
    /**
     * Put part of one pixmap on another.
     * @param src_pm source pixmap (overlay)
     * @param src_pt top left point of source pixmap
     * @param src_scale scale factor of source pixmap
     * @param dst_pm destination pixmap (underlay)
     * @param dst_rect destination area to fill
     */
    void mix_pm(const Pixmap* src_pm, const Point src_pt,
                const double src_scale, Pixmap* dst_pm,
                const Rectangle dst_rect)
    {
        // Replace per-pixel division with a single reciprocal multiply
        const double inv_scale = 1.0 / src_scale;
        const size_t src_pos_x = static_cast<size_t>(src_pt.x);
        const size_t src_pos_y = static_cast<size_t>(src_pt.y);

        if (src_pm->format() == Pixmap::ARGB) {
            for (size_t y = 0; y < dst_rect.height; ++y) {
                const size_t src_y =
                    static_cast<size_t>((src_pos_y + y) * inv_scale);
                const size_t dst_y = dst_rect.y + y;
                for (size_t x = 0; x < dst_rect.width; ++x) {
                    const size_t src_x =
                        static_cast<size_t>((src_pos_x + x) * inv_scale);
                    dst_pm->at(dst_rect.x + x, dst_y)
                        .blend(src_pm->at(src_x, src_y));
                }
            }
        } else {
            for (size_t y = 0; y < dst_rect.height; ++y) {
                const size_t src_y =
                    static_cast<size_t>((src_pos_y + y) * inv_scale);
                const size_t dst_y = dst_rect.y + y;
                for (size_t x = 0; x < dst_rect.width; ++x) {
                    const size_t src_x =
                        static_cast<size_t>((src_pos_x + x) * inv_scale);
                    dst_pm->at(dst_rect.x + x, dst_y) =
                        src_pm->at(src_x, src_y);
                }
            }
        }
    }

    /**
     * Put one pixmap on another.
     * @param dst destination pixmap (underlay)
     * @param src source pixmap (overlay)
     * @param pos top left position of source pixmap
     * @param scale scale factor of source pixmap
     * @param tpool thread pool
     */
    void draw(Pixmap& dst, const Pixmap& src, const Point& pos,
              const double scale, ThreadPool& tpool)
    {
        const Rectangle image(pos, static_cast<Size>(src) * scale);
        const Rectangle visible =
            image.intersect(Rectangle({ .x = 0, .y = 0 }, dst));
        if (!visible) {
            return; // out of pixmap
        }

        // callulate number of used threads
        const size_t total_pixels = visible.width * visible.height;
        const size_t threads = std::clamp(total_pixels / MIN_PIXELS_PER_THREAD,
                                          static_cast<size_t>(1), tpool.size());

        const Point src_start {
            .x = visible.x - image.x,
            .y = visible.y - image.y,
        };

        if (threads == 1) {
            // single-thread fast path: avoid the thread pool round-trip
            mix_pm(&src, src_start, scale, &dst, visible);
            return;
        }

        const size_t step = visible.height / threads;

        std::vector<size_t> tids;
        tids.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            const size_t src_offset = step * i;

            Point src_pos = src_start;
            src_pos.y += src_offset;

            Rectangle dst_rect = visible;
            dst_rect.y += src_offset;
            dst_rect.height = step;

            if (i == threads - 1) {
                dst_rect.height += visible.height - step * threads;
            }

            const size_t tid =
                tpool.add(&mix_pm, &src, src_pos, scale, &dst, dst_rect);
            tids.push_back(tid);
        }

        tpool.wait(tids);
    }

} // namespace NN

namespace AA { // anti-aliasing

    // 14-bit fixed point means we still comfortably fit within a 16-bit signed
    // integer, including those weights which are slightly negative or a little
    // over 1
    constexpr size_t FIXED_BITS = 14;

    constexpr double WINDOW_SIZE = 2.5;

    /** The description of a single output in a kernel. */
    struct Output {
        size_t first; ///< First input for this output
        size_t n;     ///< Number of inputs for this output
        size_t index; ///< Index of first weight in weights array
    };

    /** A 1D convolution kernel. */
    struct Kernel {
        size_t start_out;             ///< First output
        size_t n_out;                 ///< Number of outputs
        size_t start_in;              ///< First input
        size_t n_in;                  ///< Number of inputs
        std::vector<Output> outputs;  ///< Outputs
        std::vector<int16_t> weights; ///< Weights
    };

    // Get the first and last input for a given output
    inline std::pair<ssize_t, ssize_t> get_bounds(size_t out, double scale)
    {
        // Adjust by 0.5 to ensure sampling from the centers of pixels,
        // not their edges
        const double c = (out + 0.5) / scale - 0.5;
        const double d = WINDOW_SIZE / std::fmin(scale, 1.0);
        return std::make_pair(static_cast<ssize_t>(std::floor(c - d)),
                              static_cast<ssize_t>(std::ceil(c + d)));
    }

    // Magic Kernel Sharp 2013
    inline double mks13(double x)
    {
        if (x <= 0.5) {
            return 17.0 / 16.0 - 7.0 / 4.0 * x * x;
        }
        if (x <= 1.5) {
            return x * x - 11.0 / 4.0 * x + 7.0 / 4.0;
        }
        return -1.0 / 8.0 * x * x + 5.0 / 8.0 * x - 25.0 / 32.0;
    }

    // Get the weight for a given input/output pair
    double get_weight(size_t in, size_t out, double scale)
    {
        double c;
        double x;

        if (scale >= 1.0) {
            c = (out + 0.5) / scale - 0.5;
            x = std::fabs(in - c);
        } else {
            c = (in + 0.5) * scale - 0.5;
            x = std::fabs(out - c);
        }

        return x > WINDOW_SIZE ? 0.0 : mks13(x);
    }

    // Build a new fixed point kernel from its mathematical description
    void init_mks2013_kernel(Kernel& kernel, size_t nin, size_t nout,
                             ssize_t offset, double scale)
    {
        // Output bounds
        const size_t start =
            static_cast<size_t>(std::max(static_cast<ssize_t>(0), offset));
        const size_t end = static_cast<size_t>(
            std::min(static_cast<ssize_t>(nout),
                     static_cast<ssize_t>(offset + nin * scale)));
        kernel.start_out = start;
        kernel.n_out = end - start;

        // Estimate space needed for weights
        const std::pair<ssize_t, ssize_t> estimate = get_bounds(0, scale);

        // Due to floor and ceil, we need at least 2 extra to be safe, so 3
        // certainly suffices
        const size_t n_per = estimate.second - estimate.first + 3;

        // The estimation overallocates, but kernels are only live for a short
        // time
        std::vector<double> weights(n_per);
        std::vector<int16_t> int_weights(n_per);
        kernel.weights.resize(n_per * kernel.n_out);
        kernel.outputs.resize(kernel.n_out);

        // Track min and max input across all outputs
        size_t min_in = std::numeric_limits<size_t>::max();
        size_t max_in = 0;
        size_t index = 0;
        for (size_t out = start; out < end; ++out) {
            // Input bounds for this output
            const std::pair<ssize_t, ssize_t> bounds =
                get_bounds(out - offset, scale);

            const size_t first = static_cast<size_t>(
                std::max(static_cast<ssize_t>(0), bounds.first));
            const size_t last = static_cast<size_t>(
                std::min(static_cast<ssize_t>(nin - 1), bounds.second));

            double sum = 0;
            for (size_t in = first; in <= last; ++in) {
                const double w = get_weight(in, out - offset, scale);
                weights[in - first] = w;
                sum += w;
            }
            const double norm = 1.0 / sum;
            int16_t isum = 0;
            for (size_t in = first; in <= last; ++in) {
                const int16_t iw =
                    std::round(weights[in - first] * norm * (1 << FIXED_BITS));
                int_weights[in - first] = iw;
                isum += iw;
            }
            int_weights[(last - first) / 2] += (1 << FIXED_BITS) - isum;

            // Ignore leading or trailing zeros
            size_t tfirst;
            size_t tlast;
            for (tfirst = first;
                 tfirst < last && int_weights[tfirst - first] == 0; ++tfirst) {}
            for (tlast = last;
                 tlast > tfirst && int_weights[tlast - first] == 0; --tlast) {}
            min_in = std::min(tfirst, min_in);
            max_in = std::max(tlast, max_in);

            Output& output = kernel.outputs[out - start];
            output.n = tlast - tfirst + 1;
            output.first = tfirst;
            output.index = index;
            std::memcpy(&kernel.weights[index], &int_weights[tfirst - first],
                        output.n * sizeof(int16_t));
            index += output.n;
        }

        kernel.start_in = min_in;
        kernel.n_in = max_in - min_in + 1;
    }

    // Apply a horizontal kernel; the output pixmap is assumed to be only as
    // tall as needed by the vertical pass - yoff indicates where it begins in
    // the source
    void apply_hk(const Pixmap* src, Pixmap* dst, const Kernel* kernel,
                  const size_t y_low, const size_t y_high, const size_t yoff)
    {
        const size_t width = dst->width();
        const size_t rows = y_high - y_low;
        const argb_t* srow0 =
            reinterpret_cast<const argb_t*>(src->ptr(0, y_low + yoff));
        argb_t* drow0 = reinterpret_cast<argb_t*>(dst->ptr(0, y_low));
        const size_t sstride = src->stride() / sizeof(argb_t);
        const size_t dstride = dst->stride() / sizeof(argb_t);

        if (src->format() == Pixmap::ARGB) {
            for (size_t y = 0; y < rows; ++y) {
                const argb_t* srow = srow0 + static_cast<ssize_t>(y) * sstride;
                argb_t* drow = drow0 + static_cast<ssize_t>(y) * dstride;
                for (size_t x = 0; x < width; ++x) {
                    const Output& output = kernel->outputs[x];
                    const argb_t* cp = srow + output.first;
                    const int16_t* wp = &kernel->weights[output.index];
                    int64_t a = 0;
                    int64_t r = 0;
                    int64_t g = 0;
                    int64_t b = 0;
                    for (size_t i = 0; i < output.n; ++i, ++cp, ++wp) {
                        const int64_t wa = static_cast<int64_t>(*wp) * cp->a;
                        a += wa;
                        r += cp->r * wa;
                        g += cp->g * wa;
                        b += cp->b * wa;
                    }
                    const uint8_t ua =
                        std::clamp(a >> FIXED_BITS, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    if (a == 0) {
                        a = (1 << FIXED_BITS);
                    }
                    const uint8_t ur =
                        std::clamp(r / a, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    const uint8_t ug =
                        std::clamp(g / a, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    const uint8_t ub =
                        std::clamp(b / a, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    drow[x].blend(argb_t(ua, ur, ug, ub));
                }
            }
        } else {
            for (size_t y = 0; y < rows; ++y) {
                const argb_t* srow = srow0 + static_cast<ssize_t>(y) * sstride;
                argb_t* drow = drow0 + static_cast<ssize_t>(y) * dstride;
                for (size_t x = 0; x < width; ++x) {
                    const Output& output = kernel->outputs[x];
                    const argb_t* cp = srow + output.first;
                    const int16_t* wp = &kernel->weights[output.index];
                    int64_t r = 0;
                    int64_t g = 0;
                    int64_t b = 0;
                    for (size_t i = 0; i < output.n; ++i, ++cp, ++wp) {
                        const int64_t w = static_cast<int64_t>(*wp);
                        r += cp->r * w;
                        g += cp->g * w;
                        b += cp->b * w;
                    }
                    const uint8_t ur =
                        std::clamp(r >> FIXED_BITS, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    const uint8_t ug =
                        std::clamp(g >> FIXED_BITS, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    const uint8_t ub =
                        std::clamp(b >> FIXED_BITS, static_cast<int64_t>(0),
                                   static_cast<int64_t>(255));
                    drow[x] = argb_t(0xff, ur, ug, ub);
                }
            }
        }
    }

    /** Zero the scratch accumulators for one output row. */
    inline void reset_accum(int64_t* sum, const size_t count)
    {
        for (size_t x = 0; x < count; ++x) {
            sum[x] = 0;
        }
    }

    /** Accumulate one contributing input row into the scratch buffers. */
    inline void accum_row(const argb_t* cp, const int64_t w, const size_t width,
                          int64_t* sum_a, int64_t* sum_r, int64_t* sum_g,
                          int64_t* sum_b)
    {
        for (size_t x = 0; x < width; ++x, ++cp) {
            const int64_t wa = w * cp->a;
            sum_a[x] += wa;
            sum_r[x] += cp->r * wa;
            sum_g[x] += cp->g * wa;
            sum_b[x] += cp->b * wa;
        }
    }

    /** Accumulate one contributing input row (premultiplied alpha absent). */
    inline void accum_row_rgb(const argb_t* cp, const int64_t w,
                              const size_t width, int64_t* sum_r,
                              int64_t* sum_g, int64_t* sum_b)
    {
        for (size_t x = 0; x < width; ++x, ++cp) {
            sum_r[x] += cp->r * w;
            sum_g[x] += cp->g * w;
            sum_b[x] += cp->b * w;
        }
    }

    /** Un-premultiply and blend the accumulated row into the destination. */
    inline void finalize_accum(argb_t* drow, const int64_t* sum_a,
                               const int64_t* sum_r, const int64_t* sum_g,
                               const int64_t* sum_b, const size_t width)
    {
        for (size_t x = 0; x < width; ++x) {
            int64_t a = sum_a[x];
            const uint8_t ua =
                std::clamp(a >> FIXED_BITS, static_cast<int64_t>(0),
                           static_cast<int64_t>(255));
            if (a == 0) {
                a = (1 << FIXED_BITS);
            }
            const uint8_t ur = std::clamp(sum_r[x] / a, static_cast<int64_t>(0),
                                          static_cast<int64_t>(255));
            const uint8_t ug = std::clamp(sum_g[x] / a, static_cast<int64_t>(0),
                                          static_cast<int64_t>(255));
            const uint8_t ub = std::clamp(sum_b[x] / a, static_cast<int64_t>(0),
                                          static_cast<int64_t>(255));
            drow[x].blend(argb_t(ua, ur, ug, ub));
        }
    }

    /** Convert the accumulated row to the destination (opaque output). */
    inline void finalize_accum_rgb(argb_t* drow, const int64_t* sum_r,
                                   const int64_t* sum_g, const int64_t* sum_b,
                                   const size_t width)
    {
        for (size_t x = 0; x < width; ++x) {
            const uint8_t ur =
                std::clamp(sum_r[x] >> FIXED_BITS, static_cast<int64_t>(0),
                           static_cast<int64_t>(255));
            const uint8_t ug =
                std::clamp(sum_g[x] >> FIXED_BITS, static_cast<int64_t>(0),
                           static_cast<int64_t>(255));
            const uint8_t ub =
                std::clamp(sum_b[x] >> FIXED_BITS, static_cast<int64_t>(0),
                           static_cast<int64_t>(255));
            drow[x] = argb_t(0xff, ur, ug, ub);
        }
    }

    /** Apply a vertical kernel; the input pixmap is assumed to be only as tall
        as needed - xoff indicates where it should go in the destination. */
    void apply_vk(const Pixmap* src, Pixmap* dst, const struct Kernel* kernel,
                  const size_t y_low, const size_t y_high, const size_t xoff)
    {
        const size_t width = src->width();
        const argb_t* srow0 = reinterpret_cast<const argb_t*>(src->ptr(0, 0));
        argb_t* drow0 = reinterpret_cast<argb_t*>(dst->ptr(0, 0));
        const ssize_t sstride =
            static_cast<ssize_t>(src->stride() / sizeof(argb_t));
        const ssize_t dstride =
            static_cast<ssize_t>(dst->stride() / sizeof(argb_t));
        const bool alpha = src->format() == Pixmap::ARGB;

        // row-major scratch accumulators, one per output column
        std::vector<int64_t> sum_a(width);
        std::vector<int64_t> sum_r(width);
        std::vector<int64_t> sum_g(width);
        std::vector<int64_t> sum_b(width);

        // to keep the inner loop over source columns contiguous (cache
        // friendly), iterate over contributing input rows rather than jumping
        // between strided rows for every output pixel
        for (size_t y = y_low; y < y_high; ++y) {
            const Output& output = kernel->outputs[y];
            const ssize_t first_row = static_cast<ssize_t>(output.first) -
                static_cast<ssize_t>(kernel->start_in);
            const int16_t* wp = &kernel->weights[output.index];
            const argb_t* srow = srow0 + first_row * sstride;
            argb_t* drow = drow0 +
                (static_cast<ssize_t>(y) +
                 static_cast<ssize_t>(kernel->start_out)) *
                    dstride +
                static_cast<ssize_t>(xoff);
            if (alpha) {
                reset_accum(sum_a.data(), width);
                reset_accum(sum_r.data(), width);
                reset_accum(sum_g.data(), width);
                reset_accum(sum_b.data(), width);
                for (size_t i = 0; i < output.n; ++i, ++wp, srow += sstride) {
                    accum_row(srow, static_cast<int64_t>(*wp), width,
                              sum_a.data(), sum_r.data(), sum_g.data(),
                              sum_b.data());
                }
                finalize_accum(drow, sum_a.data(), sum_r.data(), sum_g.data(),
                               sum_b.data(), width);
            } else {
                reset_accum(sum_r.data(), width);
                reset_accum(sum_g.data(), width);
                reset_accum(sum_b.data(), width);
                for (size_t i = 0; i < output.n; ++i, ++wp, srow += sstride) {
                    accum_row_rgb(srow, static_cast<int64_t>(*wp), width,
                                  sum_r.data(), sum_g.data(), sum_b.data());
                }
                finalize_accum_rgb(drow, sum_r.data(), sum_g.data(),
                                   sum_b.data(), width);
            }
        }
    }

    /**
     * Put one pixmap on another using anti-aliasing.
     * @param dst destination pixmap (underlay)
     * @param src source pixmap (overlay)
     * @param pos top left position of source pixmap
     * @param scale scale factor of source pixmap
     * @param tpool thread pool
     */
    void draw(Pixmap& dst, const Pixmap& src, const Point& pos,
              const double scale, ThreadPool& tpool)
    {
        const Rectangle image(pos, static_cast<Size>(src) * scale);
        const Rectangle visible =
            image.intersect(Rectangle({ .x = 0, .y = 0 }, dst));
        if (!visible) {
            return; // out of pixmap
        }

        // initialize mks2013 kernels
        Kernel kernel_hor;
        Kernel kernel_ver;
        init_mks2013_kernel(kernel_hor, src.width(), dst.width(), pos.x, scale);
        init_mks2013_kernel(kernel_ver, src.height(), dst.height(), pos.y,
                            scale);

        // intermediate pixmap
        Pixmap tmp;
        tmp.create(src.format(), kernel_hor.n_out, kernel_ver.n_in);

        // callulate number of used threads
        const size_t total_pixels = visible.width * visible.height;
        const size_t threads = std::clamp(total_pixels / MIN_PIXELS_PER_THREAD,
                                          static_cast<size_t>(1), tpool.size());

        if (threads == 1) {
            // single-thread fast path: avoid the thread pool round-trip
            apply_hk(&src, &tmp, &kernel_hor, 0, kernel_ver.n_in,
                     kernel_ver.start_in);
            apply_vk(&tmp, &dst, &kernel_ver, 0, kernel_ver.n_out,
                     kernel_hor.start_out);
            return;
        }

        // per thread ranges to render
        const size_t hlen = kernel_ver.n_in / threads;
        const size_t vlen = kernel_ver.n_out / threads;

        std::vector<size_t> tids;
        tids.reserve(threads);

        // apply horizontal kernel and write result to temporary pixmap
        for (size_t i = 0; i < threads; ++i) {
            const bool last = i == threads - 1;
            const size_t from = i * hlen;
            const size_t to = last ? kernel_ver.n_in : from + hlen;
            const size_t tid = tpool.add(&apply_hk, &src, &tmp, &kernel_hor,
                                         from, to, kernel_ver.start_in);
            tids.push_back(tid);
        }
        tpool.wait(tids);

        // apply vertical kernel
        tids.clear();
        for (size_t i = 0; i < threads; ++i) {
            const bool last = i == threads - 1;
            const size_t from = i * vlen;
            const size_t to = last ? kernel_ver.n_out : from + vlen;
            const size_t tid = tpool.add(&apply_vk, &tmp, &dst, &kernel_ver,
                                         from, to, kernel_hor.start_out);
            tids.push_back(tid);
        }
        tpool.wait(tids);
    }
} // namespace AA

namespace Blur {
    // Constant parameters
    constexpr size_t BLUR_SIZE = 3;
    constexpr size_t BLUR_SIGMA = 16;

    /** Color accumulator (integer channels). */
    struct ColorAccum {

        /**
         * Constructor.
         * @param color RGB color to set
         * @param factor color factor
         */
        ColorAccum(const argb_t& color, const size_t factor)
            : r(static_cast<int64_t>(factor) * color.r)
            , g(static_cast<int64_t>(factor) * color.g)
            , b(static_cast<int64_t>(factor) * color.b)
        {
        }

        /**
         * Add color to accumulator.
         * @param color RGB color to add
         * @return self reference
         */
        ColorAccum& operator+=(const argb_t& color)
        {
            r += color.r;
            g += color.g;
            b += color.b;
            return *this;
        }

        /**
         * Subtract color from accumulator.
         * @param color RGB color to subtract
         * @return self reference
         */
        ColorAccum& operator-=(const argb_t& color)
        {
            r -= color.r;
            g -= color.g;
            b -= color.b;
            return *this;
        }

        /**
         * Create ARGB color from accumulator.
         * @param divisor divisor of the components
         * @return ARGB color
         */
        [[nodiscard]] argb_t argb(const int64_t divisor) const
        {
            const int64_t div = divisor != 0 ? divisor : 1;
            const argb_t::channel cr =
                std::clamp(r / div, static_cast<int64_t>(argb_t::min),
                           static_cast<int64_t>(argb_t::max));
            const argb_t::channel cg =
                std::clamp(g / div, static_cast<int64_t>(argb_t::min),
                           static_cast<int64_t>(argb_t::max));
            const argb_t::channel cb =
                std::clamp(b / div, static_cast<int64_t>(argb_t::min),
                           static_cast<int64_t>(argb_t::max));
            return { argb_t::max, cr, cg, cb };
        }

        int64_t r, g, b;
    };

    /**
     * Blur pixmap horizontally.
     * @param pm target pixmap
     * @param radius blur radius
     */
    void apply_hor(Pixmap& pm, const size_t radius)
    {
        const size_t width = pm.width();
        const size_t radius_plus = radius + 1;
        const int64_t divisor = static_cast<int64_t>(radius + radius_plus);

        for (size_t y = 0; y < pm.height(); ++y) {
            const argb_t px_first = pm.at(0, y);
            const argb_t px_last = pm.at(width - 1, y);

            ColorAccum cacc(px_first, radius_plus);
            for (size_t x = 0; x < radius && x < width; ++x) {
                cacc += pm.at(x, y);
            }

            for (size_t x = 0; x <= radius && x + radius < width; ++x) {
                cacc += pm.at(x + radius, y);
                cacc -= px_first;
                pm.at(x, y) = cacc.argb(divisor);
            }

            for (size_t x = radius_plus; x + radius < width; ++x) {
                cacc += pm.at(x + radius, y);
                cacc -= pm.at(x - radius_plus, y);
                pm.at(x, y) = cacc.argb(divisor);
            }

            for (size_t x = width - radius; x < width && x >= radius_plus;
                 ++x) {
                cacc += px_last;
                cacc -= pm.at(x - radius_plus, y);
                pm.at(x, y) = cacc.argb(divisor);
            }
        }
    }

    /**
     * Blur pixmap vertically.
     * @param pm target pixmap
     * @param radius blur radius
     */
    void apply_ver(Pixmap& pm, const size_t radius)
    {
        const size_t height = pm.height();
        const size_t radius_plus = radius + 1;
        const int64_t divisor = static_cast<int64_t>(radius + radius_plus);

        for (size_t x = 0; x < pm.width(); ++x) {
            const argb_t px_first = pm.at(x, 0);
            const argb_t px_last = pm.at(x, height - 1);

            ColorAccum cacc(px_first, radius_plus);
            for (size_t y = 0; y < radius && y < height; ++y) {
                cacc += pm.at(x, y);
            }

            for (size_t y = 0; y <= radius && y + radius < height; ++y) {
                cacc += pm.at(x, y + radius);
                cacc -= px_first;
                pm.at(x, y) = cacc.argb(divisor);
            }

            for (size_t y = radius_plus; y + radius < height; ++y) {
                cacc += pm.at(x, y + radius);
                cacc -= pm.at(x, y - radius_plus);
                pm.at(x, y) = cacc.argb(divisor);
            }

            for (size_t y = height - radius; y < height && y >= radius_plus;
                 ++y) {
                cacc += px_last;
                cacc -= pm.at(x, y - radius_plus);
                pm.at(x, y) = cacc.argb(divisor);
            }
        }
    }

    /**
     * Apply Gaussian blur to pixmap slice.
     * @param pm target pixmap
     * @param exclude excluded area to preserve
     * @param tpool thread pool
     */
    void apply(Pixmap& pm, const Rectangle& exclude, ThreadPool& tpool)
    {
        const Rectangle full { 0, 0, pm.width(), pm.height() };
        const auto [top, bottom, left, right] = full.cutout(exclude);

        const double sigma12 = 12 * BLUR_SIGMA * BLUR_SIGMA;
        size_t weight_min;
        size_t weight_max;
        size_t weight_tran;
        static size_t blur_box[BLUR_SIZE] = { 0 };

        if (!blur_box[0]) {
            // create Gaussian blur box
            weight_min = std::sqrt(sigma12 / BLUR_SIZE + 1);
            if (weight_min % 2 == 0) {
                --weight_min;
            }
            weight_max = weight_min + 2;
            weight_tran = (sigma12 - BLUR_SIZE * weight_min * weight_min -
                           4.0 * BLUR_SIZE * weight_min - 3.0 * BLUR_SIZE) /
                (-4.0 * weight_min - 4.0);
            for (size_t i = 0; i < BLUR_SIZE; ++i) {
                blur_box[i] = i < weight_tran ? weight_min : weight_max;
            }
        }

        // multi-pass blur filter
        const auto blur_fn = [](Pixmap& pm) {
            for (const size_t i : blur_box) {
                const size_t radius = (i - 1) / 2;
                apply_hor(pm, radius);
                apply_ver(pm, radius);
            }
        };

        std::vector<size_t> tids;
        tids.reserve(4); // one per each side

        // blur each pixmap block
        if (top) {
            tids.push_back(tpool.add(blur_fn, pm.submap(top)));
        }
        if (bottom) {
            tids.push_back(tpool.add(blur_fn, pm.submap(bottom)));
        }
        if (left) {
            tids.push_back(tpool.add(blur_fn, pm.submap(left)));
        }
        if (right) {
            tids.push_back(tpool.add(blur_fn, pm.submap(right)));
        }

        tpool.wait(tids);
    }

} // namespace Blur

namespace Mirror { // mirroring
    /**
     * Mirror and blur top area of pixmap.
     * @param pm target pixmap
     * @param fill area to fill
     * @param exclude area to exclude
     * @param image origin image
     */
    void fill_top(Pixmap& pm, const Rectangle& fill, const Rectangle& exclude,
                  const Pixmap& image)
    {
        Pixmap mirror = pm.submap(fill);
        const size_t img_h = image.height();
        const size_t img_w = image.width();
        const size_t off_y = img_h - (exclude.y % img_h);

        // precompute horizontal source indexes
        const size_t off_x = img_w - (exclude.x % img_w);
        const size_t ex_par_x = (exclude.x / img_w) % 2;
        std::vector<size_t> src_x(fill.width);
        for (size_t x = 0; x < fill.width; ++x) {
            size_t img_x = (x + off_x) % img_w;
            if (((off_x + x) / img_w) % 2 == ex_par_x) {
                img_x = img_w - img_x - 1;
            }
            src_x[x] = img_x;
        }

        for (size_t y = 0; y < fill.height; ++y) {
            const bool flip_y =
                ((off_y + y) / img_h) % 2 == (exclude.y / img_h) % 2;
            size_t img_y = (y + off_y) % img_h;
            if (flip_y) {
                img_y = img_h - img_y - 1;
            }
            for (size_t x = 0; x < fill.width; ++x) {
                mirror.at(x, y) = image.at(src_x[x], img_y);
            }
        }
    }

    /**
     * Mirror and blur bottom area of pixmap.
     * @param pm target pixmap
     * @param fill area to fill
     * @param exclude area to exclude
     * @param image origin image
     */
    void fill_bottom(Pixmap& pm, const Rectangle& fill,
                     const Rectangle& exclude, const Pixmap& image)
    {
        Pixmap mirror = pm.submap(fill);
        const size_t img_h = image.height();
        const size_t img_w = image.width();

        // precompute horizontal source indexes
        const size_t off_x = img_w - (exclude.x % img_w);
        const size_t ex_par_x = (exclude.x / img_w) % 2;
        std::vector<size_t> src_x(fill.width);
        for (size_t x = 0; x < fill.width; ++x) {
            size_t img_x = (x + off_x) % img_w;
            if (((off_x + x) / img_w) % 2 == ex_par_x) {
                img_x = img_w - img_x - 1;
            }
            src_x[x] = img_x;
        }

        for (size_t y = 0; y < fill.height; ++y) {
            const bool flip_y = (y / img_h) % 2 == 0;
            size_t img_y = y % img_h;
            if (flip_y) {
                img_y = img_h - img_y - 1;
            }
            for (size_t x = 0; x < fill.width; ++x) {
                mirror.at(x, y) = image.at(src_x[x], img_y);
            }
        }
    }

    /**
     * Mirror and blur left area of pixmap.
     * @param pm target pixmap
     * @param fill area to fill
     * @param exclude area to exclude
     * @param image origin image
     */
    void fill_left(Pixmap& pm, const Rectangle& fill, const Rectangle& exclude,
                   const Pixmap& image)
    {
        Pixmap mirror = pm.submap(fill);
        const size_t img_h = image.height();
        const size_t img_w = image.width();

        // precompute horizontal source indexes
        const size_t off_x = img_w - (exclude.x % img_w);
        const size_t ex_par_x = (exclude.x / img_w) % 2;
        std::vector<size_t> src_x(fill.width);
        for (size_t x = 0; x < fill.width; ++x) {
            size_t img_x = (x + off_x) % img_w;
            if (((off_x + x) / img_w) % 2 == ex_par_x) {
                img_x = img_w - img_x - 1;
            }
            src_x[x] = img_x;
        }

        for (size_t y = 0; y < fill.height; ++y) {
            const size_t img_y = y % img_h;
            for (size_t x = 0; x < fill.width; ++x) {
                mirror.at(x, y) = image.at(src_x[x], img_y);
            }
        }
    }

    /**
     * Mirror and blur right area of pixmap.
     * @param pm target pixmap
     * @param fill area to fill
     * @param image origin image
     */
    void fill_right(Pixmap& pm, const Rectangle& fill, const Pixmap& image)
    {
        Pixmap mirror = pm.submap(fill);
        const size_t img_h = image.height();
        const size_t img_w = image.width();

        // precompute horizontal source indexes
        std::vector<size_t> src_x(fill.width);
        for (size_t x = 0; x < fill.width; ++x) {
            size_t img_x = x % img_w;
            if ((x / img_w) % 2 == 0) {
                img_x = img_w - img_x - 1;
            }
            src_x[x] = img_x;
        }

        for (size_t y = 0; y < fill.height; ++y) {
            const size_t img_y = y % img_h;
            for (size_t x = 0; x < fill.width; ++x) {
                mirror.at(x, y) = image.at(src_x[x], img_y);
            }
        }
    }

} // namespace Mirror

namespace Dim { // dimming area
    /**
     * Dim area outside the specified point.
     * @param pm target pixmap
     * @param start_y starting line
     * @param height number of lines to precess
     * @param pt preserved point coordinates
     * @param radius preserved radius in pixels
     * @param dim dimming factor to apply (0=black, 1=transparent)
     */
    void apply(Pixmap& pm, const size_t start_y, const size_t height,
               const Point& pt, const size_t radius, const double dim)
    {
        const ssize_t radius2 =
            static_cast<ssize_t>(radius) * static_cast<ssize_t>(radius);

        const size_t max_x = pm.width();
        const size_t max_y = start_y + height;

        // integer dimming factor in [0, 256] (0 = black, 256 = unchanged)
        const uint32_t scale = static_cast<uint32_t>(dim * 256.0);

        for (size_t y = start_y; y < max_y; ++y) {
            const ssize_t pty = static_cast<ssize_t>(y) - pt.y;
            const ssize_t pty2 = pty * pty;

            for (size_t x = 0; x < max_x; ++x) {
                const ssize_t ptx = static_cast<ssize_t>(x) - pt.x;
                if (ptx * ptx + pty2 > radius2) {
                    argb_t& px = pm.at(x, y);
                    px.r = (px.r * scale) >> 8;
                    px.g = (px.g * scale) >> 8;
                    px.b = (px.b * scale) >> 8;
                }
            }
        }
    }

} // namespace Dim

} // anonymous namespace

Render& Render::self()
{
    static Render singleton;
    return singleton;
}

Render::Render()
    : antialiasing(Defaults::render::antialiasing)
{
}

void Render::draw(Pixmap& dst, const Pixmap& src, const Point& pos,
                  const double scale)
{
    if (scale == 1) { // 100% scale, draw 1:1
        if (dst.format() == Pixmap::ARGB || src.format() == Pixmap::ARGB) {
            dst.blend(src, pos);
        } else {
            dst.copy(src, pos);
        }
        return;
    }

    if (antialiasing) {
        AA::draw(dst, src, pos, scale, tpool);
    } else {
        NN::draw(dst, src, pos, scale, tpool);
    }
}

void Render::fill_inverse(Pixmap& pm, const Rectangle& rect,
                          const argb_t& color)
{
    const Rectangle full { 0, 0, pm.width(), pm.height() };
    const auto [top, bottom, left, right] = full.cutout(rect);

    std::vector<size_t> tids;
    tids.reserve(4); // one per each side

    if (top) {
        tids.push_back(tpool.add([&pm, &top, &color] {
            pm.fill(top, color);
        }));
    }
    if (bottom) {
        tids.push_back(tpool.add([&pm, &bottom, &color] {
            pm.fill(bottom, color);
        }));
    }
    if (left) {
        tids.push_back(tpool.add([&pm, &left, &color] {
            pm.fill(left, color);
        }));
    }
    if (right) {
        tids.push_back(tpool.add([&pm, &right, &color] {
            pm.fill(right, color);
        }));
    }

    tpool.wait(tids);
}

void Render::extend_background(Pixmap& pm, const Rectangle& preserve)
{
    assert(preserve);

    // calculate areas to fill
    const Rectangle full { 0, 0, pm.width(), pm.height() };
    const Rectangle exclude = full.intersect(preserve);
    const auto [top, bottom, left, right] = full.cutout(exclude);

    // create source image slice
    Pixmap image;
    image.attach(pm.format(), exclude.width, exclude.height,
                 pm.ptr(exclude.x, exclude.y), pm.stride());

    // calculate source image scale
    const double scale =
        std::max(static_cast<double>(pm.width()) / preserve.width,
                 static_cast<double>(pm.height()) / preserve.height);

    // calculate image position
    const Point pos(static_cast<ssize_t>(pm.width() / 2) -
                        static_cast<ssize_t>(image.width() * scale / 2),
                    static_cast<ssize_t>(pm.height() / 2) -
                        static_cast<ssize_t>(image.height() * scale / 2));

    // fill background by nearest-neighbor copy
    if (top) {
        Pixmap sub = pm.submap(top);
        NN::draw(sub, image, pos, scale, tpool);
    }
    if (bottom) {
        Pixmap sub = pm.submap(bottom);
        NN::draw(sub, image, pos + Point(-bottom.x, -bottom.y), scale, tpool);
    }
    if (left) {
        Pixmap sub = pm.submap(left);
        NN::draw(sub, image, pos + Point(left.x, -left.y), scale, tpool);
    }
    if (right) {
        Pixmap sub = pm.submap(right);
        NN::draw(sub, image, pos + Point(-right.x, -right.y), scale, tpool);
    }

    // blur extended area
    Blur::apply(pm, exclude, tpool);
}

void Render::mirror_background(Pixmap& pm, const Rectangle& preserve)
{
    assert(preserve);

    // calculate areas to fill
    const Rectangle full { 0, 0, pm.width(), pm.height() };
    const Rectangle exclude = full.intersect(preserve);
    const auto [top, bottom, left, right] = full.cutout(exclude);

    // create source image slice
    Pixmap image;
    image.attach(pm.format(), exclude.width, exclude.height,
                 pm.ptr(exclude.x, exclude.y), pm.stride());

    std::vector<size_t> tids;
    tids.reserve(4); // one per each side

    // fill mirrors
    if (top) {
        tids.push_back(tpool.add(Mirror::fill_top, pm, top, exclude, image));
    }
    if (bottom) {
        tids.push_back(
            tpool.add(Mirror::fill_bottom, pm, bottom, exclude, image));
    }
    if (left) {
        tids.push_back(tpool.add(Mirror::fill_left, pm, left, exclude, image));
    }
    if (right) {
        tids.push_back(tpool.add(Mirror::fill_right, pm, right, image));
    }

    tpool.wait(tids);

    // blur mirrored area
    Blur::apply(pm, exclude, tpool);
}

void Render::dim_outside(Pixmap& pm, const Point& pt, const size_t radius,
                         const double dim)
{
    // callulate number of used threads
    const size_t total_pixels = pm.width() * pm.height();
    const size_t threads = std::clamp(total_pixels / MIN_PIXELS_PER_THREAD,
                                      static_cast<size_t>(1), tpool.size());
    if (threads == 1) {
        // single-thread fast path: avoid the thread pool round-trip
        Dim::apply(pm, 0, pm.height(), pt, radius, dim);
        return;
    }

    const size_t step = pm.height() / threads;

    std::vector<size_t> tids;
    tids.reserve(threads);

    for (size_t i = 0; i < threads; ++i) {
        const size_t y = step * i;
        size_t height = step;
        if (i == threads - 1) {
            height += pm.height() - step * threads;
        }
        tids.push_back(tpool.add(Dim::apply, pm, y, height, pt, radius, dim));
    }

    tpool.wait(tids);
}

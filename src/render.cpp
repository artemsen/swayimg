// SPDX-License-Identifier: MIT
// Multithreaded software renderer for raster images.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace NN { // nearest-neighbor
/**
 * Put part of one pixmap on another.
 * @param src_pm source pixmap (overlay)
 * @param src_pt top left point of source pixmap
 * @param src_scale scale factor of source pixmap
 * @param dst_pm destination pixmap (underlay)
 * @param dst_rect destination area to fill
 */
static void mix_pm(const Pixmap& src_pm, const Point& src_pt,
                   const double src_scale, Pixmap& dst_pm,
                   const Rectangle& dst_rect)
{
    for (size_t y = 0; y < dst_rect.height; ++y) {
        const size_t src_y = static_cast<double>(src_pt.y + y) / src_scale;
        const size_t dst_y = dst_rect.y + y;
        for (size_t x = 0; x < dst_rect.width; ++x) {
            const size_t src_x = static_cast<double>(src_pt.x + x) / src_scale;
            const argb_t src = src_pm.at(src_x, src_y);
            argb_t& dst = dst_pm.at(dst_rect.x + x, dst_y);
            if (src_pm.format() == Pixmap::ARGB) {
                dst.blend(src);
            } else {
                dst = src;
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
static void draw(Pixmap& dst, const Pixmap& src, const Point& pos,
                 const double scale, ThreadPool& tpool)
{
    const Rectangle image(pos, static_cast<Size>(src) * scale);
    const Rectangle visible = image.intersect(Rectangle({ 0, 0 }, dst));
    if (!visible) {
        return; // out of pixmap
    }

    std::vector<size_t> tids;

    Point src_start { visible.x - image.x, visible.y - image.y };
    const size_t threads = tpool.size();
    const size_t step = visible.height / threads;
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
            tpool.add(&mix_pm, src, src_pos, scale, dst, dst_rect);
        tids.push_back(tid);
    }

    tpool.wait(tids);
}

}; // namespace NN

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
static inline std::pair<ssize_t, ssize_t> get_bounds(size_t out, double scale)
{
    // Adjust by 0.5 to ensure sampling from the centers of pixels,
    // not their edges
    const double c = (out + 0.5) / scale - 0.5;
    const double d = WINDOW_SIZE / std::fmin(scale, 1.0);
    return std::make_pair(std::floor(c - d), std::ceil(c + d));
}

// Magic Kernel Sharp 2013
static inline double mks13(double x)
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
static double get_weight(size_t in, size_t out, double scale)
{
    double c, x;

    if (scale >= 1.0) {
        c = (out + 0.5) / scale - 0.5;
        x = fabs(in - c);
    } else {
        c = (in + 0.5) * scale - 0.5;
        x = fabs(out - c);
    }

    return x > WINDOW_SIZE ? 0.0 : mks13(x);
}

// Build a new fixed point kernel from its mathematical description
static void init_mks2013_kernel(Kernel& kernel, size_t nin, size_t nout,
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

    // The estimation overallocates, but kernels are only live for a short time
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
        double norm = 1.0 / sum;
        int16_t isum = 0;
        for (size_t in = first; in <= last; ++in) {
            const int16_t iw =
                std::round(weights[in - first] * norm * (1 << FIXED_BITS));
            int_weights[in - first] = iw;
            isum += iw;
        }
        int_weights[(last - first) / 2] += (1 << FIXED_BITS) - isum;

        // Ignore leading or trailing zeros
        size_t tfirst, tlast;
        for (tfirst = first; tfirst < last && int_weights[tfirst - first] == 0;
             ++tfirst) { }
        for (tlast = last; tlast > tfirst && int_weights[tlast - first] == 0;
             --tlast) { }
        if (tfirst < min_in) {
            min_in = tfirst;
        }
        if (tlast > max_in) {
            max_in = tlast;
        }

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

// Apply a horizontal kernel; the output pixmap is assumed to be only as tall as
// needed by the vertical pass - yoff indicates where it begins in the source
static void apply_hk(const Pixmap* src, Pixmap* dst, const Kernel* kernel,
                     const size_t y_low, const size_t y_high, const size_t yoff)
{
    if (src->format() == Pixmap::ARGB) {
        // Although this duplicates some code (the loop over y and x), doing the
        // check for alpha outside gave better performance (likely due to fewer
        // instructions in the loop body or fewer branch mispredictions)
        for (size_t y = y_low; y < y_high; ++y) {
            for (size_t x = 0; x < dst->width(); ++x) {
                const Output& output = kernel->outputs[x];
                int64_t a = 0;
                int64_t r = 0;
                int64_t g = 0;
                int64_t b = 0;
                for (size_t i = 0; i < output.n; ++i) {
                    const argb_t& c = src->at(output.first + i, y + yoff);
                    const int64_t wa = kernel->weights[output.index + i] * c.a;
                    a += wa;
                    r += c.r * wa;
                    g += c.g * wa;
                    b += c.b * wa;
                }
                // XXX if we want more accuracy (without sacrificing speed), we
                // could save more than 8 bits between the passes
                const uint8_t ua =
                    std::clamp(a >> FIXED_BITS, static_cast<int64_t>(0),
                               static_cast<int64_t>(255));
                if (a == 0) {
                    a = (1 << FIXED_BITS);
                }
                // TODO irrespective of the above, saving the intermediate with
                // premultiplied alpha would almost certainly improve
                // performance
                const uint8_t ur = std::clamp(r / a, static_cast<int64_t>(0),
                                              static_cast<int64_t>(255));
                const uint8_t ug = std::clamp(g / a, static_cast<int64_t>(0),
                                              static_cast<int64_t>(255));
                const uint8_t ub = std::clamp(b / a, static_cast<int64_t>(0),
                                              static_cast<int64_t>(255));
                dst->at(x, y).blend(argb_t(ua, ur, ug, ub));
            }
        }
    } else {
        for (size_t y = y_low; y < y_high; ++y) {
            for (size_t x = 0; x < dst->width(); ++x) {
                const Output& output = kernel->outputs[x];
                int64_t r = 0;
                int64_t g = 0;
                int64_t b = 0;
                for (size_t i = 0; i < output.n; ++i) {
                    const int64_t w = kernel->weights[output.index + i];
                    const argb_t& c = src->at(output.first + i, y + yoff);
                    r += c.r * w;
                    g += c.g * w;
                    b += c.b * w;
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
                dst->at(x, y) = argb_t(0xff, ur, ug, ub);
            }
        }
    }
}

// Apply a vertical kernel; the input pixmap is assumed to be only as tall as
// needed - xoff indicates where it should go in the destination
static void apply_vk(const Pixmap* src, Pixmap* dst,
                     const struct Kernel* kernel, const size_t y_low,
                     const size_t y_high, const size_t xoff)
{
    if (src->format() == Pixmap::ARGB) {
        for (size_t y = y_low; y < y_high; ++y) {
            for (size_t x = 0; x < src->width(); ++x) {
                const Output& output = kernel->outputs[y];
                int64_t a = 0;
                int64_t r = 0;
                int64_t g = 0;
                int64_t b = 0;
                for (size_t i = 0; i < output.n; ++i) {
                    const argb_t& c =
                        src->at(x, output.first + i - kernel->start_in);
                    const int64_t wa = kernel->weights[output.index + i] * c.a;
                    a += wa;
                    r += c.r * wa;
                    g += c.g * wa;
                    b += c.b * wa;
                }
                const uint8_t ua =
                    std::clamp(a >> FIXED_BITS, static_cast<int64_t>(0),
                               static_cast<int64_t>(255));
                if (a == 0) {
                    a = (1 << FIXED_BITS);
                }
                const uint8_t ur = std::clamp(r / a, static_cast<int64_t>(0),
                                              static_cast<int64_t>(255));
                const uint8_t ug = std::clamp(g / a, static_cast<int64_t>(0),
                                              static_cast<int64_t>(255));
                const uint8_t ub = std::clamp(b / a, static_cast<int64_t>(0),
                                              static_cast<int64_t>(255));
                dst->at(x + xoff, y + kernel->start_out)
                    .blend(argb_t(ua, ur, ug, ub));
            }
        }
    } else {
        for (size_t y = y_low; y < y_high; ++y) {
            for (size_t x = 0; x < src->width(); ++x) {
                const Output& output = kernel->outputs[y];
                int64_t r = 0;
                int64_t g = 0;
                int64_t b = 0;
                for (size_t i = 0; i < output.n; ++i) {
                    const argb_t& c =
                        src->at(x, output.first + i - kernel->start_in);
                    const int64_t w = kernel->weights[output.index + i];
                    r += c.r * w;
                    g += c.g * w;
                    b += c.b * w;
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
                dst->at(x + xoff, y + kernel->start_out) =
                    argb_t(0xff, ur, ug, ub);
            }
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
static void draw(Pixmap& dst, const Pixmap& src, const Point& pos,
                 const double scale, ThreadPool& tpool)
{
    const Rectangle image(pos, static_cast<Size>(src) * scale);
    const Rectangle visible = image.intersect(Rectangle({ 0, 0 }, dst));
    if (!visible) {
        return; // out of pixmap
    }

    // initialize mks2013 kernels
    Kernel kernel_hor;
    Kernel kernel_ver;
    init_mks2013_kernel(kernel_hor, src.width(), dst.width(), pos.x, scale);
    init_mks2013_kernel(kernel_ver, src.height(), dst.height(), pos.y, scale);

    // intermediate pixmap
    Pixmap tmp;
    tmp.create(src.format(), kernel_hor.n_out, kernel_ver.n_in);

    // per thread ranges to render
    const size_t threads = tpool.size();
    const size_t hlen = kernel_ver.n_in / threads;
    const size_t vlen = kernel_ver.n_out / threads;

    // apply horizontal kernel and write result to temporary pixmap
    std::vector<size_t> tids;
    for (size_t i = 0; i < threads; ++i) {
        const bool last = i == threads - 1;
        const size_t from = i * hlen;
        const size_t to = last ? kernel_ver.n_in : from + hlen;
        const size_t tid = tpool.add(&apply_hk, &src, &tmp, &kernel_hor, from,
                                     to, kernel_ver.start_in);
        tids.push_back(tid);
    }
    tpool.wait(tids);

    // apply vertical kernel
    tids.clear();
    for (size_t i = 0; i < threads; ++i) {
        const bool last = i == threads - 1;
        const size_t from = i * vlen;
        const size_t to = last ? kernel_ver.n_out : from + vlen;
        const size_t tid = tpool.add(&apply_vk, &tmp, &dst, &kernel_ver, from,
                                     to, kernel_hor.start_out);
        tids.push_back(tid);
    }
    tpool.wait(tids);
}
}; // namespace AA

namespace Blur {
// Constant parameters
constexpr size_t BLUR_SIZE = 3;
constexpr size_t BLUR_SIGMA = 16;

/** Color accumulator. */
struct ColorAccum {

    /**
     * Constructor.
     * @param color RGB color to set
     * @param factor color factor
     */
    ColorAccum(const argb_t& color, const double factor)
        : r(factor * color.r)
        , g(factor * color.g)
        , b(factor * color.b)
    {
    }

    /**
     * Add color to accumulator.
     * @param color RGB color to add
     * @return self reference
     */
    inline ColorAccum& operator+=(const argb_t& color)
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
    inline ColorAccum& operator-=(const argb_t& color)
    {
        r -= color.r;
        g -= color.g;
        b -= color.b;
        return *this;
    }

    /**
     * Create ARGB color from accumulator.
     * @param weight weight of the components
     * @return ARGB color
     */
    inline argb_t argb(const double weight) const
    {
        const argb_t::channel cr =
            std::clamp(r * weight, static_cast<double>(argb_t::min),
                       static_cast<double>(argb_t::max));
        const argb_t::channel cg =
            std::clamp(g * weight, static_cast<double>(argb_t::min),
                       static_cast<double>(argb_t::max));
        const argb_t::channel cb =
            std::clamp(b * weight, static_cast<double>(argb_t::min),
                       static_cast<double>(argb_t::max));
        return argb_t(argb_t::max, cr, cg, cb);
    }

    double r, g, b;
};

/**
 * Blur pixmap horizontally.
 * @param pm target pixmap
 * @param radius blur radius
 */
void apply_hor(Pixmap& pm, const size_t radius)
{
    const size_t radius_plus = radius + 1;
    const double weight = 1.0 / (radius + radius_plus);

    for (size_t y = 0; y < pm.height(); ++y) {
        const argb_t px_first = pm.at(0, y);
        const argb_t px_last = pm.at(pm.width() - 1, y);

        ColorAccum cacc(px_first, radius_plus);
        for (size_t x = 0; x < radius && x < pm.width(); ++x) {
            cacc += pm.at(x, y);
        }

        for (size_t x = 0; x <= radius && x + radius < pm.width(); ++x) {
            cacc += pm.at(x + radius, y);
            cacc -= px_first;
            pm.at(x, y) = cacc.argb(weight);
        }

        for (size_t x = radius_plus; x + radius < pm.width(); ++x) {
            cacc += pm.at(x + radius, y);
            cacc -= pm.at(x - radius_plus, y);
            pm.at(x, y) = cacc.argb(weight);
        }

        for (size_t x = pm.width() - radius; x < pm.width() && x >= radius_plus;
             ++x) {
            cacc += px_last;
            cacc -= pm.at(x - radius_plus, y);
            pm.at(x, y) = cacc.argb(weight);
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
    const size_t radius_plus = radius + 1;
    const double weight = 1.0 / (radius + radius_plus);

    for (size_t x = 0; x < pm.width(); ++x) {
        const argb_t px_first = pm.at(x, 0);
        const argb_t px_last = pm.at(x, pm.height() - 1);

        ColorAccum cacc(px_first, radius_plus);
        for (size_t y = 0; y < radius && y < pm.height(); ++y) {
            cacc += pm.at(x, y);
        }

        for (size_t y = 0; y <= radius && y + radius < pm.height(); ++y) {
            cacc += pm.at(x, y + radius);
            cacc -= px_first;
            pm.at(x, y) = cacc.argb(weight);
        }

        for (size_t y = radius_plus; y + radius < pm.height(); ++y) {
            cacc += pm.at(x, y + radius);
            cacc -= pm.at(x, y - radius_plus);
            pm.at(x, y) = cacc.argb(weight);
        }

        for (size_t y = pm.height() - radius;
             y < pm.height() && y >= radius_plus; ++y) {
            cacc += px_last;
            cacc -= pm.at(x, y - radius_plus);
            pm.at(x, y) = cacc.argb(weight);
        }
    }
}

/**
 * Apply Gaussian blur to pixmap slice.
 * @param pm target pixmap
 * @param exclude excluded area to preserve
 * @param tpool thread pool
 */
static void apply(Pixmap& pm, const Rectangle& exclude, ThreadPool& tpool)
{
    const Rectangle full { 0, 0, pm.width(), pm.height() };
    const auto [top, bottom, left, right] = full.cutout(exclude);

    const double sigma12 = 12 * BLUR_SIGMA * BLUR_SIGMA;
    size_t weight_min, weight_max;
    size_t weight_tran;
    static size_t blur_box[BLUR_SIZE] = { 0 };

    if (!blur_box[0]) {
        // create Gaussian blur box
        weight_min = sqrt(sigma12 / BLUR_SIZE + 1);
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
    auto blur_fn = [](Pixmap& pm) {
        for (size_t i = 0; i < BLUR_SIZE; ++i) {
            const size_t radius = (blur_box[i] - 1) / 2;
            apply_hor(pm, radius);
            apply_ver(pm, radius);
        }
    };

    // blur each pixmap block
    std::vector<size_t> tids;
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

}; // namespace Blur

Render& Render::self()
{
    static Render singleton;
    return singleton;
}

void Render::draw(Pixmap& dst, const Pixmap& src, const Point& pos,
                  const double scale)
{
    if (scale == 1) { // 100% scale, draw 1:1
        if (dst.format() == Pixmap::ARGB || src.format() == Pixmap::ARGB) {
            dst.blend(src, pos.x, pos.y);
        } else {
            dst.copy(src, pos.x, pos.y);
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

    if (top) {
        tids.push_back(tpool.add([&pm, &top, &color]() {
            pm.fill(top, color);
        }));
    }
    if (bottom) {
        tids.push_back(tpool.add([&pm, &bottom, &color]() {
            pm.fill(bottom, color);
        }));
    }
    if (left) {
        tids.push_back(tpool.add([&pm, &left, &color]() {
            pm.fill(left, color);
        }));
    }
    if (right) {
        tids.push_back(tpool.add([&pm, &right, &color]() {
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

    // fill mirrors
    std::vector<size_t> tids;

    if (top) {
        tids.push_back(tpool.add([&pm, &top, &exclude, &image]() {
            Pixmap mirror = pm.submap(top);
            const size_t img_h = image.height();
            const size_t img_w = image.width();
            const size_t off_y = img_h - (exclude.y % img_h);
            for (size_t y = 0; y < top.height; ++y) {
                const bool flip_y =
                    ((off_y + y) / img_h) % 2 == (exclude.y / img_h) % 2;
                size_t img_y = (y + off_y) % img_h;
                if (flip_y) {
                    img_y = img_h - img_y - 1;
                }
                for (size_t x = 0; x < top.width; ++x) {
                    const size_t off_x = img_w - (exclude.x % img_w);
                    const bool flip_x =
                        ((off_x + x) / img_w) % 2 == (exclude.x / img_w) % 2;
                    size_t img_x = (x + off_x) % img_w;
                    if (flip_x) {
                        img_x = img_w - img_x - 1;
                    }
                    mirror.at(x, y) = image.at(img_x, img_y);
                }
            }
        }));
    }
    if (bottom) {
        tids.push_back(tpool.add([&pm, &bottom, &exclude, &image]() {
            Pixmap mirror = pm.submap(bottom);
            const size_t img_h = image.height();
            const size_t img_w = image.width();
            for (size_t y = 0; y < bottom.height; ++y) {
                const bool flip_y = (y / img_h) % 2 == 0;
                size_t img_y = y % img_h;
                if (flip_y) {
                    img_y = img_h - img_y - 1;
                }
                for (size_t x = 0; x < bottom.width; ++x) {
                    const size_t off_x = img_w - (exclude.x % img_w);
                    const bool flip_x =
                        ((off_x + x) / img_w) % 2 == (exclude.x / img_w) % 2;
                    size_t img_x = (x + off_x) % img_w;
                    if (flip_x) {
                        img_x = img_w - img_x - 1;
                    }
                    mirror.at(x, y) = image.at(img_x, img_y);
                }
            }
        }));
    }
    if (left) {
        tids.push_back(tpool.add([&pm, &left, &exclude, &image]() {
            Pixmap mirror = pm.submap(left);
            const size_t img_h = image.height();
            const size_t img_w = image.width();
            for (size_t y = 0; y < left.height; ++y) {
                size_t img_y = y % img_h;
                for (size_t x = 0; x < left.width; ++x) {
                    const size_t off_x = img_w - (exclude.x % img_w);
                    const bool flip_x =
                        ((off_x + x) / img_w) % 2 == (exclude.x / img_w) % 2;
                    size_t img_x = (x + off_x) % img_w;
                    if (flip_x) {
                        img_x = img_w - img_x - 1;
                    }
                    mirror.at(x, y) = image.at(img_x, img_y);
                }
            }
        }));
    }
    if (right) {
        tids.push_back(tpool.add([&pm, &right, &image]() {
            Pixmap mirror = pm.submap(right);
            const size_t img_h = image.height();
            const size_t img_w = image.width();
            for (size_t y = 0; y < right.height; ++y) {
                size_t img_y = y % img_h;
                for (size_t x = 0; x < right.width; ++x) {
                    const bool flip_x = (x / img_w) % 2 == 0;
                    size_t img_x = x % img_w;
                    if (flip_x) {
                        img_x = img_w - img_x - 1;
                    }
                    mirror.at(x, y) = image.at(img_x, img_y);
                }
            }
        }));
    }

    tpool.wait(tids);

    // blur mirrored area
    Blur::apply(pm, exclude, tpool);
}

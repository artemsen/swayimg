#include "loader.h"
#include "pixmap.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void time_scale(enum pixmap_scale scaler, const struct pixmap* src,
                       float scale, size_t iters)
{
    struct pixmap dst;
    struct timespec start, end;

    if (!pixmap_create(&dst, src->width * scale, src->height * scale)) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < iters; ++i) {
        pixmap_scale(scaler, src, &dst, 0, 0, scale, false);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    if (end.tv_nsec < start.tv_nsec) {
        end.tv_nsec += 1000000000;
        end.tv_sec -= 1;
    }
    printf("%11s %f (%zu iters): %03ld.%09ld\n", pixmap_scale_names[scaler],
           scale, iters, end.tv_sec - start.tv_sec,
           end.tv_nsec - start.tv_nsec);
    pixmap_free(&dst);
}

void pixmap_scale_bench(const struct pixmap* pm)
{
    size_t iters = 100;
    time_scale(pixmap_nearest, pm, 0.25, iters);
    time_scale(pixmap_box, pm, 0.25, iters);
    time_scale(pixmap_bilinear, pm, 0.25, iters);
    time_scale(pixmap_bicubic, pm, 0.25, iters);
    time_scale(pixmap_mks13, pm, 0.25, iters);
    time_scale(pixmap_nearest, pm, 0.5, iters);
    time_scale(pixmap_box, pm, 0.5, iters);
    time_scale(pixmap_bilinear, pm, 0.5, iters);
    time_scale(pixmap_bicubic, pm, 0.5, iters);
    time_scale(pixmap_mks13, pm, 0.5, iters);
    time_scale(pixmap_nearest, pm, 2.0, iters);
    time_scale(pixmap_box, pm, 2.0, iters);
    time_scale(pixmap_bilinear, pm, 2.0, iters);
    time_scale(pixmap_bicubic, pm, 2.0, iters);
    time_scale(pixmap_mks13, pm, 2.0, iters);
    time_scale(pixmap_nearest, pm, 4.0, iters);
    time_scale(pixmap_box, pm, 4.0, iters);
    time_scale(pixmap_bilinear, pm, 4.0, iters);
    time_scale(pixmap_bicubic, pm, 4.0, iters);
    time_scale(pixmap_mks13, pm, 4.0, iters);
}

static void test_scale(enum pixmap_scale scaler, const struct pixmap* src,
                       float scale, const char* file)
{
    struct pixmap dst;

    if (!pixmap_create(&dst, src->width * scale, src->height * scale)) {
        return;
    }
    pixmap_scale(scaler, src, &dst, 0, 0, scale, false);
    pixmap_to_file(&dst, file);
    pixmap_free(&dst);
}

void pixmap_scale_test(const struct pixmap* pm)
{
    test_scale(pixmap_box, pm, 2.0, "box.ppm");
    test_scale(pixmap_bilinear, pm, 2.0, "lin.ppm");
    test_scale(pixmap_bicubic, pm, 2.0, "cub.ppm");
    test_scale(pixmap_mks13, pm, 2.0, "mks13_dir.ppm");
}

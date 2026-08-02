// lib/acr/tests/classic/e10_mandelbrot.cpp — E10 Branch Divergence (Mandelbrot)
// 验证能力：工作量不均
// Cases: mandelbrot_full / mandelbrot_zoom / mandelbrot_edge / escape_iter
// 每个像素的逃逸迭代数差异巨大（边界点 max_iter，内部点 0-几次）。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// 串行 reference：Mandelbrot 逃逸迭代数
void ref_mandelbrot(std::vector<int>& out, std::size_t w, std::size_t h,
                    double cx_min, double cx_max, double cy_min, double cy_max,
                    int max_iter) {
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            double cx = cx_min + (cx_max - cx_min) * x / (w - 1);
            double cy = cy_min + (cy_max - cy_min) * y / (h - 1);
            double zx = 0.0, zy = 0.0;
            int iter = 0;
            while (iter < max_iter && zx * zx + zy * zy < 4.0) {
                double nzx = zx * zx - zy * zy + cx;
                zy = 2.0 * zx * zy + cy;
                zx = nzx;
                ++iter;
            }
            out[y * w + x] = iter;
        }
    }
}

CaseResult run_mandelbrot(std::size_t w, std::size_t h,
                          double cx_min, double cx_max, double cy_min, double cy_max,
                          int max_iter, const std::string& case_id) {
    std::vector<int> out(w * h, 0), ref_out(w * h, 0);
    ref_mandelbrot(ref_out, w, h, cx_min, cx_max, cy_min, cy_max, max_iter);

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Mandelbrot, Extent2D{w, h},
            [&](std::size_t x, std::size_t y) {
                double cx = cx_min + (cx_max - cx_min) * x / (w - 1);
                double cy = cy_min + (cy_max - cy_min) * y / (h - 1);
                double zx = 0.0, zy = 0.0;
                int iter = 0;
                while (iter < max_iter && zx * zx + zy * zy < 4.0) {
                    double nzx = zx * zx - zy * zy + cx;
                    zy = 2.0 * zx * zy + cy;
                    zx = nzx;
                    ++iter;
                }
                out[y * w + x] = iter;
            });
    });

    auto err = compute_errors_int(out.data(), ref_out.data(), w * h);
    bool ok = (err.max_abs == 0.0);  // 整数 exact
    return make_result("E10", case_id, "integer", w * h, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "mandelbrot mismatch");
}

} // anonymous namespace

TEST(E10Mandelbrot, FullView64)    { auto r = run_mandelbrot(64, 64, -2.5, 1.0, -1.25, 1.25, 256, "full_64");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, FullView128)   { auto r = run_mandelbrot(128, 128, -2.5, 1.0, -1.25, 1.25, 256, "full_128");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, ZoomEdge)      { auto r = run_mandelbrot(64, 64, -0.75, -0.74, 0.10, 0.11, 1024, "zoom_edge"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, HighIter)      { auto r = run_mandelbrot(32, 32, -2.0, 0.5, -1.25, 1.25, 4096, "high_iter");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, RectWide)      { auto r = run_mandelbrot(128, 32, -2.5, 1.0, -0.5, 0.5, 128, "rect_wide");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, SmallMaxIter)  { auto r = run_mandelbrot(64, 64, -2.5, 1.0, -1.25, 1.25, 16, "small_iter");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, Edge10x10)     { auto r = run_mandelbrot(10, 10, -2.5, 1.0, -1.25, 1.25, 64, "edge_10x10");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, Cardioid)      { auto r = run_mandelbrot(64, 64, -0.5, 0.3, -0.4, 0.4, 512, "cardioid");       ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e10() {
    return {
        run_mandelbrot(64, 64, -2.5, 1.0, -1.25, 1.25, 256, "full_64"),
        run_mandelbrot(128, 128, -2.5, 1.0, -1.25, 1.25, 256, "full_128"),
        run_mandelbrot(64, 64, -0.75, -0.74, 0.10, 0.11, 1024, "zoom_edge"),
        run_mandelbrot(32, 32, -2.0, 0.5, -1.25, 1.25, 4096, "high_iter"),
        run_mandelbrot(128, 32, -2.5, 1.0, -0.5, 0.5, 128, "rect_wide"),
        run_mandelbrot(64, 64, -2.5, 1.0, -1.25, 1.25, 16, "small_iter"),
        run_mandelbrot(10, 10, -2.5, 1.0, -1.25, 1.25, 64, "edge_10x10"),
        run_mandelbrot(64, 64, -0.5, 0.3, -0.4, 0.4, 512, "cardioid"),
    };
}

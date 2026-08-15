// lib/acr/tests/classic/e10_mandelbrot.cpp — E10 Branch Divergence (Mandelbrot)
// 验证能力（17 §15）：工作量不均、分支发散
//
// Phase H 扩展：
// - 整数逃逸计数（exact）
// - 区域：快速逃逸、边界高迭代、混合
// - 尺寸：1024²、4096²（大尺寸限部分区域控制时长）
// - 画像记录 uniformity 与设备差异
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

// 串行 reference：Mandelbrot 逃逸迭代数（整数 exact）
void ref_mandelbrot(std::vector<int>& out, std::size_t w, std::size_t h,
                    double cx_min, double cx_max, double cy_min, double cy_max,
                    int max_iter) {
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            double cx = cx_min + (cx_max - cx_min) * x / (w > 1 ? w - 1 : 1);
            double cy = cy_min + (cy_max - cy_min) * y / (h > 1 ? h - 1 : 1);
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
                          int max_iter, const std::string& case_id,
                          const std::string& region_label) {
    std::vector<int> out(w * h, 0), ref_out(w * h, 0);
    ref_mandelbrot(ref_out, w, h, cx_min, cx_max, cy_min, cy_max, max_iter);

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Mandelbrot, Extent2D{w, h},
            [&](std::size_t x, std::size_t y) {
                double cx = cx_min + (cx_max - cx_min) * x / (w > 1 ? w - 1 : 1);
                double cy = cy_min + (cy_max - cy_min) * y / (h > 1 ? h - 1 : 1);
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
                       ok ? "PASS" : "FAIL", ok ? "" : "mandelbrot mismatch",
                       "cpu", "cpu");
}

// ===== 区域定义 =====
// 快速逃逸：远离集合，大部分点 1-5 次迭代逃逸
struct Region { double cx_min, cx_max, cy_min, cy_max; int max_iter; const char* label; };

// 快速逃逸区域（远离 Mandelbrot 集）
constexpr Region kFastEscape = {-2.5, 1.0, -1.25, 1.25, 64, "fast"};
// 边界高迭代区域（Mandelbrot 集边界，大量点 max_iter 不逃逸）
constexpr Region kBoundaryHighIter = {-0.75, -0.74, 0.10, 0.11, 4096, "boundary"};
// 混合区域（含内部、边界、外部）
constexpr Region kMixed = {-2.0, 0.5, -1.25, 1.25, 1024, "mixed"};
// 主心形区（大部分内部点 max_iter 不逃逸）
constexpr Region kCardioid = {-0.5, 0.3, -0.4, 0.4, 2048, "cardioid"};

} // anonymous namespace

// ===== 向后兼容 TEST =====
TEST(E10Mandelbrot, FullView64)    { auto r = run_mandelbrot(64, 64, -2.5, 1.0, -1.25, 1.25, 256, "full_64", "compat");      ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, FullView128)   { auto r = run_mandelbrot(128, 128, -2.5, 1.0, -1.25, 1.25, 256, "full_128", "compat");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, ZoomEdge)      { auto r = run_mandelbrot(64, 64, -0.75, -0.74, 0.10, 0.11, 1024, "zoom_edge", "compat");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, HighIter)      { auto r = run_mandelbrot(32, 32, -2.0, 0.5, -1.25, 1.25, 4096, "high_iter", "compat");    ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E10Mandelbrot, Cardioid)      { auto r = run_mandelbrot(64, 64, -0.5, 0.3, -0.4, 0.4, 512, "cardioid", "compat");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

// ===== Phase H 扩展：17 §15 规范 =====
// 快速逃逸 1024²
TEST(E10Mandelbrot, FastEscape_1024) {
    auto r = run_mandelbrot(1024, 1024, kFastEscape.cx_min, kFastEscape.cx_max,
                             kFastEscape.cy_min, kFastEscape.cy_max, kFastEscape.max_iter,
                             "fast_escape_1024", kFastEscape.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 快速逃逸 4096²
TEST(E10Mandelbrot, FastEscape_4096) {
    auto r = run_mandelbrot(4096, 4096, kFastEscape.cx_min, kFastEscape.cx_max,
                             kFastEscape.cy_min, kFastEscape.cy_max, kFastEscape.max_iter,
                             "fast_escape_4096", kFastEscape.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 边界高迭代 1024²
TEST(E10Mandelbrot, BoundaryHighIter_1024) {
    auto r = run_mandelbrot(1024, 1024, kBoundaryHighIter.cx_min, kBoundaryHighIter.cx_max,
                             kBoundaryHighIter.cy_min, kBoundaryHighIter.cy_max, kBoundaryHighIter.max_iter,
                             "boundary_high_1024", kBoundaryHighIter.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 边界高迭代 2048²（4096²×4096 迭代太慢，限 2048²）
TEST(E10Mandelbrot, BoundaryHighIter_2048) {
    auto r = run_mandelbrot(2048, 2048, kBoundaryHighIter.cx_min, kBoundaryHighIter.cx_max,
                             kBoundaryHighIter.cy_min, kBoundaryHighIter.cy_max, kBoundaryHighIter.max_iter,
                             "boundary_high_2048", kBoundaryHighIter.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 混合 1024²
TEST(E10Mandelbrot, Mixed_1024) {
    auto r = run_mandelbrot(1024, 1024, kMixed.cx_min, kMixed.cx_max,
                             kMixed.cy_min, kMixed.cy_max, kMixed.max_iter,
                             "mixed_1024", kMixed.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 混合 4096²
TEST(E10Mandelbrot, Mixed_4096) {
    auto r = run_mandelbrot(4096, 4096, kMixed.cx_min, kMixed.cx_max,
                             kMixed.cy_min, kMixed.cy_max, kMixed.max_iter,
                             "mixed_4096", kMixed.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 主心形 1024²（内部点密集，max_iter 高）
TEST(E10Mandelbrot, Cardioid_1024) {
    auto r = run_mandelbrot(1024, 1024, kCardioid.cx_min, kCardioid.cx_max,
                             kCardioid.cy_min, kCardioid.cy_max, kCardioid.max_iter,
                             "cardioid_1024", kCardioid.label);
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}
// 小 max_iter 边界尺寸验证
TEST(E10Mandelbrot, SmallMaxIter_1024) {
    auto r = run_mandelbrot(1024, 1024, -2.5, 1.0, -1.25, 1.25, 16, "small_iter_1024", "fast");
    ResultSink::instance().push(r); EXPECT_TRUE(r.correct);
}

extern "C" std::vector<CaseResult> run_e10() {
    return {
        // 向后兼容
        run_mandelbrot(64, 64, -2.5, 1.0, -1.25, 1.25, 256, "full_64", "compat"),
        run_mandelbrot(128, 128, -2.5, 1.0, -1.25, 1.25, 256, "full_128", "compat"),
        run_mandelbrot(64, 64, -0.75, -0.74, 0.10, 0.11, 1024, "zoom_edge", "compat"),
        run_mandelbrot(32, 32, -2.0, 0.5, -1.25, 1.25, 4096, "high_iter", "compat"),
        run_mandelbrot(64, 64, -0.5, 0.3, -0.4, 0.4, 512, "cardioid", "compat"),
        // Phase H 扩展
        run_mandelbrot(1024, 1024, kFastEscape.cx_min, kFastEscape.cx_max,
                       kFastEscape.cy_min, kFastEscape.cy_max, kFastEscape.max_iter,
                       "fast_escape_1024", kFastEscape.label),
        run_mandelbrot(4096, 4096, kFastEscape.cx_min, kFastEscape.cx_max,
                       kFastEscape.cy_min, kFastEscape.cy_max, kFastEscape.max_iter,
                       "fast_escape_4096", kFastEscape.label),
        run_mandelbrot(1024, 1024, kBoundaryHighIter.cx_min, kBoundaryHighIter.cx_max,
                       kBoundaryHighIter.cy_min, kBoundaryHighIter.cy_max, kBoundaryHighIter.max_iter,
                       "boundary_high_1024", kBoundaryHighIter.label),
        run_mandelbrot(2048, 2048, kBoundaryHighIter.cx_min, kBoundaryHighIter.cx_max,
                       kBoundaryHighIter.cy_min, kBoundaryHighIter.cy_max, kBoundaryHighIter.max_iter,
                       "boundary_high_2048", kBoundaryHighIter.label),
        run_mandelbrot(1024, 1024, kMixed.cx_min, kMixed.cx_max,
                       kMixed.cy_min, kMixed.cy_max, kMixed.max_iter,
                       "mixed_1024", kMixed.label),
        run_mandelbrot(4096, 4096, kMixed.cx_min, kMixed.cx_max,
                       kMixed.cy_min, kMixed.cy_max, kMixed.max_iter,
                       "mixed_4096", kMixed.label),
        run_mandelbrot(1024, 1024, kCardioid.cx_min, kCardioid.cx_max,
                       kCardioid.cy_min, kCardioid.cy_max, kCardioid.max_iter,
                       "cardioid_1024", kCardioid.label),
        run_mandelbrot(1024, 1024, -2.5, 1.0, -1.25, 1.25, 16, "small_iter_1024", "fast"),
    };
}

// lib/acr/tests/classic/e04_transpose.cpp — E04 Tiled Matrix Transpose
// 验证能力：2D range、Tile、边缘
// Cases: transpose_square / transpose_rect / tile_4x4 / tile_16x16 / edge_10x10
// 用 parallel_tiles 实现 tile 化转置，验证边缘 tile 正确 clamp。
#include "classic_common.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

namespace {

void fill_matrix(std::vector<float>& m, std::size_t rows, std::size_t cols, std::uint64_t seed) {
    LCG rng(seed);
    for (std::size_t i = 0; i < rows * cols; ++i) {
        m[i] = static_cast<float>(rng.next_double() * 2.0 - 1.0);
    }
}

// 串行转置 reference
void ref_transpose(const std::vector<float>& src, std::vector<float>& dst,
                   std::size_t rows, std::size_t cols) {
    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < cols; ++x) {
            dst[x * rows + y] = src[y * cols + x];
        }
    }
}

// Tile 化转置：用 parallel_tiles，每 tile 处理 tile_w × tile_h 块
CaseResult run_tiled_transpose(std::size_t rows, std::size_t cols,
                               std::size_t tile_w, std::size_t tile_h,
                               const std::string& case_id) {
    std::vector<float> src(rows * cols), dst(rows * cols, 0.0f), ref_dst(cols * rows, 0.0f);
    fill_matrix(src, rows, cols, FIXED_SEED);
    ref_transpose(src, ref_dst, rows, cols);

    std::size_t dst_rows = cols, dst_cols = rows;
    Extent2D extent{dst_cols, dst_rows};  // tile 网格按 dst 维度
    TileShape tile{tile_w, tile_h};

    auto tm = measure_timing([&] {
        parallel_tiles(KernelId::Transpose, extent, tile,
            [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
                // dst 坐标：(tx*tile_w + i, ty*tile_h + j)
                // src 坐标：(ty*tile_h + j, tx*tile_w + i) → 但转置后 dst[r][c] = src[c][r]
                for (std::size_t j = 0; j < th; ++j) {
                    for (std::size_t i = 0; i < tw; ++i) {
                        std::size_t dx = tx * tile_w + i;
                        std::size_t dy = ty * tile_h + j;
                        if (dx < dst_cols && dy < dst_rows) {
                            // dst[dy * dst_cols + dx] = src[dx * cols + dy]
                            // dst 维度 cols×rows：dst[c][r] = src[r][c]
                            dst[dy * dst_cols + dx] = src[dx * cols + dy];
                        }
                    }
                }
            });
    });

    // 验证：dst 与 ref_dst 都是 cols×rows 矩阵
    auto err = compute_errors<float>(dst.data(), ref_dst.data(), rows * cols);
    bool ok = (err.max_abs <= 1e-6);  // 转置应 bit-exact
    return make_result("E04", case_id, "fp32", rows * cols, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "transpose mismatch");
}

// parallel_for_2d 转置（每像素一个 kernel）
CaseResult run_2d_transpose(std::size_t rows, std::size_t cols, const std::string& case_id) {
    std::vector<float> src(rows * cols), dst(cols * rows, 0.0f), ref_dst(cols * rows, 0.0f);
    fill_matrix(src, rows, cols, FIXED_SEED);
    ref_transpose(src, ref_dst, rows, cols);

    std::size_t dst_rows = cols, dst_cols = rows;
    Extent2D extent{dst_cols, dst_rows};

    auto tm = measure_timing([&] {
        parallel_for_2d(KernelId::Transpose, extent,
            [&](std::size_t x, std::size_t y) {
                dst[y * dst_cols + x] = src[x * cols + y];
            });
    });

    auto err = compute_errors<float>(dst.data(), ref_dst.data(), rows * cols);
    bool ok = (err.max_abs <= 1e-6);
    return make_result("E04", case_id, "fp32", rows * cols, ok, err, tm,
                       ok ? "PASS" : "FAIL", ok ? "" : "2d transpose mismatch");
}

} // anonymous namespace

TEST(E04Transpose, Square64Tile4)   { auto r = run_tiled_transpose(64, 64, 4, 4, "square64_tile4");   ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Square256Tile16) { auto r = run_tiled_transpose(256, 256, 16, 16, "square256_tile16"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Rect8x32Tile4)   { auto r = run_tiled_transpose(8, 32, 4, 4, "rect_8x32_tile4");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Edge10x10Tile4)  { auto r = run_tiled_transpose(10, 10, 4, 4, "edge_10x10_tile4"); ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, Edge13x7Tile4)   { auto r = run_tiled_transpose(13, 7, 4, 4, "edge_13x7_tile4");  ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, For2dSquare64)   { auto r = run_2d_transpose(64, 64, "for2d_square64");          ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, For2dEdge10x10)  { auto r = run_2d_transpose(10, 10, "for2d_edge10x10");         ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }
TEST(E04Transpose, For2dLarge512)   { auto r = run_2d_transpose(512, 512, "for2d_large512");        ResultSink::instance().push(r); EXPECT_TRUE(r.correct); }

extern "C" std::vector<CaseResult> run_e04() {
    return {
        run_tiled_transpose(64, 64, 4, 4, "square64_tile4"),
        run_tiled_transpose(256, 256, 16, 16, "square256_tile16"),
        run_tiled_transpose(8, 32, 4, 4, "rect_8x32_tile4"),
        run_tiled_transpose(10, 10, 4, 4, "edge_10x10_tile4"),
        run_tiled_transpose(13, 7, 4, 4, "edge_13x7_tile4"),
        run_2d_transpose(64, 64, "for2d_square64"),
        run_2d_transpose(10, 10, "for2d_edge10x10"),
        run_2d_transpose(512, 512, "for2d_large512"),
    };
}

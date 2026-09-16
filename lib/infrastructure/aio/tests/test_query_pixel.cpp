// ============================================================================
// test_query_pixel.cpp - Phase B2: BITMAP/SPARSE 查询索引测试覆盖
//
// 专门测试 HissReader::query_pixel 方法, 覆盖 FULL/BITMAP/SPARSE_LIST 三种
// occupancy 模式下的像素查询行为, 包括首/中/末像素命中、无效像素、越界、
// 跨 Tile 边界等场景。
//
// 设计原则:
// - 使用生产 HissWriter/HissReader (不使用模拟函数冒充生产代码)
// - 测试 oracle (radec_to_nested_ipix) 仅用于计算预期 local_ipix,
// 不替代生产 query_pixel; query_pixel 与 read_tile 两条独立生产路径交叉验证
// - 每个断言真正验证, 禁止 ASSERT_TRUE(true, "known issue") 软通过
// - 测试失败时非零退出
//
// 编译 (从 tests/ 目录):
// g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
// -I../include -I../src \
// test_query_pixel.cpp \
// ../src/hiss_codec.cpp ../src/hiss_common.cpp \
// ../src/hiss_tile_model.cpp \
// ../src/hiss_transform.cpp \
// ../src/hiss_writer.cpp ../src/hiss_stream_writer.cpp \
// ../src/hiss_reader.cpp \
// ../src/aio_api.cpp ../src/aio_log.cpp \
// -llz4 -lzstd -lm -o test_query_pixel.exe
// ============================================================================
#include "hiss_format.h"
#include "hiss_tile_model.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <algorithm>
#include <random>

// ============================================================================
// 测试框架: 轻量级断言 + 结果收集 (参考 hiss_correctness_test.cpp)
// ============================================================================

static int g_test_total  = 0;
static int g_test_passed = 0;
static std::vector<std::string> g_failures;

#define TEST_CASE(name, id) \
    fprintf(stderr, "\n========== [TEST %02d] %s ==========\n", id, name); \
    g_test_total++;

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::string m = std::string("[TEST ") + std::to_string(id) + "] FAIL: " + (msg); \
        fprintf(stderr, "  FAIL: %s\n", (msg)); \
        g_failures.push_back(m); \
        return; \
    } else { \
        fprintf(stderr, "  OK: %s\n", (msg)); \
    }

#define ASSERT_NEAR(a, b, tol, msg) \
    if (std::fabs((double)(a) - (double)(b)) > (tol)) { \
        char buf[512]; \
        std::snprintf(buf, sizeof(buf), "%s (got=%.6g expected=%.6g tol=%.6g)", \
                      (msg), (double)(a), (double)(b), (double)(tol)); \
        std::string m = std::string("[TEST ") + std::to_string(id) + "] FAIL: " + buf; \
        fprintf(stderr, "  FAIL: %s\n", buf); \
        g_failures.push_back(m); \
        return; \
    } else { \
        fprintf(stderr, "  OK: %s (got=%.6g)\n", (msg), (double)(a)); \
    }

// ============================================================================
// 测试 oracle: ra/dec → NESTED ipix
// 复制自 hiss_reader.cpp 内部 static 实现, 仅用于计算预期 local_ipix。
// 生产 query_pixel 内部使用相同算法, 此处作为独立 oracle 交叉验证。
// ============================================================================

static const double kPi        = 3.14159265358979323846;
static const double kTwoPi     = 2.0 * kPi;
static const double kHalfPi    = kPi / 2.0;
static const double kTwoThirds = 2.0 / 3.0;

static inline double oracle_sq(double d) { return d * d; }
static inline int oracle_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// theta/phi → (bighp, x, y) [HEALPix 面内坐标]
static void oracle_ang2xy(double theta, double phi, int nside,
                          int* bighp, int* x, int* y) {
    double z = std::cos(theta);
    int Ns = nside;
    phi = phi - kTwoPi * std::floor(phi / kTwoPi);
    if (phi < 0.0) phi += kTwoPi;
    if (phi >= kTwoPi) phi -= kTwoPi;
    double phi_t = std::fmod(phi, kHalfPi);

    if (z >= kTwoThirds || z <= -kTwoThirds) {
        bool north = (z >= kTwoThirds);
        double zfactor = north ? 1.0 : -1.0;
        double root1 = (1.0 - z * zfactor) * 3.0 *
                       oracle_sq((double)Ns * (2.0 * phi_t - kPi) / kPi);
        double kx = (root1 <= 0.0) ? 0.0 : std::sqrt(root1);
        double root2 = (1.0 - z * zfactor) * 3.0 *
                       oracle_sq((double)Ns * 2.0 * phi_t / kPi);
        double ky = (root2 <= 0.0) ? 0.0 : std::sqrt(root2);
        double xx, yy;
        if (north) { xx = Ns - kx; yy = Ns - ky; }
        else       { xx = ky;      yy = kx; }
        *x = oracle_clampi((int)std::floor(xx), 0, Ns - 1);
        *y = oracle_clampi((int)std::floor(yy), 0, Ns - 1);
        double sector = (phi - phi_t) / kHalfPi;
        int offset = (int)std::round(sector);
        offset = ((offset % 4) + 4) % 4;
        *bighp = north ? offset : (8 + offset);
    } else {
        double zunits  = (z + kTwoThirds) / (4.0 / 3.0);
        double phiunits = phi_t / kHalfPi;
        double u1 = zunits + phiunits;
        double u2 = zunits - phiunits + 1.0;
        double xx = u1 * Ns;
        double yy = u2 * Ns;
        double sector = (phi - phi_t) / kHalfPi;
        int offset = (int)std::round(sector);
        offset = ((offset % 4) + 4) % 4;
        if (xx >= Ns) {
            xx -= Ns;
            if (yy >= Ns) {
                yy -= Ns;
                *bighp = offset;
            } else {
                *bighp = ((offset + 1) % 4) + 4;
            }
        } else {
            if (yy >= Ns) {
                yy -= Ns;
                *bighp = offset + 4;
            } else {
                *bighp = 8 + offset;
            }
        }
        *x = oracle_clampi((int)std::floor(xx), 0, Ns - 1);
        *y = oracle_clampi((int)std::floor(yy), 0, Ns - 1);
    }
}

static uint64_t oracle_xy2nest(int bighp, int x, int y, int nside) {
    uint64_t index = 0;
    int xb = x, yb = y;
    for (int i = 0; i < 32; i++) {
        index |= ((uint64_t)(((yb & 1) << 1) | (xb & 1))) << (i * 2);
        xb >>= 1;
        yb >>= 1;
        if (!xb && !yb) break;
    }
    return index + (uint64_t)bighp * (uint64_t)nside * (uint64_t)nside;
}

// ra/dec (度, ICRS) → NESTED ipix (测试 oracle, 与 reader 内部算法一致)
static uint64_t oracle_radec_to_nested_ipix(double ra_deg, double dec_deg, int nside) {
    double theta = kHalfPi - dec_deg * kPi / 180.0;
    double phi   = ra_deg * kPi / 180.0;
    int bighp, x, y;
    oracle_ang2xy(theta, phi, nside, &bighp, &x, &y);
    return oracle_xy2nest(bighp, x, y, nside);
}

// ============================================================================
// 辅助: 像素位置结构
// ============================================================================

struct PixelLoc {
    uint32_t local_ipix;
    double ra;
    double dec;
};

// ============================================================================
// 辅助: 遍历 ra/dec 网格, 找到落在目标 Tile 内的所有 local_ipix 的 ra/dec
// 两阶段: 粗扫描(2°)定位 Tile 中心 → 精细遍历(0.1°)收集所有 local_ipix
// ============================================================================

static std::vector<PixelLoc> find_tile_pixels(uint32_t nside, uint32_t tile_nside,
                                               uint64_t parent_ipix,
                                               double fine_step_deg = 0.1) {
    std::vector<PixelLoc> result;
    std::set<uint32_t> found;

    uint32_t depth = hiss::compute_tile_depth(nside);
    int shift = 2 * (int)depth;
    uint64_t local_mask = (shift >= 64) ? ~0ULL : ((1ULL << shift) - 1);
    uint32_t n_leaf_expected = (shift >= 32) ? 0 : (1u << shift);

    // 阶段1: 粗扫描 (2° 步长) 找到 Tile 内的一个点
    double center_ra = -1.0, center_dec = -1.0;
    for (double dec = -88.0; dec <= 88.0 && center_ra < 0.0; dec += 2.0) {
        for (double ra = 0.0; ra < 360.0; ra += 2.0) {
            uint64_t gip = oracle_radec_to_nested_ipix(ra, dec, (int)nside);
            if ((gip >> shift) == parent_ipix) {
                center_ra = ra;
                center_dec = dec;
                break;
            }
        }
    }
    if (center_ra < 0.0) {
        fprintf(stderr, "[find_tile_pixels] 未找到 parent_ipix=%llu 的区域\n",
                (unsigned long long)parent_ipix);
        return result;
    }

    // 阶段2: 以中心点为基础, ±12° 范围精细遍历
    double range = 12.0;
    for (double dec = center_dec - range; dec <= center_dec + range; dec += fine_step_deg) {
        if (dec < -90.0 || dec > 90.0) continue;
        for (double ra = center_ra - range; ra <= center_ra + range; ra += fine_step_deg) {
            double ra_mod = ra;
            while (ra_mod < 0.0) ra_mod += 360.0;
            while (ra_mod >= 360.0) ra_mod -= 360.0;
            uint64_t gip = oracle_radec_to_nested_ipix(ra_mod, dec, (int)nside);
            if ((gip >> shift) == parent_ipix) {
                uint32_t lip = (uint32_t)(gip & local_mask);
                if (found.insert(lip).second) {
                    result.push_back({lip, ra_mod, dec});
                    if (n_leaf_expected > 0 && result.size() == n_leaf_expected) {
                        return result;  // 全部找到
                    }
                }
            }
        }
    }
    return result;
}

// 按 local_ipix 查找对应的 ra/dec
static PixelLoc find_pixel_loc(const std::vector<PixelLoc>& pixels, uint32_t local_ipix) {
    for (const auto& p : pixels) {
        if (p.local_ipix == local_ipix) return p;
    }
    return {0xFFFFFFFFu, -1.0, -1.0};  // 未找到哨兵
}

// ============================================================================
// 辅助: 构造网格和元数据
// ============================================================================

static void make_grid_meta(uint32_t nside, hiss::HissGridSpec& grid, hiss::HissMetadata& meta) {
    uint32_t tile_nside = hiss::compute_tile_nside(nside);
    grid.nside = nside;
    grid.tile_nside = tile_nside;
    grid.ordering = 1;  // NESTED
    grid.radesys = 0;   // ICRS
    grid.pixfrac = 1.0;
    meta.nside = nside;
    meta.tile_nside = tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");
}

// 计算 HEALPix 像素面积 (球面度)
static double pixel_area(uint32_t nside) {
    return 4.0 * 3.14159265358979 / (12.0 * (double)nside * (double)nside);
}

// ============================================================================
// 测试上下文: 预构造的 HISS 文件 + 像素位置映射, 供多个测试共用
// ============================================================================

struct TestContext {
    // FULL 模式 (NSIDE=64, n_leaf=16)
    std::string full_path;
    std::vector<PixelLoc> full_pixels;
    uint64_t full_parent = 0;
    uint32_t full_nside = 64;
    uint32_t full_n_leaf = 16;

    // BITMAP 模式 (NSIDE=64, n_leaf=16, 5 个有效像素)
    std::string bitmap_path;
    std::vector<PixelLoc> bitmap_pixels;
    uint64_t bitmap_parent = 0;
    uint32_t bitmap_nside = 64;
    uint32_t bitmap_n_leaf = 16;
    std::vector<uint32_t> bitmap_valid;  // 有效像素索引

    // SPARSE_LIST 模式 (NSIDE=256, n_leaf=256, 5 个有效像素)
    // 需 SPARSE_LIST(20B) < BITMAP(32B) 才能自动选 SPARSE_LIST
    std::string sparse_path;
    std::vector<PixelLoc> sparse_pixels;
    uint64_t sparse_parent = 0;
    uint32_t sparse_nside = 256;
    uint32_t sparse_n_leaf = 256;
    std::vector<uint32_t> sparse_valid;  // 有效像素索引 (sparse_list)

    // FULL 模式 (n_leaf=8, 用于越界测试)
    std::string short_path;
    uint64_t short_parent = 0;
    uint32_t short_nside = 64;
    uint32_t short_n_leaf = 8;  // 故意小于 n_leaf_per_tile=16

    // 两个相邻 Tile (用于跨 Tile 边界测试, NSIDE=64)
    std::string multi_path;
    uint64_t multi_parent1 = 0;
    uint64_t multi_parent2 = 0;
    uint32_t multi_nside = 64;
};

// ============================================================================
// Setup: 构造所有测试 HISS 文件
// ============================================================================

static bool setup(TestContext& ctx) {
    using namespace hiss;

    // ---- FULL 模式 (NSIDE=64, n_leaf=16) ----
    {
        ctx.full_path = "tqp_full.hiss";
        ctx.full_parent = 0;
        HissGridSpec grid; HissMetadata meta;
        make_grid_meta(ctx.full_nside, grid, meta);
        double A_p = pixel_area(ctx.full_nside);

        DrizzleTileAccumulator acc;
        acc.tile_nside = grid.tile_nside;
        acc.parent_ipix = ctx.full_parent;
        acc.pixel_area = A_p;
        acc.pixels.resize(ctx.full_n_leaf);
        for (uint32_t i = 0; i < ctx.full_n_leaf; i++) {
            acc.pixels[i].sum_flux = (double)i * 10.0 + 100.0;
            acc.pixels[i].sum_area = A_p;  // support = 255
            acc.pixels[i].n_contrib = 1;
        }

        HissWriter w;
        if (w.open(ctx.full_path, grid, meta) != 0) { fprintf(stderr, "setup: full open 失败\n"); return false; }
        if (w.add_tile(ctx.full_parent, acc, nullptr, OccupancyMode::FULL) != 0) { fprintf(stderr, "setup: full add_tile 失败\n"); return false; }
        if (w.finalize() != 0) { fprintf(stderr, "setup: full finalize 失败\n"); return false; }

        ctx.full_pixels = find_tile_pixels(ctx.full_nside, grid.tile_nside, ctx.full_parent);
        fprintf(stderr, "[setup] FULL: 找到 %zu / %u 个 local_ipix\n",
                ctx.full_pixels.size(), ctx.full_n_leaf);
    }

    // ---- BITMAP 模式 (NSIDE=64, n_leaf=16, 5 个有效像素) ----
    {
        ctx.bitmap_path = "tqp_bitmap.hiss";
        ctx.bitmap_parent = 0;
        ctx.bitmap_valid = {0, 4, 8, 12, 15};  // 5/16 = 31.25% >= 0.1 → BITMAP
        HissGridSpec grid; HissMetadata meta;
        make_grid_meta(ctx.bitmap_nside, grid, meta);
        double A_p = pixel_area(ctx.bitmap_nside);

        DrizzleTileAccumulator acc;
        acc.tile_nside = grid.tile_nside;
        acc.parent_ipix = ctx.bitmap_parent;
        acc.pixel_area = A_p;
        acc.pixels.resize(ctx.bitmap_n_leaf);
        for (uint32_t i = 0; i < ctx.bitmap_n_leaf; i++) {
            acc.pixels[i].sum_flux = 0.0;
            acc.pixels[i].sum_area = 0.0;
            acc.pixels[i].n_contrib = 0;
        }
        for (uint32_t idx : ctx.bitmap_valid) {
            acc.pixels[idx].sum_flux = (double)idx * 10.0 + 100.0;
            acc.pixels[idx].sum_area = A_p;  // support = 255
            acc.pixels[idx].n_contrib = 1;
        }

        HissWriter w;
        if (w.open(ctx.bitmap_path, grid, meta) != 0) return false;
        if (w.add_tile(ctx.bitmap_parent, acc, nullptr, OccupancyMode::BITMAP) != 0) return false;
        if (w.finalize() != 0) return false;

        ctx.bitmap_pixels = find_tile_pixels(ctx.bitmap_nside, grid.tile_nside, ctx.bitmap_parent);
        fprintf(stderr, "[setup] BITMAP: 找到 %zu / %u 个 local_ipix\n",
                ctx.bitmap_pixels.size(), ctx.bitmap_n_leaf);
    }

    // ---- SPARSE_LIST 模式 (NSIDE=128, n_leaf=64, 5 个有效像素) ----
    {
        ctx.sparse_path = "tqp_sparse.hiss";
        ctx.sparse_parent = 0;
        // Writer 按编码大小自动选择 (忽略传入的 occ_mode)
        // NSIDE=256 → n_leaf=256, BITMAP=32B, SPARSE_LIST(5点)=20B → 自动选 SPARSE_LIST
        ctx.sparse_valid = {0, 32, 63, 128, 200};  // 5/256, sparse=20 < bitmap=32
        HissGridSpec grid; HissMetadata meta;
        make_grid_meta(ctx.sparse_nside, grid, meta);
        double A_p = pixel_area(ctx.sparse_nside);

        DrizzleTileAccumulator acc;
        acc.tile_nside = grid.tile_nside;
        acc.parent_ipix = ctx.sparse_parent;
        acc.pixel_area = A_p;
        acc.pixels.resize(ctx.sparse_n_leaf);
        for (uint32_t i = 0; i < ctx.sparse_n_leaf; i++) {
            acc.pixels[i].sum_flux = 0.0;
            acc.pixels[i].sum_area = 0.0;
            acc.pixels[i].n_contrib = 0;
        }
        for (uint32_t idx : ctx.sparse_valid) {
            acc.pixels[idx].sum_flux = (double)idx * 10.0 + 100.0;
            acc.pixels[idx].sum_area = A_p;  // support = 255
            acc.pixels[idx].n_contrib = 1;
        }

        HissWriter w;
        if (w.open(ctx.sparse_path, grid, meta) != 0) return false;
        // Writer 忽略 occ_mode, 按编码大小自动选择
        if (w.add_tile(ctx.sparse_parent, acc, nullptr, OccupancyMode::SPARSE_LIST) != 0) return false;
        if (w.finalize() != 0) return false;

        ctx.sparse_pixels = find_tile_pixels(ctx.sparse_nside, grid.tile_nside, ctx.sparse_parent, 0.05);
        fprintf(stderr, "[setup] SPARSE: 找到 %zu / %u 个 local_ipix\n",
                ctx.sparse_pixels.size(), ctx.sparse_n_leaf);
    }

    // ---- FULL 模式 (n_leaf=8, 用于越界测试) ----
    {
        ctx.short_path = "tqp_short.hiss";
        ctx.short_parent = 0;
        HissGridSpec grid; HissMetadata meta;
        make_grid_meta(ctx.short_nside, grid, meta);
        double A_p = pixel_area(ctx.short_nside);

        DrizzleTileAccumulator acc;
        acc.tile_nside = grid.tile_nside;
        acc.parent_ipix = ctx.short_parent;
        acc.pixel_area = A_p;
        acc.pixels.resize(ctx.short_n_leaf);  // 8 个像素, < n_leaf_per_tile=16
        for (uint32_t i = 0; i < ctx.short_n_leaf; i++) {
            acc.pixels[i].sum_flux = (double)i * 10.0 + 100.0;
            acc.pixels[i].sum_area = A_p;
            acc.pixels[i].n_contrib = 1;
        }

        HissWriter w;
        if (w.open(ctx.short_path, grid, meta) != 0) return false;
        if (w.add_tile(ctx.short_parent, acc, nullptr, OccupancyMode::FULL) != 0) return false;
        if (w.finalize() != 0) return false;
    }

    // ---- 两个 Tile (跨 Tile 边界测试, NSIDE=64) ----
    {
        ctx.multi_path = "tqp_multi.hiss";
        ctx.multi_parent1 = 0;
        ctx.multi_parent2 = 1;
        HissGridSpec grid; HissMetadata meta;
        make_grid_meta(ctx.multi_nside, grid, meta);
        double A_p = pixel_area(ctx.multi_nside);

        HissWriter w;
        if (w.open(ctx.multi_path, grid, meta) != 0) return false;

        // Tile 1: parent_ipix=0, signal = i * 10 + 1000
        {
            DrizzleTileAccumulator acc;
            acc.tile_nside = grid.tile_nside;
            acc.parent_ipix = ctx.multi_parent1;
            acc.pixel_area = A_p;
            acc.pixels.resize(16);
            for (uint32_t i = 0; i < 16; i++) {
                acc.pixels[i].sum_flux = (double)i * 10.0 + 1000.0;
                acc.pixels[i].sum_area = A_p;
                acc.pixels[i].n_contrib = 1;
            }
            if (w.add_tile(ctx.multi_parent1, acc, nullptr, OccupancyMode::FULL) != 0) return false;
        }
        // Tile 2: parent_ipix=1, signal = i * 10 + 2000
        {
            DrizzleTileAccumulator acc;
            acc.tile_nside = grid.tile_nside;
            acc.parent_ipix = ctx.multi_parent2;
            acc.pixel_area = A_p;
            acc.pixels.resize(16);
            for (uint32_t i = 0; i < 16; i++) {
                acc.pixels[i].sum_flux = (double)i * 10.0 + 2000.0;
                acc.pixels[i].sum_area = A_p;
                acc.pixels[i].n_contrib = 1;
            }
            if (w.add_tile(ctx.multi_parent2, acc, nullptr, OccupancyMode::FULL) != 0) return false;
        }
        if (w.finalize() != 0) return false;
    }

    return true;
}

// ============================================================================
// Teardown: 清理测试文件
// ============================================================================

static void teardown(TestContext& ctx) {
    std::filesystem::remove(ctx.full_path);
    std::filesystem::remove(ctx.bitmap_path);
    std::filesystem::remove(ctx.sparse_path);
    std::filesystem::remove(ctx.short_path);
    std::filesystem::remove(ctx.multi_path);
}

// ============================================================================
// FULL 模式测试 (1-4)
// ============================================================================

// 测试 01: FULL 模式 - 命中首个有效像素 (local_ipix=0)
static void test_01_full_first_pixel(int id, const TestContext& ctx) {
    TEST_CASE("FULL 模式 - 命中首个有效像素 (local_ipix=0)", id);
    ASSERT_TRUE(!ctx.full_pixels.empty(), "find_tile_pixels 找到 FULL Tile 像素");

    PixelLoc loc = find_pixel_loc(ctx.full_pixels, 0);
    ASSERT_TRUE(loc.local_ipix == 0, "找到 local_ipix=0 的 ra/dec");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.full_path) == 0, "Reader.open FULL 文件");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal[0] = 0 * 10 + 100 = 100, support = 255
    ASSERT_NEAR(sig, 100.0f, 1e-3, "signal[0] = 100.0");
    ASSERT_TRUE(sup == 255, "support[0] = 255");

    // 交叉验证: read_tile 读取完整数组
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.full_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_TRUE(sig_arr.size() == ctx.full_n_leaf, "read_tile 数组长度 = 16");
    ASSERT_NEAR(sig_arr[0], 100.0f, 1e-3, "read_tile signal[0] = 100.0");
    ASSERT_NEAR(sig, sig_arr[0], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 02: FULL 模式 - 命中中间有效像素 (local_ipix=8)
static void test_02_full_middle_pixel(int id, const TestContext& ctx) {
    TEST_CASE("FULL 模式 - 命中中间有效像素 (local_ipix=8)", id);
    ASSERT_TRUE(!ctx.full_pixels.empty(), "find_tile_pixels 找到 FULL Tile 像素");

    PixelLoc loc = find_pixel_loc(ctx.full_pixels, 8);
    ASSERT_TRUE(loc.local_ipix == 8, "找到 local_ipix=8 的 ra/dec");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.full_path) == 0, "Reader.open FULL 文件");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal[8] = 8 * 10 + 100 = 180, support = 255
    ASSERT_NEAR(sig, 180.0f, 1e-3, "signal[8] = 180.0");
    ASSERT_TRUE(sup == 255, "support[8] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.full_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[8], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 03: FULL 模式 - 命中末个有效像素 (local_ipix=15)
static void test_03_full_last_pixel(int id, const TestContext& ctx) {
    TEST_CASE("FULL 模式 - 命中末个有效像素 (local_ipix=15)", id);
    ASSERT_TRUE(!ctx.full_pixels.empty(), "find_tile_pixels 找到 FULL Tile 像素");

    PixelLoc loc = find_pixel_loc(ctx.full_pixels, 15);
    ASSERT_TRUE(loc.local_ipix == 15, "找到 local_ipix=15 的 ra/dec");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.full_path) == 0, "Reader.open FULL 文件");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal[15] = 15 * 10 + 100 = 250, support = 255
    ASSERT_NEAR(sig, 250.0f, 1e-3, "signal[15] = 250.0");
    ASSERT_TRUE(sup == 255, "support[15] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.full_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[15], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 04: FULL 模式 - 越界 local_ipix (数组长度 < n_leaf_per_tile)
// 构造 n_leaf=8 的 Tile (n_leaf_per_tile=16), query_pixel local_ipix ∈ [8,15] 应返回零值
static void test_04_full_out_of_range(int id, const TestContext& ctx) {
    TEST_CASE("FULL 模式 - 越界 local_ipix 返回零值不报错", id);
    ASSERT_TRUE(!ctx.full_pixels.empty(), "需要 FULL Tile 像素映射");

    // short_path 的 Tile 只有 8 个像素, local_ipix >= 8 应返回零值
    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.short_path) == 0, "Reader.open short 文件");

    // 验证 read_tile 返回的数组长度 = 8 (不是 16)
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.short_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_TRUE(sig_arr.size() == ctx.short_n_leaf, "read_tile 数组长度 = 8 (短数组)");

    // 用 full_pixels 找到 local_ipix=10 的 ra/dec (在 short Tile 内但 >= 8)
    PixelLoc loc = find_pixel_loc(ctx.full_pixels, 10);
    ASSERT_TRUE(loc.local_ipix == 10, "找到 local_ipix=10 的 ra/dec");

    float sig = -999.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 越界返回 0 (不报错)");
    ASSERT_NEAR(sig, 0.0f, 1e-6, "越界 local_ipix signal = 0.0");
    ASSERT_TRUE(sup == 0, "越界 local_ipix support = 0");

    // 对照: local_ipix=5 (在 [0,7] 范围内) 应返回非零值
    PixelLoc loc5 = find_pixel_loc(ctx.full_pixels, 5);
    ASSERT_TRUE(loc5.local_ipix == 5, "找到 local_ipix=5 的 ra/dec");
    float sig5 = -999.0f; uint8_t sup5 = 255;
    ret = r.query_pixel(loc5.ra, loc5.dec, &sig5, &sup5);
    ASSERT_TRUE(ret == 0, "query_pixel local_ipix=5 返回 0");
    ASSERT_NEAR(sig5, 150.0f, 1e-3, "local_ipix=5 signal = 150.0 (5*10+100)");

    r.close();
}

// ============================================================================
// BITMAP 模式测试 (5-8)
// ============================================================================

// 测试 05: BITMAP 模式 - 命中首个有效像素 (位图第0位=1, local_ipix=0)
static void test_05_bitmap_first_pixel(int id, const TestContext& ctx) {
    TEST_CASE("BITMAP 模式 - 命中首个有效像素 (local_ipix=0)", id);
    ASSERT_TRUE(!ctx.bitmap_pixels.empty(), "find_tile_pixels 找到 BITMAP Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.bitmap_path) == 0, "Reader.open BITMAP 文件");

    // 验证 occ_mode = BITMAP
    ASSERT_TRUE(r.tiles().size() == 1, "Tile 数 = 1");
    ASSERT_TRUE(r.tiles()[0].occ_mode == hiss::OccupancyMode::BITMAP, "occ_mode = BITMAP");

    PixelLoc loc = find_pixel_loc(ctx.bitmap_pixels, 0);
    ASSERT_TRUE(loc.local_ipix == 0, "找到 local_ipix=0 的 ra/dec");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal = 0*10+100 = 100, support = 255
    ASSERT_NEAR(sig, 100.0f, 1e-3, "BITMAP signal[0] = 100.0");
    ASSERT_TRUE(sup == 255, "BITMAP support[0] = 255");

    // 交叉验证: read_tile
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.bitmap_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[0], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 06: BITMAP 模式 - 命中中间有效像素 (local_ipix=8)
static void test_06_bitmap_middle_pixel(int id, const TestContext& ctx) {
    TEST_CASE("BITMAP 模式 - 命中中间有效像素 (local_ipix=8)", id);
    ASSERT_TRUE(!ctx.bitmap_pixels.empty(), "find_tile_pixels 找到 BITMAP Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.bitmap_path) == 0, "Reader.open BITMAP 文件");

    PixelLoc loc = find_pixel_loc(ctx.bitmap_pixels, 8);
    ASSERT_TRUE(loc.local_ipix == 8, "找到 local_ipix=8 的 ra/dec");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal = 8*10+100 = 180, support = 255
    ASSERT_NEAR(sig, 180.0f, 1e-3, "BITMAP signal[8] = 180.0");
    ASSERT_TRUE(sup == 255, "BITMAP support[8] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.bitmap_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[8], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 07: BITMAP 模式 - 命中末个有效像素 (local_ipix=15)
static void test_07_bitmap_last_pixel(int id, const TestContext& ctx) {
    TEST_CASE("BITMAP 模式 - 命中末个有效像素 (local_ipix=15)", id);
    ASSERT_TRUE(!ctx.bitmap_pixels.empty(), "find_tile_pixels 找到 BITMAP Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.bitmap_path) == 0, "Reader.open BITMAP 文件");

    PixelLoc loc = find_pixel_loc(ctx.bitmap_pixels, 15);
    ASSERT_TRUE(loc.local_ipix == 15, "找到 local_ipix=15 的 ra/dec");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal = 15*10+100 = 250, support = 255
    ASSERT_NEAR(sig, 250.0f, 1e-3, "BITMAP signal[15] = 250.0");
    ASSERT_TRUE(sup == 255, "BITMAP support[15] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.bitmap_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[15], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 08: BITMAP 模式 - 命中无效像素 (位图为0) → signal=0, support=0
static void test_08_bitmap_invalid_pixel(int id, const TestContext& ctx) {
    TEST_CASE("BITMAP 模式 - 命中无效像素返回零值", id);
    ASSERT_TRUE(!ctx.bitmap_pixels.empty(), "find_tile_pixels 找到 BITMAP Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.bitmap_path) == 0, "Reader.open BITMAP 文件");

    // local_ipix=5 不在有效列表 {0,4,8,12,15} 中
    PixelLoc loc = find_pixel_loc(ctx.bitmap_pixels, 5);
    ASSERT_TRUE(loc.local_ipix == 5, "找到 local_ipix=5 的 ra/dec");

    float sig = -999.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 无效像素返回 0 (不报错)");
    ASSERT_NEAR(sig, 0.0f, 1e-6, "无效像素 signal = 0.0");
    ASSERT_TRUE(sup == 0, "无效像素 support = 0");

    // 交叉验证: read_tile 展开后 [5] 也应为 0
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.bitmap_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR((float)sig_arr[5], 0.0f, 1e-6, "read_tile signal[5] = 0 (展开后)");
    ASSERT_TRUE(sup_arr[5] == 0, "read_tile support[5] = 0 (展开后)");

    r.close();
}

// ============================================================================
// SPARSE_LIST 模式测试 (9-12)
// ============================================================================

// 测试 09: SPARSE_LIST 模式 - 命中首个索引 (sparse_list[0] = 0)
static void test_09_sparse_first_index(int id, const TestContext& ctx) {
    TEST_CASE("SPARSE_LIST 模式 - 命中首个索引 (local_ipix=0)", id);
    ASSERT_TRUE(!ctx.sparse_pixels.empty(), "find_tile_pixels 找到 SPARSE Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.sparse_path) == 0, "Reader.open SPARSE 文件");

    // 验证 occ_mode = SPARSE_LIST
    ASSERT_TRUE(r.tiles().size() == 1, "Tile 数 = 1");
    ASSERT_TRUE(r.tiles()[0].occ_mode == hiss::OccupancyMode::SPARSE_LIST, "occ_mode = SPARSE_LIST");

    PixelLoc loc = find_pixel_loc(ctx.sparse_pixels, 0);
    ASSERT_TRUE(loc.local_ipix == 0, "找到 local_ipix=0 的 ra/dec");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal = 0*10+100 = 100, support = 255
    ASSERT_NEAR(sig, 100.0f, 1e-3, "SPARSE signal[0] = 100.0");
    ASSERT_TRUE(sup == 255, "SPARSE support[0] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.sparse_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[0], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 10: SPARSE_LIST 模式 - 命中中间索引 (sparse_list[2] = 32)
static void test_10_sparse_middle_index(int id, const TestContext& ctx) {
    TEST_CASE("SPARSE_LIST 模式 - 命中中间索引 (local_ipix=32)", id);
    ASSERT_TRUE(!ctx.sparse_pixels.empty(), "find_tile_pixels 找到 SPARSE Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.sparse_path) == 0, "Reader.open SPARSE 文件");

    PixelLoc loc = find_pixel_loc(ctx.sparse_pixels, 32);
    ASSERT_TRUE(loc.local_ipix == 32, "找到 local_ipix=32 的 ra/dec");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal = 32*10+100 = 420, support = 255
    ASSERT_NEAR(sig, 420.0f, 1e-3, "SPARSE signal[32] = 420.0");
    ASSERT_TRUE(sup == 255, "SPARSE support[32] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.sparse_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[32], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 11: SPARSE_LIST 模式 - 命中 sparse_list 中的 local_ipix=63
static void test_11_sparse_last_index(int id, const TestContext& ctx) {
    TEST_CASE("SPARSE_LIST 模式 - 命中 local_ipix=63", id);
    ASSERT_TRUE(!ctx.sparse_pixels.empty(), "find_tile_pixels 找到 SPARSE Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.sparse_path) == 0, "Reader.open SPARSE 文件");

    PixelLoc loc = find_pixel_loc(ctx.sparse_pixels, 63);
    ASSERT_TRUE(loc.local_ipix == 63, "找到 local_ipix=63 的 ra/dec");

    float sig = -1.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 预期: signal = 63*10+100 = 730, support = 255
    ASSERT_NEAR(sig, 730.0f, 1e-3, "SPARSE signal[63] = 730.0");
    ASSERT_TRUE(sup == 255, "SPARSE support[63] = 255");

    // 交叉验证
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.sparse_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR(sig, sig_arr[63], 1e-4, "query_pixel 与 read_tile 一致");

    r.close();
}

// 测试 12: SPARSE_LIST 模式 - 二分未命中 (缺失像素) → 返回零值不报错
static void test_12_sparse_miss(int id, const TestContext& ctx) {
    TEST_CASE("SPARSE_LIST 模式 - 二分未命中返回零值", id);
    ASSERT_TRUE(!ctx.sparse_pixels.empty(), "find_tile_pixels 找到 SPARSE Tile 像素");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(ctx.sparse_path) == 0, "Reader.open SPARSE 文件");

    // local_ipix=5 不在 sparse_list {0,32,63,128,200} 中
    PixelLoc loc = find_pixel_loc(ctx.sparse_pixels, 5);
    ASSERT_TRUE(loc.local_ipix == 5, "找到 local_ipix=5 的 ra/dec");

    float sig = -999.0f; uint8_t sup = 255;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 未命中返回 0 (不报错)");
    ASSERT_NEAR(sig, 0.0f, 1e-6, "未命中 signal = 0.0");
    ASSERT_TRUE(sup == 0, "未命中 support = 0");

    // 再测一个: local_ipix=40 (在 32 和 63 之间, 不在列表中)
    PixelLoc loc40 = find_pixel_loc(ctx.sparse_pixels, 40);
    if (loc40.local_ipix == 40) {
        float sig40 = -999.0f; uint8_t sup40 = 255;
        ret = r.query_pixel(loc40.ra, loc40.dec, &sig40, &sup40);
        ASSERT_TRUE(ret == 0, "query_pixel local_ipix=40 未命中返回 0");
        ASSERT_NEAR(sig40, 0.0f, 1e-6, "未命中 signal[40] = 0.0");
        ASSERT_TRUE(sup40 == 0, "未命中 support[40] = 0");
    }

    // 交叉验证: read_tile 展开后 [5] 也应为 0
    std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
    ASSERT_TRUE(r.read_tile(ctx.sparse_parent, sig_arr, sup_arr) == 0, "read_tile 成功");
    ASSERT_NEAR((float)sig_arr[5], 0.0f, 1e-6, "read_tile signal[5] = 0 (展开后)");

    r.close();
}

// ============================================================================
// 特殊情况测试 (13-15)
// ============================================================================

// 测试 13: signal=0 但像素存在 (sum_flux=0 且 sum_area>0)
// 验证 query_pixel 返回 signal=0.0f, support>0
static void test_13_zero_signal_valid_pixel(int id, const TestContext& ctx) {
    TEST_CASE("signal=0 但像素存在 (support>0)", id);
    using namespace hiss;

    const char* path = "tqp_zerosig.hiss";
    uint32_t nside = 64;
    uint64_t parent_ipix = 0;
    HissGridSpec grid; HissMetadata meta;
    make_grid_meta(nside, grid, meta);
    double A_p = pixel_area(nside);

    // 构造 Tile: 像素 0 的 sum_flux=0, sum_area=A_p (有效但 signal=0)
    // 其他像素 sum_flux>0
    DrizzleTileAccumulator acc;
    acc.tile_nside = grid.tile_nside;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area = A_p;
    acc.pixels.resize(16);
    for (uint32_t i = 0; i < 16; i++) {
        acc.pixels[i].sum_flux = (i == 0) ? 0.0 : (double)i * 10.0 + 100.0;
        acc.pixels[i].sum_area = A_p;  // 所有像素 support=255
        acc.pixels[i].n_contrib = 1;
    }

    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "Writer.open");
    ASSERT_TRUE(w.add_tile(parent_ipix, acc, nullptr, OccupancyMode::FULL) == 0, "add_tile");
    ASSERT_TRUE(w.finalize() == 0, "finalize");

    // 用 ctx.full_pixels 找到 local_ipix=0 的 ra/dec (同一个 parent_ipix=0)
    ASSERT_TRUE(!ctx.full_pixels.empty(), "需要像素位置映射");
    PixelLoc loc = find_pixel_loc(ctx.full_pixels, 0);
    ASSERT_TRUE(loc.local_ipix == 0, "找到 local_ipix=0 的 ra/dec");

    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "Reader.open");

    float sig = -999.0f; uint8_t sup = 0;
    int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 返回 0");

    // 关键验证: signal=0 但 support>0 (像素存在)
    ASSERT_NEAR(sig, 0.0f, 1e-6, "signal=0.0 (sum_flux=0)");
    ASSERT_TRUE(sup > 0, "support>0 (像素存在, sum_area>0)");
    ASSERT_TRUE(sup == 255, "support=255 (sum_area=A_p)");

    // 对照: local_ipix=5 应有 signal>0
    PixelLoc loc5 = find_pixel_loc(ctx.full_pixels, 5);
    ASSERT_TRUE(loc5.local_ipix == 5, "找到 local_ipix=5");
    float sig5 = -999.0f; uint8_t sup5 = 0;
    ret = r.query_pixel(loc5.ra, loc5.dec, &sig5, &sup5);
    ASSERT_TRUE(ret == 0, "query_pixel local_ipix=5 返回 0");
    ASSERT_NEAR(sig5, 150.0f, 1e-3, "signal[5]=150.0 (对照非零)");

    r.close();
    std::filesystem::remove(path);
}

// 测试 14: 跨 Tile 边界 (两个相邻 Tile 的边界像素)
// 验证 query_pixel 能正确区分两个 Tile, 数据不混淆
static void test_14_cross_tile_boundary(int id, const TestContext& ctx) {
    TEST_CASE("跨 Tile 边界 (两个 Tile 数据不混淆)", id);
    using namespace hiss;

    ASSERT_TRUE(!ctx.full_pixels.empty(), "需要像素位置映射");

    HissReader r;
    ASSERT_TRUE(r.open(ctx.multi_path) == 0, "Reader.open multi 文件");
    ASSERT_TRUE(r.tiles().size() == 2, "Tile 数 = 2");

    // 查找 parent_ipix=0 和 parent_ipix=1 各自的像素位置
    // 用 find_tile_pixels 分别查找
    auto pixels1 = find_tile_pixels(ctx.multi_nside, compute_tile_nside(ctx.multi_nside),
                                     ctx.multi_parent1);
    auto pixels2 = find_tile_pixels(ctx.multi_nside, compute_tile_nside(ctx.multi_nside),
                                     ctx.multi_parent2);
    ASSERT_TRUE(!pixels1.empty(), "找到 Tile1 像素");
    ASSERT_TRUE(!pixels2.empty(), "找到 Tile2 像素");

    // 从 Tile1 取一个像素 (local_ipix=5)
    PixelLoc loc1 = find_pixel_loc(pixels1, 5);
    ASSERT_TRUE(loc1.local_ipix == 5, "Tile1 local_ipix=5 找到");

    // 从 Tile2 取一个像素 (local_ipix=5)
    PixelLoc loc2 = find_pixel_loc(pixels2, 5);
    ASSERT_TRUE(loc2.local_ipix == 5, "Tile2 local_ipix=5 找到");

    // query_pixel Tile1 的像素: signal = 5*10+1000 = 1050
    float sig1 = -1.0f; uint8_t sup1 = 0;
    int ret1 = r.query_pixel(loc1.ra, loc1.dec, &sig1, &sup1);
    ASSERT_TRUE(ret1 == 0, "query_pixel Tile1 返回 0");
    ASSERT_NEAR(sig1, 1050.0f, 1e-3, "Tile1 signal = 1050.0 (5*10+1000)");
    ASSERT_TRUE(sup1 == 255, "Tile1 support = 255");

    // query_pixel Tile2 的像素: signal = 5*10+2000 = 2050
    float sig2 = -1.0f; uint8_t sup2 = 0;
    int ret2 = r.query_pixel(loc2.ra, loc2.dec, &sig2, &sup2);
    ASSERT_TRUE(ret2 == 0, "query_pixel Tile2 返回 0");
    ASSERT_NEAR(sig2, 2050.0f, 1e-3, "Tile2 signal = 2050.0 (5*10+2000)");
    ASSERT_TRUE(sup2 == 255, "Tile2 support = 255");

    // 关键验证: 两个 Tile 的数据不混淆
    ASSERT_TRUE(std::fabs((double)sig1 - (double)sig2) > 100.0, "Tile1 与 Tile2 signal 明显不同");

    r.close();
}

// 测试 15: ra/dec 落在已写入 Tile 范围外 → 返回零值不报错
static void test_15_outside_tile(int id, const TestContext& ctx) {
    TEST_CASE("ra/dec 落在 Tile 范围外返回零值", id);
    using namespace hiss;

    ASSERT_TRUE(!ctx.full_pixels.empty(), "需要像素位置映射");

    HissReader r;
    ASSERT_TRUE(r.open(ctx.full_path) == 0, "Reader.open FULL 文件");

    // full_path 只写入 parent_ipix=0 的 Tile
    // 查找 parent_ipix=1 的 Tile 内的某个 ra/dec (不在 parent_ipix=0 内)
    uint32_t depth = compute_tile_depth(ctx.full_nside);
    int shift = 2 * (int)depth;

    // 用 oracle 找一个 parent_ipix != 0 的 ra/dec
    double outside_ra = -1.0, outside_dec = -1.0;
    for (double dec = -88.0; dec <= 88.0 && outside_ra < 0.0; dec += 2.0) {
        for (double ra = 0.0; ra < 360.0; ra += 2.0) {
            uint64_t gip = oracle_radec_to_nested_ipix(ra, dec, (int)ctx.full_nside);
            if ((gip >> shift) != ctx.full_parent) {
                outside_ra = ra;
                outside_dec = dec;
                break;
            }
        }
    }
    ASSERT_TRUE(outside_ra >= 0.0, "找到 Tile 范围外的 ra/dec");

    float sig = -999.0f; uint8_t sup = 255;
    int ret = r.query_pixel(outside_ra, outside_dec, &sig, &sup);
    ASSERT_TRUE(ret == 0, "query_pixel 范围外返回 0 (不报错)");
    ASSERT_NEAR(sig, 0.0f, 1e-6, "范围外 signal = 0.0");
    ASSERT_TRUE(sup == 0, "范围外 support = 0");

    // 对照: 范围内应返回非零值
    PixelLoc loc = find_pixel_loc(ctx.full_pixels, 5);
    ASSERT_TRUE(loc.local_ipix == 5, "找到范围内 local_ipix=5");
    float sig_in = -999.0f; uint8_t sup_in = 0;
    ret = r.query_pixel(loc.ra, loc.dec, &sig_in, &sup_in);
    ASSERT_TRUE(ret == 0, "query_pixel 范围内返回 0");
    ASSERT_NEAR(sig_in, 150.0f, 1e-3, "范围内 signal = 150.0 (对照非零)");

    r.close();
}

// 测试 16: 随机位置查询 (FULL/BITMAP/SPARSE 三种模式, 固定种子可复现)
// B21 回归: 补充随机位置覆盖, 与 read_tile 交叉验证 query_pixel 一致性
static void test_16_random_positions(int id, const TestContext& ctx) {
    TEST_CASE("随机位置查询 (FULL/BITMAP/SPARSE)", id);

    std::mt19937 rng(42);  // 固定种子, 保证可复现

    // ---- FULL 模式: 随机选 local_ipix ----
    {
        ASSERT_TRUE(!ctx.full_pixels.empty(), "FULL 像素映射非空");
        hiss::HissReader r;
        ASSERT_TRUE(r.open(ctx.full_path) == 0, "Reader.open FULL");
        std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
        ASSERT_TRUE(r.read_tile(ctx.full_parent, sig_arr, sup_arr) == 0, "FULL read_tile");
        ASSERT_TRUE(sig_arr.size() == ctx.full_n_leaf, "FULL read_tile 长度 = 16");

        int checked = 0;
        for (int trial = 0; trial < 12 && checked < 6; trial++) {
            uint32_t lip = rng() % ctx.full_n_leaf;
            PixelLoc loc = find_pixel_loc(ctx.full_pixels, lip);
            if (loc.local_ipix != lip) continue;  // 网格扫描未命中, 跳过
            float sig = -1.0f; uint8_t sup = 0;
            int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
            ASSERT_TRUE(ret == 0, "FULL 随机 query_pixel 返回 0");
            ASSERT_NEAR(sig, sig_arr[lip], 1e-4, "FULL 随机 query 与 read_tile signal 一致");
            ASSERT_TRUE(sup == sup_arr[lip], "FULL 随机 query 与 read_tile support 一致");
            checked++;
        }
        ASSERT_TRUE(checked > 0, "FULL 随机至少成功校验 1 个像素");
        r.close();
    }

    // ---- BITMAP 模式: 随机选有效 local_ipix ----
    {
        ASSERT_TRUE(!ctx.bitmap_pixels.empty(), "BITMAP 像素映射非空");
        hiss::HissReader r;
        ASSERT_TRUE(r.open(ctx.bitmap_path) == 0, "Reader.open BITMAP");
        std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
        ASSERT_TRUE(r.read_tile(ctx.bitmap_parent, sig_arr, sup_arr) == 0, "BITMAP read_tile");

        int checked = 0;
        for (int trial = 0; trial < 12 && checked < 6; trial++) {
            uint32_t idx = rng() % ctx.bitmap_valid.size();
            uint32_t lip = ctx.bitmap_valid[idx];
            PixelLoc loc = find_pixel_loc(ctx.bitmap_pixels, lip);
            if (loc.local_ipix != lip) continue;
            float sig = -1.0f; uint8_t sup = 0;
            int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
            ASSERT_TRUE(ret == 0, "BITMAP 随机 query_pixel 返回 0");
            ASSERT_NEAR(sig, sig_arr[lip], 1e-4, "BITMAP 随机 query 与 read_tile signal 一致");
            ASSERT_TRUE(sup == sup_arr[lip], "BITMAP 随机 query 与 read_tile support 一致");
            checked++;
        }
        ASSERT_TRUE(checked > 0, "BITMAP 随机至少成功校验 1 个像素");
        r.close();
    }

    // ---- SPARSE_LIST 模式: 随机选有效 local_ipix ----
    {
        ASSERT_TRUE(!ctx.sparse_pixels.empty(), "SPARSE 像素映射非空");
        hiss::HissReader r;
        ASSERT_TRUE(r.open(ctx.sparse_path) == 0, "Reader.open SPARSE");
        std::vector<float> sig_arr; std::vector<uint8_t> sup_arr;
        ASSERT_TRUE(r.read_tile(ctx.sparse_parent, sig_arr, sup_arr) == 0, "SPARSE read_tile");

        int checked = 0;
        for (int trial = 0; trial < 12 && checked < 6; trial++) {
            uint32_t idx = rng() % ctx.sparse_valid.size();
            uint32_t lip = ctx.sparse_valid[idx];
            PixelLoc loc = find_pixel_loc(ctx.sparse_pixels, lip);
            if (loc.local_ipix != lip) continue;
            float sig = -1.0f; uint8_t sup = 0;
            int ret = r.query_pixel(loc.ra, loc.dec, &sig, &sup);
            ASSERT_TRUE(ret == 0, "SPARSE 随机 query_pixel 返回 0");
            ASSERT_NEAR(sig, sig_arr[lip], 1e-4, "SPARSE 随机 query 与 read_tile signal 一致");
            ASSERT_TRUE(sup == sup_arr[lip], "SPARSE 随机 query 与 read_tile support 一致");
            checked++;
        }
        ASSERT_TRUE(checked > 0, "SPARSE 随机至少成功校验 1 个像素");
        r.close();
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    fprintf(stderr, "=== Phase B2: BITMAP/SPARSE 查询索引测试 ===\n");
    fprintf(stderr, "编译时间: %s %s\n\n", __DATE__, __TIME__);

    TestContext ctx;
    if (!setup(ctx)) {
        fprintf(stderr, "setup 失败, 终止测试\n");
        teardown(ctx);
        return 2;
    }

    // FULL 模式 (1-4)
    test_01_full_first_pixel(1, ctx);
    test_02_full_middle_pixel(2, ctx);
    test_03_full_last_pixel(3, ctx);
    test_04_full_out_of_range(4, ctx);

    // BITMAP 模式 (5-8)
    test_05_bitmap_first_pixel(5, ctx);
    test_06_bitmap_middle_pixel(6, ctx);
    test_07_bitmap_last_pixel(7, ctx);
    test_08_bitmap_invalid_pixel(8, ctx);

    // SPARSE_LIST 模式 (9-12)
    test_09_sparse_first_index(9, ctx);
    test_10_sparse_middle_index(10, ctx);
    test_11_sparse_last_index(11, ctx);
    test_12_sparse_miss(12, ctx);

    // 特殊情况 (13-15)
    test_13_zero_signal_valid_pixel(13, ctx);
    test_14_cross_tile_boundary(14, ctx);
    test_15_outside_tile(15, ctx);

    // 随机位置覆盖 (16) - B21 回归
    test_16_random_positions(16, ctx);

    teardown(ctx);

    // 汇总
    g_test_passed = g_test_total - (int)g_failures.size();
    fprintf(stderr, "\n=== 测试结果 ===\n");
    fprintf(stderr, "  总计: %d\n", g_test_total);
    fprintf(stderr, "  通过: %d\n", g_test_passed);
    fprintf(stderr, "  失败: %zu\n", g_failures.size());

    if (!g_failures.empty()) {
        fprintf(stderr, "\n--- 失败详情 ---\n");
        for (const auto& f : g_failures) {
            fprintf(stderr, "  %s\n", f.c_str());
        }
        return 1;
    }

    fprintf(stderr, "\nALL PASS\n");
    return 0;
}

// ============================================================================
// test_writer_integration.cpp - HISS Writer 核心改造集成测试 (WP-E 步骤7,8,10,11)
//
// 覆盖范围 (10 个测试用例):
//   01. signal 保存累计通量 (步骤7: signal = sumFlux, 不除面积)
//   02. FULL 模式往返读写
//   03. BITMAP 模式往返读写 (步骤11: 仅保存有效像素)
//   04. SPARSE_LIST 模式往返读写 (步骤11: 仅保存有效像素)
//   05. occupancy 自动选择验证 (步骤11: 按占用率自动选择模式)
//   06. 流式写入验证 (步骤10: .partial 清理 + 文件完整性)
//   07. SNR 子块往返读写 (冻结布局: n_points + points)
//   08. support 语义验证 (步骤2: support = sum_area / A_p)
//   09. aio_hiss_write/read C API 往返 (步骤8: 新后端兼容性)
//   10. 元数据 WCS 移除验证 (步骤8: 不含 cd/crval/crpix/sip)
//
// 编译 (从 tests/ 目录):
//   g++ -std=c++17 -O2 -fopenmp -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
//     -I../include -I../src \
//     test_writer_integration.cpp \
//     ../src/hiss_codec.cpp ../src/hiss_common.cpp \
//     ../src/hiss_tile_model.cpp \
//     ../src/hiss_writer.cpp ../src/hiss_stream_writer.cpp \
//     ../src/hiss_reader.cpp \
//     ../src/healpix/aio_healpix_io.cpp \
//     ../src/aio_api.cpp ../src/aio_log.cpp \
//     -lzstd -lm -o test_writer_integration.exe
//
// 运行:
//   ./test_writer_integration.exe
// ============================================================================
#include "hiss_format.h"
#include "hiss_tile_model.h"
#include "aio_healpix_io.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>

// ============================================================================
// 测试框架: 轻量级断言 + 结果收集
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
// 辅助: 构造测试用累加器
// ============================================================================

static hiss::DrizzleTileAccumulator make_test_acc(uint32_t tile_nside, uint64_t parent_ipix,
                                                   uint32_t n_leaf, double pixel_area,
                                                   bool full_coverage) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area = pixel_area;
    acc.pixels.resize(n_leaf);
    for (uint32_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = (double)i * 10.0;
        acc.pixels[i].sum_area = full_coverage ? pixel_area : 0.0;
        acc.pixels[i].n_contrib = full_coverage ? 1 : 0;
    }
    return acc;
}

// ============================================================================
// 测试 01: signal 保存累计通量 (步骤7)
//   验证: signal[p] = float(sumFlux), 不是 sumFlux/sumArea
// ============================================================================
static void test_01_signal_is_cumulative_flux(int id) {
    TEST_CASE("signal 保存累计通量 (步骤7)", id);

    using namespace hiss;
    DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = 0;
    acc.pixel_area = 0.5;  // 任意正面积
    acc.pixels.resize(4);

    // 设置已知通量和面积
    acc.pixels[0].sum_flux = 100.0;  acc.pixels[0].sum_area = 0.5;  // 旧错误会算成 200
    acc.pixels[1].sum_flux = 50.0;   acc.pixels[1].sum_area = 0.25; // 旧错误会算成 200
    acc.pixels[2].sum_flux = 0.0;    acc.pixels[2].sum_area = 0.0;  // 无贡献
    acc.pixels[3].sum_flux = 33.5;   acc.pixels[3].sum_area = 1.0;

    std::vector<float> signal;
    acc.finalize_signal(signal);

    // signal 应等于 sumFlux, 不除面积
    ASSERT_NEAR(signal[0], 100.0f, 1e-4, "signal[0] = sumFlux (100.0), 不除面积");
    ASSERT_NEAR(signal[1], 50.0f, 1e-4, "signal[1] = sumFlux (50.0), 不除面积");
    ASSERT_NEAR(signal[2], 0.0f, 1e-4, "signal[2] = 0 (无贡献)");
    ASSERT_NEAR(signal[3], 33.5f, 1e-4, "signal[3] = sumFlux (33.5)");
}

// ============================================================================
// 测试 02: FULL 模式往返读写
//   验证: 写入 FULL 模式 → 读取 → signal/support 一致
// ============================================================================
static void test_02_full_roundtrip(int id) {
    TEST_CASE("FULL 模式往返读写", id);
    using namespace hiss;
    const char* path = "test_full_roundtrip.hiss";

    HissGridSpec grid;
    grid.nside = 64;
    grid.tile_nside = compute_tile_nside(64);  // 16
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    HissMetadata meta;
    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    // 构造 FULL 模式累加器 (全部像素有效)
    // NSIDE=64: depth=2, tile_nside=16, n_leaf_per_tile = 4^2 = 16
    uint32_t n_leaf = 16;
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);
    DrizzleTileAccumulator acc = make_test_acc(16, 42, n_leaf, A_p, true);

    // 写入
    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "open 成功");
    ASSERT_TRUE(w.add_tile(42, acc, nullptr, OccupancyMode::FULL) == 0, "add_tile FULL 成功");
    ASSERT_TRUE(w.finalize() == 0, "finalize 成功");

    // 读取
    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "reader.open 成功");
    ASSERT_TRUE(r.tiles().size() == 1, "Tile 数 = 1");

    std::vector<float> signal;
    std::vector<uint8_t> support;
    ASSERT_TRUE(r.read_tile(42, signal, support) == 0, "read_tile 成功");

    // 验证: FULL 模式数组长度 = n_leaf_per_tile
    ASSERT_TRUE(signal.size() == n_leaf, "signal 长度 = n_leaf_per_tile (256)");
    ASSERT_TRUE(support.size() == n_leaf, "support 长度 = n_leaf_per_tile (256)");

    // 验证 signal 值
    for (uint32_t i = 0; i < n_leaf; i++) {
        double expected = (double)i * 10.0;
        if (std::fabs((double)signal[i] - expected) > 1e-3) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "signal[%u] = %.6f (期望 %.6f)", i, signal[i], expected);
            ASSERT_TRUE(false, buf);
        }
    }
    ASSERT_TRUE(true, "全部 256 个 signal 值匹配");

    // 验证 support 值 (sum_area = A_p → S = 1.0 → support = 255)
    ASSERT_TRUE(support[0] == 255, "support[0] = 255 (S=1.0)");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 03: BITMAP 模式往返读写 (步骤11: 仅保存有效像素)
//   验证: 部分有效像素 → BITMAP 模式 → 读取展开后一致
// ============================================================================
static void test_03_bitmap_roundtrip(int id) {
    TEST_CASE("BITMAP 模式往返读写 (步骤11)", id);
    using namespace hiss;
    const char* path = "test_bitmap_roundtrip.hiss";

    HissGridSpec grid;
    grid.nside = 64;
    grid.tile_nside = compute_tile_nside(64);
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    HissMetadata meta;
    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    uint32_t n_leaf = 16;  // 4^2 = 16 for NSIDE=64
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);

    // 构造 ~30% 有效像素 (BITMAP 模式, 占用率 ~0.3)
    DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = 5;
    acc.pixel_area = A_p;
    acc.pixels.resize(n_leaf);
    for (uint32_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = 0.0;
        acc.pixels[i].sum_area = 0.0;
        acc.pixels[i].n_contrib = 0;
    }
    // 5/16 = 31.25% → BITMAP (0.1 <= 0.3125 < 0.8)
    std::vector<uint32_t> valid_indices = {0, 3, 6, 9, 12};
    for (uint32_t idx : valid_indices) {
        acc.pixels[idx].sum_flux = (double)idx * 2.0;
        acc.pixels[idx].sum_area = A_p * 0.8;
        acc.pixels[idx].n_contrib = 1;
    }

    // 写入 (传入 BITMAP, Writer 会自动选择)
    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "open 成功");
    ASSERT_TRUE(w.add_tile(5, acc, nullptr, OccupancyMode::BITMAP) == 0, "add_tile BITMAP 成功");
    ASSERT_TRUE(w.finalize() == 0, "finalize 成功");

    // 读取
    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "reader.open 成功");
    ASSERT_TRUE(r.tiles().size() == 1, "Tile 数 = 1");

    // 验证 Tile 的 occ_mode
    // 占用率 ~86/256 = 0.336 → BITMAP (0.1 <= 0.336 < 0.8)
    ASSERT_TRUE(r.tiles()[0].occ_mode == OccupancyMode::BITMAP,
                "occ_mode = BITMAP (自动选择)");

    std::vector<float> signal;
    std::vector<uint8_t> support;
    ASSERT_TRUE(r.read_tile(5, signal, support) == 0, "read_tile 成功");

    // 验证: 展开后数组长度 = n_leaf_per_tile
    ASSERT_TRUE(signal.size() == n_leaf, "展开后 signal 长度 = 256");
    ASSERT_TRUE(support.size() == n_leaf, "展开后 support 长度 = 256");

    // 验证有效像素值
    for (uint32_t idx : valid_indices) {
        double expected = (double)idx * 2.0;
        if (std::fabs((double)signal[idx] - expected) > 1e-3) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "signal[%u] = %.6f (期望 %.6f)", idx, signal[idx], expected);
            ASSERT_TRUE(false, buf);
        }
    }
    ASSERT_TRUE(true, "有效像素 signal 值匹配");

    // 验证无效像素为 0
    ASSERT_NEAR(signal[1], 0.0f, 1e-6, "无效像素 signal[1] = 0");
    ASSERT_NEAR(signal[2], 0.0f, 1e-6, "无效像素 signal[2] = 0");
    ASSERT_TRUE(support[1] == 0, "无效像素 support[1] = 0");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 04: SPARSE_LIST 模式往返读写 (步骤11: 仅保存有效像素)
//   验证: 极少有效像素 → SPARSE_LIST 模式 → 读取展开后一致
// ============================================================================
static void test_04_sparse_roundtrip(int id) {
    TEST_CASE("SPARSE_LIST 模式往返读写 (步骤11)", id);
    using namespace hiss;
    const char* path = "test_sparse_roundtrip.hiss";

    HissGridSpec grid;
    // R04-B12: Writer 按实际编码大小自动选择 (忽略传入的 occ_mode)
    //   BITMAP = ceil(n_leaf/8), SPARSE_LIST = n_valid*4
    //   需 SPARSE_LIST < BITMAP: n_valid*4 < ceil(n_leaf/8)
    // NSIDE=1024 → depth=6, tile_nside=16, n_leaf_per_tile=4^6=4096
    //   BITMAP=512B, SPARSE_LIST(n_valid=1)=4B → 自动选 SPARSE_LIST
    grid.nside = 1024;
    grid.tile_nside = compute_tile_nside(1024);  // 16
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    HissMetadata meta;
    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    meta.photappl = 1;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    uint32_t n_leaf = 4096;  // 4^6 = 4096 for NSIDE=1024
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 1024.0 * 1024.0);

    // 构造极少有效像素 (1/4096 → SPARSE_LIST 自动选择)
    DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = 7;
    acc.pixel_area = A_p;
    acc.pixels.resize(n_leaf);
    // 仅 1 个有效像素: SPARSE_LIST(4B) < BITMAP(512B) → 自动选 SPARSE_LIST
    std::vector<uint32_t> valid_idx = {3};
    for (uint32_t idx : valid_idx) {
        acc.pixels[idx].sum_flux = (double)idx * 5.0;
        acc.pixels[idx].sum_area = A_p * 0.9;
        acc.pixels[idx].n_contrib = 1;
    }

    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "open 成功");
    // Writer 忽略传入的 occ_mode, 按编码大小自动选择
    ASSERT_TRUE(w.add_tile(7, acc, nullptr, OccupancyMode::SPARSE_LIST) == 0, "add_tile SPARSE 成功");
    ASSERT_TRUE(w.finalize() == 0, "finalize 成功");

    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "reader.open 成功");

    // n_valid=1, BITMAP=512B, SPARSE_LIST=4B → 自动选 SPARSE_LIST
    ASSERT_TRUE(r.tiles()[0].occ_mode == OccupancyMode::SPARSE_LIST,
                "occ_mode = SPARSE_LIST (自动选择)");

    std::vector<float> signal;
    std::vector<uint8_t> support;
    ASSERT_TRUE(r.read_tile(7, signal, support) == 0, "read_tile 成功");

    ASSERT_TRUE(signal.size() == n_leaf, "展开后 signal 长度 = 4096");
    ASSERT_TRUE(support.size() == n_leaf, "展开后 support 长度 = 4096");

    // 验证有效像素
    for (uint32_t idx : valid_idx) {
        double expected = (double)idx * 5.0;
        if (std::fabs((double)signal[idx] - expected) > 1e-3) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "signal[%u] = %.6f (期望 %.6f)", idx, signal[idx], expected);
            ASSERT_TRUE(false, buf);
        }
        ASSERT_TRUE(support[idx] > 0, "有效像素 support > 0");
    }
    ASSERT_TRUE(true, "全部有效像素 signal 值匹配");

    // 验证无效像素
    ASSERT_NEAR(signal[0], 0.0f, 1e-6, "无效像素 signal[0] = 0");
    ASSERT_NEAR(signal[1], 0.0f, 1e-6, "无效像素 signal[1] = 0");
    ASSERT_TRUE(support[0] == 0, "无效像素 support[0] = 0");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 05: occupancy 自动选择验证 (步骤11)
//   验证: Writer 根据占用率自动选择 FULL/BITMAP/SPARSE_LIST
// ============================================================================
static void test_05_auto_occupancy(int id) {
    TEST_CASE("occupancy 自动选择验证 (步骤11)", id);
    using namespace hiss;

    // 90% 占用 → BITMAP (R04-B12: FULL 仅当 100% 覆盖)
    {
        uint32_t n_leaf = 100;
        uint32_t n_valid = 90;  // 90% → 非 FULL
        OccupancyMode mode; // 由 writer 内部 auto_select 决定, 通过写入+读取验证
        // 直接构造 acc 写入, 读回 occ_mode 验证
        const char* path = "test_auto_full.hiss";
        HissGridSpec grid;
        grid.nside = 64; grid.tile_nside = compute_tile_nside(64);
        grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
        HissMetadata meta;
        meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
        meta.photappl = 1; meta.photscal = 1.0;
        std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

        double A_p = 0.001;
        DrizzleTileAccumulator acc;
        acc.tile_nside = 16; acc.parent_ipix = 0; acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        for (uint32_t i = 0; i < n_valid; i++) {
            acc.pixels[i].sum_flux = 1.0; acc.pixels[i].sum_area = A_p;
        }
        HissWriter w;
        w.open(path, grid, meta);
        w.add_tile(0, acc, nullptr, OccupancyMode::FULL);
        w.finalize();
        HissReader r;
        r.open(path);
        mode = r.tiles()[0].occ_mode;
        r.close(); // reader 关闭文件
        // 需要手动清理 reader 资源后再删除文件
        std::filesystem::remove(path);
        // R04-B12: 90% 不选 FULL (需 100%), BITMAP(13B) < SPARSE_LIST(360B) → BITMAP
        ASSERT_TRUE(mode == OccupancyMode::BITMAP, "90% 占用 → BITMAP (FULL 仅当 100%)");
    }

    // 30% 占用 → BITMAP (0.1 <= occ < 0.8)
    {
        const char* path = "test_auto_bitmap.hiss";
        HissGridSpec grid;
        grid.nside = 64; grid.tile_nside = compute_tile_nside(64);
        grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
        HissMetadata meta;
        meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
        meta.photappl = 1; meta.photscal = 1.0;
        std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

        uint32_t n_leaf = 100;
        double A_p = 0.001;
        DrizzleTileAccumulator acc;
        acc.tile_nside = 16; acc.parent_ipix = 1; acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        for (uint32_t i = 0; i < 30; i++) {
            acc.pixels[i].sum_flux = 1.0; acc.pixels[i].sum_area = A_p;
        }
        HissWriter w;
        w.open(path, grid, meta);
        w.add_tile(1, acc, nullptr, OccupancyMode::FULL);
        w.finalize();
        HissReader r;
        r.open(path);
        OccupancyMode mode = r.tiles()[0].occ_mode;
        r.close();
        std::filesystem::remove(path);
        ASSERT_TRUE(mode == OccupancyMode::BITMAP, "30% 占用 → BITMAP");
    }

    // 3% 占用 → SPARSE_LIST (< 0.1)
    {
        const char* path = "test_auto_sparse.hiss";
        HissGridSpec grid;
        grid.nside = 64; grid.tile_nside = compute_tile_nside(64);
        grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
        HissMetadata meta;
        meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
        meta.photappl = 1; meta.photscal = 1.0;
        std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

        uint32_t n_leaf = 100;
        double A_p = 0.001;
        DrizzleTileAccumulator acc;
        acc.tile_nside = 16; acc.parent_ipix = 2; acc.pixel_area = A_p;
        acc.pixels.resize(n_leaf);
        for (uint32_t i = 0; i < 3; i++) {
            acc.pixels[i].sum_flux = 1.0; acc.pixels[i].sum_area = A_p;
        }
        HissWriter w;
        w.open(path, grid, meta);
        w.add_tile(2, acc, nullptr, OccupancyMode::FULL);
        w.finalize();
        HissReader r;
        r.open(path);
        OccupancyMode mode = r.tiles()[0].occ_mode;
        r.close();
        std::filesystem::remove(path);
        ASSERT_TRUE(mode == OccupancyMode::SPARSE_LIST, "3% 占用 → SPARSE_LIST");
    }
}

// ============================================================================
// 测试 06: 流式写入验证 (步骤10)
//   验证: .partial 已清理, 文件签名正确, 多 Tile 写入正常
// ============================================================================
static void test_06_streaming_write(int id) {
    TEST_CASE("流式写入验证 (步骤10)", id);
    using namespace hiss;
    const char* path = "test_streaming.hiss";
    std::string partial_path = std::string(path) + ".partial";

    HissGridSpec grid;
    grid.nside = 64; grid.tile_nside = compute_tile_nside(64);
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
    HissMetadata meta;
    meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
    meta.photappl = 1; meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    double A_p = 0.001;
    // 写入 3 个 Tile (n_leaf_per_tile = 4^2 = 16 for NSIDE=64)
    uint32_t n_leaf = 16;
    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "open 成功");
    for (int t = 0; t < 3; t++) {
        DrizzleTileAccumulator acc;
        acc.tile_nside = 16; acc.parent_ipix = (uint64_t)(t + 10);
        acc.pixel_area = A_p; acc.pixels.resize(n_leaf);
        for (uint32_t i = 0; i < n_leaf; i++) {
            acc.pixels[i].sum_flux = (double)(t * 100 + i);
            acc.pixels[i].sum_area = A_p;
        }
        ASSERT_TRUE(w.add_tile((uint64_t)(t + 10), acc, nullptr, OccupancyMode::FULL) == 0,
                    "add_tile 多 Tile 写入成功");
    }
    ASSERT_TRUE(w.finalize() == 0, "finalize 成功");

    // 验证 .partial 已清理
    ASSERT_TRUE(!std::filesystem::exists(partial_path), ".partial 已清理");

    // 验证最终文件存在且签名正确
    ASSERT_TRUE(std::filesystem::exists(path), "最终文件存在");
    FILE* fp = std::fopen(path, "rb");
    ASSERT_TRUE(fp != nullptr, "可打开最终文件");
    unsigned char sig[8];
    ASSERT_TRUE(std::fread(sig, 1, 8, fp) == 8, "读取签名块");
    std::fclose(fp);
    ASSERT_TRUE(std::memcmp(sig, "HISS0100", 8) == 0, "MAGIC = HISS0100");

    // 验证 Tile 数
    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "reader.open 成功");
    ASSERT_TRUE(r.tiles().size() == 3, "Tile 数 = 3");

    // 验证各 Tile 数据
    for (int t = 0; t < 3; t++) {
        std::vector<float> signal;
        std::vector<uint8_t> support;
        ASSERT_TRUE(r.read_tile((uint64_t)(t + 10), signal, support) == 0,
                    "read_tile 多 Tile 读取成功");
        ASSERT_TRUE(signal.size() == n_leaf, "signal 长度 = n_leaf_per_tile (16)");
        ASSERT_NEAR(signal[0], (float)(t * 100), 1e-3, "signal[0] 匹配");
    }

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 07: SNR 子块往返读写 (冻结布局)
//   验证: SNR 子块布局 [n_points: uint32][points: n_points*8B]
// ============================================================================
static void test_07_snr_roundtrip(int id) {
    TEST_CASE("SNR 子块往返读写", id);
    using namespace hiss;
    const char* path = "test_snr.hiss";

    HissGridSpec grid;
    grid.nside = 64; grid.tile_nside = compute_tile_nside(64);
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
    HissMetadata meta;
    meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
    meta.photappl = 1; meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    double A_p = 0.001;
    DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 99;
    acc.pixel_area = A_p; acc.pixels.resize(16);  // n_leaf_per_tile = 4^2 = 16
    for (uint32_t i = 0; i < 16; i++) {
        acc.pixels[i].sum_flux = 1.0; acc.pixels[i].sum_area = A_p;
    }

    // SNR 控制点 (local_ipix must be < 16)
    // R04-B18: Writer 按升序排序并去重, 读取后顺序为按 local_ipix 升序
    HissSnrBlock snr;
    snr.points.push_back({5, 12.5f});
    snr.points.push_back({10, 25.0f});
    snr.points.push_back({3, 8.3f});

    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "open 成功");
    ASSERT_TRUE(w.add_tile(99, acc, &snr, OccupancyMode::FULL) == 0, "add_tile with SNR 成功");
    ASSERT_TRUE(w.finalize() == 0, "finalize 成功");

    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "reader.open 成功");

    HissSnrBlock snr_read;
    ASSERT_TRUE(r.read_tile_snr(99, snr_read) == 0, "read_tile_snr 成功");
    ASSERT_TRUE(snr_read.points.size() == 3, "SNR 控制点数 = 3");

    // 验证每点 local_ipix + snr (Writer 排序后按 local_ipix 升序: 3, 5, 10)
    ASSERT_TRUE(snr_read.points[0].local_ipix == 3, "point[0].local_ipix = 3 (排序后)");
    ASSERT_NEAR(snr_read.points[0].snr, 8.3f, 1e-5, "point[0].snr = 8.3");
    ASSERT_TRUE(snr_read.points[1].local_ipix == 5, "point[1].local_ipix = 5 (排序后)");
    ASSERT_NEAR(snr_read.points[1].snr, 12.5f, 1e-5, "point[1].snr = 12.5");
    ASSERT_TRUE(snr_read.points[2].local_ipix == 10, "point[2].local_ipix = 10 (排序后)");
    ASSERT_NEAR(snr_read.points[2].snr, 25.0f, 1e-5, "point[2].snr = 25.0");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 08: support 语义验证 (步骤2)
//   验证: support = round(255 * sum_area / A_p), 钳制 [0,1]
// ============================================================================
static void test_08_support_semantics(int id) {
    TEST_CASE("support 语义验证 (步骤2: support = sum_area / A_p)", id);
    using namespace hiss;

    DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 0;
    acc.pixel_area = 0.01;  // A_p = 0.01 球面度
    acc.pixels.resize(4);

    // 像素 0: sum_area = A_p → S = 1.0 → support = 255
    acc.pixels[0].sum_area = 0.01;
    // 像素 1: sum_area = 0.5*A_p → S = 0.5 → support = round(127.5) = 128
    acc.pixels[1].sum_area = 0.005;
    // 像素 2: sum_area = 0 → S = 0 → support = 0
    acc.pixels[2].sum_area = 0.0;
    // 像素 3: sum_area = 2*A_p → S = 2.0 → 钳制 1.0 → support = 255
    acc.pixels[3].sum_area = 0.02;

    std::vector<uint8_t> support;
    acc.finalize_support(support);

    ASSERT_TRUE(support[0] == 255, "S=1.0 → support=255");
    ASSERT_TRUE(support[1] == 128, "S=0.5 → support=128 (round(127.5))");
    ASSERT_TRUE(support[2] == 0, "S=0.0 → support=0");
    ASSERT_TRUE(support[3] == 255, "S=2.0 钳制 1.0 → support=255");
}

// ============================================================================
// 测试 09: aio_hiss_write/read C API 往返 (步骤8: 新后端兼容性)
//   验证: C API 写入 → C API 读取 → 数据一致
// ============================================================================
static void test_09_capi_roundtrip(int id) {
    TEST_CASE("aio_hiss_write/read C API 往返 (步骤8)", id);
    const char* path = "test_capi.hiss";

    // 准备测试数据 (NSIDE=64, NESTED)
    uint32_t nside = 64;
    int nested = 1;
    // 构造 50 个像素, 分布在不同 Tile
    uint64_t n_pix = 50;
    std::vector<uint64_t> ipix(n_pix);
    std::vector<float> pixel(n_pix);
    for (uint64_t i = 0; i < n_pix; i++) {
        ipix[i] = i * 100;  // 分散在不同 Tile
        pixel[i] = (float)(i * 1.5);
    }

    // 写入 (新 HissWriter 后端)
    int ret = aio_hiss_write(path, nside, nested, n_pix, ipix.data(), pixel.data(),
                              nullptr, nullptr);
    ASSERT_TRUE(ret == 0, "aio_hiss_write 成功 (新 HissWriter 后端)");

    // 读取 (新 HissReader 后端)
    uint32_t r_nside = 0;
    int r_nested = 0;
    uint64_t r_npix = 0;
    uint64_t* r_ipix = nullptr;
    float* r_pixel = nullptr;
    float* r_snr = nullptr;
    char* r_meta = nullptr;
    ret = aio_hiss_read(path, &r_nside, &r_nested, &r_npix, &r_ipix, &r_pixel, &r_snr, &r_meta);
    ASSERT_TRUE(ret == 0, "aio_hiss_read 成功 (新 HissReader 后端)");
    ASSERT_TRUE(r_nside == nside, "nside 一致");
    ASSERT_TRUE(r_nested == nested, "nested 一致");
    ASSERT_TRUE(r_npix > 0, "读取到像素数据");
    ASSERT_TRUE(r_snr == nullptr, "SNR = nullptr (新格式不输出逐像素 SNR)");
    ASSERT_TRUE(r_meta != nullptr, "meta_json 非空");

    // 验证: 读取的像素应包含写入的像素 (signal = sumFlux, 非零)
    // 注意: aio_hiss_write 中 sum_area = A_p (全覆盖), 所以所有写入像素应被读回
    // 建立查找表
    std::map<uint64_t, float> expected_map;
    for (uint64_t i = 0; i < n_pix; i++) {
        expected_map[ipix[i]] = pixel[i];
    }

    uint64_t matched = 0;
    for (uint64_t i = 0; i < r_npix; i++) {
        auto it = expected_map.find(r_ipix[i]);
        if (it != expected_map.end()) {
            if (std::fabs((double)r_pixel[i] - (double)it->second) < 1e-3) {
                matched++;
            }
        }
    }
    fprintf(stderr, "  匹配 %llu / %llu 像素\n", (unsigned long long)matched,
            (unsigned long long)n_pix);
    ASSERT_TRUE(matched == n_pix, "全部写入像素被正确读回");

    // 清理
    if (r_ipix) aio_hio_free(r_ipix);
    if (r_pixel) aio_hio_free(r_pixel);
    if (r_meta) aio_hio_free(r_meta);
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 10: 元数据 WCS 移除验证 (步骤8)
//   验证: HissMetadata::to_json 不包含 cd/crval/crpix/sip 等完整 WCS 字段
// ============================================================================
static void test_10_no_wcs_metadata(int id) {
    TEST_CASE("元数据 WCS 移除验证 (步骤8)", id);
    using namespace hiss;
    const char* path = "test_no_wcs.hiss";

    HissGridSpec grid;
    grid.nside = 64; grid.tile_nside = compute_tile_nside(64);
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;
    HissMetadata meta;
    meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
    meta.photappl = 1; meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");
    std::snprintf(meta.object, sizeof(meta.object), "TestObject");
    meta.exptime = 60.0;

    double A_p = 0.001;
    DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 0;
    acc.pixel_area = A_p; acc.pixels.resize(16);
    for (uint32_t i = 0; i < 16; i++) {
        acc.pixels[i].sum_flux = 1.0; acc.pixels[i].sum_area = A_p;
    }

    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "open 成功");
    ASSERT_TRUE(w.add_tile(0, acc, nullptr, OccupancyMode::FULL) == 0, "add_tile 成功");
    ASSERT_TRUE(w.finalize() == 0, "finalize 成功");

    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "reader.open 成功");
    HissMetadata read_meta = r.metadata();
    std::string json = read_meta.to_json();
    fprintf(stderr, "  元数据 JSON: %s\n", json.c_str());

    // 验证: 不包含完整 WCS 字段
    ASSERT_TRUE(json.find("cd") == std::string::npos || json.find("\"cd") == std::string::npos,
                "JSON 不含 cd 矩阵字段");
    ASSERT_TRUE(json.find("crval") == std::string::npos, "JSON 不含 crval 字段");
    ASSERT_TRUE(json.find("crpix") == std::string::npos, "JSON 不含 crpix 字段");
    ASSERT_TRUE(json.find("sip_order") == std::string::npos, "JSON 不含 sip_order 字段");
    ASSERT_TRUE(json.find("sip_a") == std::string::npos, "JSON 不含 sip_a 系数");
    ASSERT_TRUE(json.find("sip_b") == std::string::npos, "JSON 不含 sip_b 系数");

    // 验证: 包含必需字段
    ASSERT_TRUE(json.find("nside") != std::string::npos, "JSON 含 nside");
    ASSERT_TRUE(json.find("tile_nside") != std::string::npos, "JSON 含 tile_nside");
    ASSERT_TRUE(json.find("photappl") != std::string::npos, "JSON 含 photappl");
    ASSERT_TRUE(json.find("bunit") != std::string::npos, "JSON 含 bunit");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// main
// ============================================================================
int main() {
    fprintf(stderr, "=== HISS Writer 核心改造集成测试 (WP-E 步骤7,8,10,11) ===\n");

    test_01_signal_is_cumulative_flux(1);
    test_02_full_roundtrip(2);
    test_03_bitmap_roundtrip(3);
    test_04_sparse_roundtrip(4);
    test_05_auto_occupancy(5);
    test_06_streaming_write(6);
    test_07_snr_roundtrip(7);
    test_08_support_semantics(8);
    test_09_capi_roundtrip(9);
    test_10_no_wcs_metadata(10);

    fprintf(stderr, "\n=== 测试结果 ===\n");
    fprintf(stderr, "  总计: %d\n", g_test_total);
    fprintf(stderr, "  通过: %d\n", g_test_passed + (g_test_total - (int)g_failures.size()));
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

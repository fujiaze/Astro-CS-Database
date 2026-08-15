// ============================================================================
// test_precision_dual.cpp - HISS Writer/Reader FP32/FP64 双模式验证测试
//
// 用确定性合成数据验证:
// 01. FP32 模式 roundtrip 精确匹配 (bit-exact)
// 02. FP64 模式 roundtrip 精确匹配 (bit-exact)
// 03. 禁止静默转换: FP32 文件用 read_tile_signal_f64 应返回 HISS_ERR_UNSUPPORTED
// FP64 文件用 read_tile_signal / read_tile 应返回 HISS_ERR_UNSUPPORTED
// 04. metadata 中 precision_mode 和 signal_dtype 字段正确记录
// 05. FP32 与 FP64 精度差异验证 (FP64 保留 float32 无法表示的精度)
//
// 编译 (从 lib/astro_image_io/ 目录):
// g++ -std=c++17 -O2 -fopenmp -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
// -Iinclude -Isrc \
// tests/test_precision_dual.cpp \
// src/hiss_codec.cpp src/hiss_common.cpp \
// src/hiss_tile_model.cpp src/hiss_transform.cpp \
// src/hiss_writer.cpp src/hiss_stream_writer.cpp \
// src/hiss_reader.cpp \
// src/healpix/aio_healpix_io.cpp \
// src/aio_api.cpp src/aio_log.cpp \
// -lzstd -lm -o tests/test_precision_dual.exe
//
// 运行:
// ./tests/test_precision_dual.exe
//
// 约束:
// - 确定性合成数据 (固定值, 不用随机数)
// - FP32 roundtrip bit-exact (writer: double->float32 强转, reader: float32 读回)
// - FP64 roundtrip bit-exact (writer: double 直接写, reader: double 读回)
// - 不使用 -ffast-math
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
#include <filesystem>

// ============================================================================
// 测试框架: 轻量级断言 + 结果收集
// ============================================================================

static int g_test_total  = 0;
static int g_test_passed = 0;
static std::vector<std::string> g_failures;

#define TEST_CASE(name, id) \
    fprintf(stderr, "\n========== [TEST %02d] %s ==========\n", id, name); \
    g_test_total++;

#define PASS(msg) \
    do { \
        fprintf(stderr, "  OK: %s\n", (msg)); \
        g_test_passed++; \
    } while (0)

#define FAIL(msg) \
    do { \
        std::string m = std::string("[TEST ") + std::to_string(id) + "] FAIL: " + (msg); \
        fprintf(stderr, "  FAIL: %s\n", (msg)); \
        g_failures.push_back(m); \
    } while (0)

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        FAIL(msg); \
        return; \
    } else { \
        fprintf(stderr, "  OK: %s\n", (msg)); \
        g_test_passed++; \
    }

// ============================================================================
// 辅助: 构造确定性合成累加器
// 使用 NSIDE=64 -> tile_nside=16 -> n_leaf_per_tile = 4^2 = 16
// 所有像素全覆盖 (sum_area = A_p), FULL 模式
// ============================================================================

static hiss::DrizzleTileAccumulator make_deterministic_acc(
    uint32_t tile_nside, uint64_t parent_ipix, uint32_t n_leaf,
    const std::vector<double>& flux_values, double pixel_area) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area = pixel_area;
    acc.pixels.resize(n_leaf);
    for (uint32_t i = 0; i < n_leaf && i < flux_values.size(); i++) {
        acc.pixels[i].sum_flux = flux_values[i];
        acc.pixels[i].sum_area = pixel_area;  // 全覆盖 -> support = 255
        acc.pixels[i].n_contrib = 1;
    }
    return acc;
}

// ============================================================================
// 测试 01: FP32 模式 roundtrip 精确匹配 (bit-exact)
// 写入: add_tile (FP32, 默认), writer 内部 double->float32 强转
// 读取: read_tile_signal (float32)
// 验证: 每个像素 bit-exact 匹配 (float32 字节比较)
//
// 使用值必须在 float32 中精确可表示 (整数 + 简单二进制分数)
// ============================================================================

static void test_01_fp32_roundtrip_bitexact(int id) {
    TEST_CASE("FP32 模式 roundtrip bit-exact", id);
    using namespace hiss;
    const char* path = "test_fp32_roundtrip.hiss";

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

    uint32_t n_leaf = 16;  // 4^2 = 16 for NSIDE=64
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);

    // 确定性合成数据: 全部在 float32 中精确可表示
    // (整数 / 2 的幂次分数 / 0.0 / 负数)
    std::vector<double> flux_values = {
        0.0,         // 零
        1.0,         // 单位
        100.5,       // 100 + 1/2
        200.25,      // 200 + 1/4
        0.125,       // 1/8
        -50.5,       // 负数 + 1/2
        1234.0,      // 整数
        4096.0,      // 2 的幂
        8192.0,      // 2 的幂
        0.0,         // 零 (无贡献但 sum_area=A_p)
        3.5,         // 3 + 1/2
        7.75,        // 7 + 3/4
        -0.0625,     // -1/16
        65536.0,     // 2^16
        1.0,         // 重复值
        255.99609375 // 255 + 255/256 (精确 float32)
    };

    DrizzleTileAccumulator acc = make_deterministic_acc(16, 42, n_leaf, flux_values, A_p);

    // 写入 FP32 (默认 add_tile)
    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "FP32: open 成功");
    ASSERT_TRUE(w.add_tile(42, acc, nullptr, OccupancyMode::FULL) == 0,
                "FP32: add_tile (FP32 默认) 成功");
    ASSERT_TRUE(w.finalize() == 0, "FP32: finalize 成功");

    // 读取 FP32 signal
    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "FP32: reader.open 成功");
    ASSERT_TRUE(r.tiles().size() == 1, "FP32: Tile 数 = 1");

    std::vector<float> signal;
    ASSERT_TRUE(r.read_tile_signal(42, signal) == 0, "FP32: read_tile_signal 成功");
    ASSERT_TRUE(signal.size() == n_leaf, "FP32: signal 长度 = 16");

    // bit-exact 验证: reader 读回的 float32 应等于 (float)原始 double
    int bitexact_count = 0;
    for (uint32_t i = 0; i < n_leaf; i++) {
        float expected_f32 = (float)flux_values[i];
        if (std::memcmp(&signal[i], &expected_f32, sizeof(float)) == 0) {
            bitexact_count++;
        } else {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "FP32: signal[%u] bit-exact 失败 (got=0x%08x expected=0x%08x)",
                          i, *(uint32_t*)&signal[i], *(uint32_t*)&expected_f32);
            FAIL(buf);
            r.close();
            std::filesystem::remove(path);
            return;
        }
    }
    ASSERT_TRUE(bitexact_count == (int)n_leaf, "FP32: 全部 16 像素 bit-exact 匹配");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 02: FP64 模式 roundtrip 精确匹配 (bit-exact)
// 写入: add_tile_f64 (FP64), writer 直接写 double 字节
// 读取: read_tile_signal_f64 (double)
// 验证: 每个像素 bit-exact 匹配 (double 字节比较)
//
// 使用 float32 无法精确表示的值, 证明 FP64 保留精度
// ============================================================================

static void test_02_fp64_roundtrip_bitexact(int id) {
    TEST_CASE("FP64 模式 roundtrip bit-exact", id);
    using namespace hiss;
    const char* path = "test_fp64_roundtrip.hiss";

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

    uint32_t n_leaf = 16;
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);

    // 确定性合成数据: 包含 float32 无法精确表示的值
    // 这些值在 double 中精确 (到 double 精度), 但 (float) 强转会损失精度
    std::vector<double> flux_values = {
        0.1,                    // float32 无法精确表示
        0.2,                    // float32 无法精确表示
        1.0 / 3.0,              // 1/3, float32 无法精确表示
        3.141592653589793,      // PI, float32 损失精度
        2.718281828459045,      // e, float32 损失精度
        123456789.123456789,    // 大数 + 小数, float32 损失精度
        1e-10,                  // 小数, float32 损失精度
        1e15 + 0.125,           // 大数 + 分数
        -0.1,                   // 负小数
        0.0,
        1.0 / 7.0,              // 1/7
        999999999.999999,       // 大小数混合
        1e-100,                 // 极小数 (subnormal 范围)
        1e100,                  // 极大数
        -3.141592653589793,     // 负 PI
        123456789012345.0       // 大整数 (double 精确, float32 损失)
    };

    DrizzleTileAccumulator acc = make_deterministic_acc(16, 99, n_leaf, flux_values, A_p);

    // 写入 FP64
    HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "FP64: open 成功");
    ASSERT_TRUE(w.add_tile_f64(99, acc, nullptr, OccupancyMode::FULL) == 0,
                "FP64: add_tile_f64 成功");
    ASSERT_TRUE(w.finalize() == 0, "FP64: finalize 成功");

    // 读取 FP64 signal
    HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "FP64: reader.open 成功");
    ASSERT_TRUE(r.tiles().size() == 1, "FP64: Tile 数 = 1");

    std::vector<double> signal;
    ASSERT_TRUE(r.read_tile_signal_f64(99, signal) == 0, "FP64: read_tile_signal_f64 成功");
    ASSERT_TRUE(signal.size() == n_leaf, "FP64: signal 长度 = 16");

    // bit-exact 验证: reader 读回的 double 应等于原始 double
    int bitexact_count = 0;
    for (uint32_t i = 0; i < n_leaf; i++) {
        if (std::memcmp(&signal[i], &flux_values[i], sizeof(double)) == 0) {
            bitexact_count++;
        } else {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "FP64: signal[%u] bit-exact 失败 (got=%.17g expected=%.17g)",
                          i, signal[i], flux_values[i]);
            FAIL(buf);
            r.close();
            std::filesystem::remove(path);
            return;
        }
    }
    ASSERT_TRUE(bitexact_count == (int)n_leaf, "FP64: 全部 16 像素 bit-exact 匹配");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 测试 03: 禁止静默转换 (cross-mode rejection)
// FP32 文件 + read_tile_signal_f64 -> HISS_ERR_UNSUPPORTED
// FP64 文件 + read_tile_signal -> HISS_ERR_UNSUPPORTED
// FP64 文件 + read_tile -> HISS_ERR_UNSUPPORTED (signal+support 接口)
// ============================================================================

static void test_03_cross_mode_rejection(int id) {
    TEST_CASE("禁止静默转换 (cross-mode rejection)", id);
    using namespace hiss;

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

    uint32_t n_leaf = 16;
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);

    std::vector<double> flux_values = {
        1.0, 2.0, 3.5, 4.25, 0.5, 100.0, -1.5, 0.0,
        8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0
    };

    // --- 3a: 写 FP32 文件, 用 read_tile_signal_f64 读 -> 应返回 HISS_ERR_UNSUPPORTED ---
    {
        const char* path = "test_cross_fp32_file.hiss";
        DrizzleTileAccumulator acc = make_deterministic_acc(16, 42, n_leaf, flux_values, A_p);
        HissWriter w;
        ASSERT_TRUE(w.open(path, grid, meta) == 0, "3a: FP32 文件 open 成功");
        ASSERT_TRUE(w.add_tile(42, acc, nullptr, OccupancyMode::FULL) == 0,
                    "3a: FP32 文件 add_tile 成功");
        ASSERT_TRUE(w.finalize() == 0, "3a: FP32 文件 finalize 成功");

        HissReader r;
        ASSERT_TRUE(r.open(path) == 0, "3a: FP32 文件 reader.open 成功");

        // FP32 文件用 read_tile_signal_f64 -> 应返回 HISS_ERR_UNSUPPORTED
        std::vector<double> signal_f64;
        int ret = r.read_tile_signal_f64(42, signal_f64);
        ASSERT_TRUE(ret == HISS_ERR_UNSUPPORTED,
                    "3a: FP32 文件 + read_tile_signal_f64 -> HISS_ERR_UNSUPPORTED");

        r.close();
        std::filesystem::remove(path);
    }

    // --- 3b: 写 FP64 文件, 用 read_tile_signal 读 -> 应返回 HISS_ERR_UNSUPPORTED ---
    {
        const char* path = "test_cross_fp64_file.hiss";
        DrizzleTileAccumulator acc = make_deterministic_acc(16, 99, n_leaf, flux_values, A_p);
        HissWriter w;
        ASSERT_TRUE(w.open(path, grid, meta) == 0, "3b: FP64 文件 open 成功");
        ASSERT_TRUE(w.add_tile_f64(99, acc, nullptr, OccupancyMode::FULL) == 0,
                    "3b: FP64 文件 add_tile_f64 成功");
        ASSERT_TRUE(w.finalize() == 0, "3b: FP64 文件 finalize 成功");

        HissReader r;
        ASSERT_TRUE(r.open(path) == 0, "3b: FP64 文件 reader.open 成功");

        // FP64 文件用 read_tile_signal (float32) -> 应返回 HISS_ERR_UNSUPPORTED
        std::vector<float> signal_f32;
        int ret = r.read_tile_signal(99, signal_f32);
        ASSERT_TRUE(ret == HISS_ERR_UNSUPPORTED,
                    "3b: FP64 文件 + read_tile_signal -> HISS_ERR_UNSUPPORTED");

        // FP64 文件用 read_tile (signal+support, float32 接口) -> 应返回 HISS_ERR_UNSUPPORTED
        std::vector<float> sig2;
        std::vector<uint8_t> sup2;
        int ret2 = r.read_tile(99, sig2, sup2);
        ASSERT_TRUE(ret2 == HISS_ERR_UNSUPPORTED,
                    "3b: FP64 文件 + read_tile (signal+support) -> HISS_ERR_UNSUPPORTED");

        r.close();
        std::filesystem::remove(path);
    }
}

// ============================================================================
// 测试 04: metadata 中 precision_mode 和 signal_dtype 字段正确记录
// FP32 文件: precision_mode=0, signal_dtype=0
// FP64 文件: precision_mode=1, signal_dtype=1
// 同时验证 to_json 输出包含这些字段
// ============================================================================

static void test_04_metadata_precision_fields(int id) {
    TEST_CASE("metadata precision_mode / signal_dtype 字段", id);
    using namespace hiss;

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

    uint32_t n_leaf = 16;
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);
    std::vector<double> flux_values = {
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
        9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0
    };

    // --- 4a: FP32 文件 metadata 字段 ---
    {
        const char* path = "test_meta_fp32.hiss";
        DrizzleTileAccumulator acc = make_deterministic_acc(16, 42, n_leaf, flux_values, A_p);
        HissWriter w;
        ASSERT_TRUE(w.open(path, grid, meta) == 0, "4a: FP32 open 成功");
        ASSERT_TRUE(w.add_tile(42, acc, nullptr, OccupancyMode::FULL) == 0,
                    "4a: FP32 add_tile 成功");
        ASSERT_TRUE(w.finalize() == 0, "4a: FP32 finalize 成功");

        HissReader r;
        ASSERT_TRUE(r.open(path) == 0, "4a: FP32 reader.open 成功");

        ASSERT_TRUE(r.precision_mode() == 0, "4a: FP32 precision_mode == 0");
        ASSERT_TRUE(r.signal_dtype() == 0, "4a: FP32 signal_dtype == 0");

        HissMetadata read_meta = r.metadata();
        ASSERT_TRUE(read_meta.precision_mode == 0, "4a: FP32 metadata.precision_mode == 0");
        ASSERT_TRUE(read_meta.signal_dtype == 0, "4a: FP32 metadata.signal_dtype == 0");

        // 验证 JSON 包含字段
        std::string json = read_meta.to_json();
        ASSERT_TRUE(json.find("precision_mode") != std::string::npos,
                    "4a: JSON 含 precision_mode 字段");
        ASSERT_TRUE(json.find("signal_dtype") != std::string::npos,
                    "4a: JSON 含 signal_dtype 字段");
        ASSERT_TRUE(json.find("\"precision_mode\":0") != std::string::npos ||
                    json.find("\"precision_mode\": 0") != std::string::npos,
                    "4a: JSON precision_mode=0");
        ASSERT_TRUE(json.find("\"signal_dtype\":0") != std::string::npos ||
                    json.find("\"signal_dtype\": 0") != std::string::npos,
                    "4a: JSON signal_dtype=0");

        fprintf(stderr, "  FP32 metadata JSON: %s\n", json.c_str());

        r.close();
        std::filesystem::remove(path);
    }

    // --- 4b: FP64 文件 metadata 字段 ---
    {
        const char* path = "test_meta_fp64.hiss";
        DrizzleTileAccumulator acc = make_deterministic_acc(16, 99, n_leaf, flux_values, A_p);
        HissWriter w;
        ASSERT_TRUE(w.open(path, grid, meta) == 0, "4b: FP64 open 成功");
        ASSERT_TRUE(w.add_tile_f64(99, acc, nullptr, OccupancyMode::FULL) == 0,
                    "4b: FP64 add_tile_f64 成功");
        ASSERT_TRUE(w.finalize() == 0, "4b: FP64 finalize 成功");

        HissReader r;
        ASSERT_TRUE(r.open(path) == 0, "4b: FP64 reader.open 成功");

        ASSERT_TRUE(r.precision_mode() == 1, "4b: FP64 precision_mode == 1");
        ASSERT_TRUE(r.signal_dtype() == 1, "4b: FP64 signal_dtype == 1");

        HissMetadata read_meta = r.metadata();
        ASSERT_TRUE(read_meta.precision_mode == 1, "4b: FP64 metadata.precision_mode == 1");
        ASSERT_TRUE(read_meta.signal_dtype == 1, "4b: FP64 metadata.signal_dtype == 1");

        std::string json = read_meta.to_json();
        ASSERT_TRUE(json.find("precision_mode") != std::string::npos,
                    "4b: JSON 含 precision_mode 字段");
        ASSERT_TRUE(json.find("signal_dtype") != std::string::npos,
                    "4b: JSON 含 signal_dtype 字段");
        ASSERT_TRUE(json.find("\"precision_mode\":1") != std::string::npos ||
                    json.find("\"precision_mode\": 1") != std::string::npos,
                    "4b: JSON precision_mode=1");
        ASSERT_TRUE(json.find("\"signal_dtype\":1") != std::string::npos ||
                    json.find("\"signal_dtype\": 1") != std::string::npos,
                    "4b: JSON signal_dtype=1");

        fprintf(stderr, "  FP64 metadata JSON: %s\n", json.c_str());

        r.close();
        std::filesystem::remove(path);
    }
}

// ============================================================================
// 测试 05: FP32 与 FP64 精度差异验证
// 同一 double 输入值, FP32 模式会损失精度, FP64 模式保留精度
// 验证: 对 float32 无法精确表示的值, FP32 roundtrip != 原始 double
// 而 FP64 roundtrip == 原始 double
// 这证明双模式的真实意义 (不是空转)
// ============================================================================

static void test_05_precision_difference(int id) {
    TEST_CASE("FP32 与 FP64 精度差异验证", id);
    using namespace hiss;

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

    uint32_t n_leaf = 16;
    double A_p = 4.0 * 3.14159265358979 / (12.0 * 64.0 * 64.0);

    // 使用 float32 无法精确表示的值
    std::vector<double> flux_values = {
        0.1, 0.2, 1.0/3.0, 3.141592653589793,
        2.718281828459045, 0.7, 0.3, 1.0/7.0,
        0.123456789, 0.987654321, 0.555555555, 0.444444444,
        1.1, 2.2, 3.3, 4.4
    };

    // --- 5a: FP32 模式 roundtrip, 验证部分值与原始 double 不同 ---
    {
        const char* path = "test_diff_fp32.hiss";
        DrizzleTileAccumulator acc = make_deterministic_acc(16, 42, n_leaf, flux_values, A_p);
        HissWriter w;
        ASSERT_TRUE(w.open(path, grid, meta) == 0, "5a: FP32 open 成功");
        ASSERT_TRUE(w.add_tile(42, acc, nullptr, OccupancyMode::FULL) == 0,
                    "5a: FP32 add_tile 成功");
        ASSERT_TRUE(w.finalize() == 0, "5a: FP32 finalize 成功");

        HissReader r;
        ASSERT_TRUE(r.open(path) == 0, "5a: FP32 reader.open 成功");
        std::vector<float> signal;
        ASSERT_TRUE(r.read_tile_signal(42, signal) == 0, "5a: FP32 read_tile_signal 成功");

        // 统计: float32 roundtrip 后, 哪些值与原始 double 不同
        // (float 强转 -> float32 存储 -> 读回 float32, 升回 double 比较)
        int diff_count = 0;
        for (uint32_t i = 0; i < n_leaf; i++) {
            double roundtrip_double = (double)signal[i];  // float32 -> double
            if (std::memcmp(&roundtrip_double, &flux_values[i], sizeof(double)) != 0) {
                diff_count++;
            }
        }
        fprintf(stderr, "  5a: FP32 roundtrip 后 %d / %u 个值与原始 double 不同 (预期 >0)\n",
                diff_count, n_leaf);
        ASSERT_TRUE(diff_count > 0,
                    "5a: FP32 roundtrip 至少 1 个值与原始 double 不同 (证明 FP32 有精度损失)");

        r.close();
        std::filesystem::remove(path);
    }

    // --- 5b: FP64 模式 roundtrip, 验证全部值与原始 double 相同 ---
    {
        const char* path = "test_diff_fp64.hiss";
        DrizzleTileAccumulator acc = make_deterministic_acc(16, 99, n_leaf, flux_values, A_p);
        HissWriter w;
        ASSERT_TRUE(w.open(path, grid, meta) == 0, "5b: FP64 open 成功");
        ASSERT_TRUE(w.add_tile_f64(99, acc, nullptr, OccupancyMode::FULL) == 0,
                    "5b: FP64 add_tile_f64 成功");
        ASSERT_TRUE(w.finalize() == 0, "5b: FP64 finalize 成功");

        HissReader r;
        ASSERT_TRUE(r.open(path) == 0, "5b: FP64 reader.open 成功");
        std::vector<double> signal;
        ASSERT_TRUE(r.read_tile_signal_f64(99, signal) == 0, "5b: FP64 read_tile_signal_f64 成功");

        int same_count = 0;
        for (uint32_t i = 0; i < n_leaf; i++) {
            if (std::memcmp(&signal[i], &flux_values[i], sizeof(double)) == 0) {
                same_count++;
            }
        }
        fprintf(stderr, "  5b: FP64 roundtrip 后 %d / %u 个值与原始 double bit-exact 相同\n",
                same_count, n_leaf);
        ASSERT_TRUE(same_count == (int)n_leaf,
                    "5b: FP64 roundtrip 全部值与原始 double bit-exact 相同");

        r.close();
        std::filesystem::remove(path);
    }
}

// ============================================================================
// main
// ============================================================================

int main() {
    fprintf(stderr, "=== HISS Writer/Reader FP32/FP64 双模式验证测试 (R10) ===\n");
    fprintf(stderr, "=== 确定性合成数据, bit-exact 验证, 禁止 -ffast-math ===\n");

    test_01_fp32_roundtrip_bitexact(1);
    test_02_fp64_roundtrip_bitexact(2);
    test_03_cross_mode_rejection(3);
    test_04_metadata_precision_fields(4);
    test_05_precision_difference(5);

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

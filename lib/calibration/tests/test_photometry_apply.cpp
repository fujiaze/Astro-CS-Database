// ============================================================================
// test_photometry_apply.cpp - Gaia 测光比例应用模块单元测试
// TEST-CAL-001: 测光比例应用 + Writer 元数据一致性
//
// 规范依据: 02_FROZEN §7 / spec.md 步骤9 / 00_COMMON_CONTRACTS §5
//
// 测试覆盖:
// 1. photscal=2.0 → 每个像素值×2.0
// 2. photscal=1.0 → 像素值不变
// 3. photscal=0.0 → 所有像素为 0
// 4. photscal=0.5 → 像素值减半
// 5. 参数校验 (nullptr/非法尺寸/NaN photscal)
// 6. Writer 元数据一致性: apply_photometry=true + BUNIT=ASTROCS_RELATIVE_FLUX → 成功
// 7. Writer 元数据一致性: apply_photometry=false + BUNIT=ASTROCS_RELATIVE_FLUX → 拒绝
// 8. Writer 元数据一致性: apply_photometry=false + BUNIT=ADU → 成功
//
// 编译 (从 tests/ 目录, 链接真实 HissWriter::open()):
// g++ -std=c++17 -O2 -fopenmp -DHAS_ZSTD -DAIO_ENABLE_HEALPIX \
// -I../src -I../../astro_image_io/include -I../../astro_image_io/src \
// test_photometry_apply.cpp ../src/photometry_apply.cpp \
// ../../astro_image_io/src/hiss_codec.cpp \
// ../../astro_image_io/src/hiss_common.cpp \
// ../../astro_image_io/src/hiss_tile_model.cpp \
// ../../astro_image_io/src/hiss_writer.cpp \
// ../../astro_image_io/src/hiss_stream_writer.cpp \
// ../../astro_image_io/src/hiss_transform.cpp \
// ../../astro_image_io/src/hiss_reader.cpp \
// ../../astro_image_io/src/healpix/aio_healpix_io.cpp \
// ../../astro_image_io/src/aio_api.cpp \
// ../../astro_image_io/src/aio_log.cpp \
// -lzstd -lm -o test_photometry_apply.exe
// ============================================================================

#include "photometry_apply.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

// ---- Writer 测试依赖: 链接真实 HissWriter (hiss_format.h) ----
#include "hiss_format.h"

// ============================================================================
// 简单测试框架 (维护正确通过/失败计数, 禁止 ASSERT_TRUE(true) 软通过)
// ============================================================================

static int g_pass = 0;
static int g_fail = 0;

static void record_pass(const char* msg) {
    g_pass++;
    printf("[PASS] %s\n", msg);
}

static void record_fail(const char* msg, const char* detail = nullptr) {
    g_fail++;
    if (detail) {
        printf("[FAIL] %s (%s)\n", msg, detail);
    } else {
        printf("[FAIL] %s\n", msg);
    }
}

#define ASSERT_TRUE(cond, msg) do { \
    if (cond) record_pass(msg); \
    else record_fail(msg); \
} while(0)

#define ASSERT_FLOAT_NEAR(actual, expected, eps, msg) do { \
    float a = (float)(actual); \
    float e = (float)(expected); \
    if (std::fabs(a - e) <= (eps)) record_pass(msg); \
    else { char buf[128]; std::snprintf(buf, sizeof(buf), "got=%g expected=%g", a, e); record_fail(msg, buf); } \
} while(0)

// ============================================================================
// 测试 1: photscal=2.0 → 每个像素值×2.0
// ============================================================================
static void test_photscal_2x() {
    const int W = 4, H = 4;
    std::vector<float> light(W * H);
    std::vector<float> out(W * H);
    for (int i = 0; i < W * H; i++) light[i] = (float)i;

    int rc = calibration::apply_photometry(light.data(), W, H, 2.0, out.data());
    ASSERT_TRUE(rc == 0, "photscal=2.0: 返回值=0 (成功)");

    bool allOk = true;
    for (int i = 0; i < W * H; i++) {
        if (std::fabs(out[i] - (float)i * 2.0f) > 1e-5f) { allOk = false; break; }
    }
    ASSERT_TRUE(allOk, "photscal=2.0: 每个像素值×2.0");
}

// ============================================================================
// 测试 2: photscal=1.0 → 像素值不变
// ============================================================================
static void test_photscal_1x() {
    const int W = 4, H = 4;
    std::vector<float> light(W * H);
    std::vector<float> out(W * H);
    for (int i = 0; i < W * H; i++) light[i] = (float)(i * 0.5f);

    int rc = calibration::apply_photometry(light.data(), W, H, 1.0, out.data());
    ASSERT_TRUE(rc == 0, "photscal=1.0: 返回值=0 (成功)");

    bool allOk = true;
    for (int i = 0; i < W * H; i++) {
        if (std::fabs(out[i] - light[i]) > 1e-6f) { allOk = false; break; }
    }
    ASSERT_TRUE(allOk, "photscal=1.0: 像素值不变");
}

// ============================================================================
// 测试 3: photscal=0.0 → 所有像素为 0
// ============================================================================
static void test_photscal_0() {
    const int W = 4, H = 4;
    std::vector<float> light(W * H);
    std::vector<float> out(W * H);
    for (int i = 0; i < W * H; i++) light[i] = (float)(i + 1);  // 非零值

    int rc = calibration::apply_photometry(light.data(), W, H, 0.0, out.data());
    ASSERT_TRUE(rc == 0, "photscal=0.0: 返回值=0 (成功)");

    bool allZero = true;
    for (int i = 0; i < W * H; i++) {
        if (out[i] != 0.0f) { allZero = false; break; }
    }
    ASSERT_TRUE(allZero, "photscal=0.0: 所有像素为 0");
}

// ============================================================================
// 测试 4: photscal=0.5 → 像素值减半
// ============================================================================
static void test_photscal_half() {
    const int W = 8, H = 8;
    std::vector<float> light(W * H);
    std::vector<float> out(W * H);
    for (int i = 0; i < W * H; i++) light[i] = (float)i;

    int rc = calibration::apply_photometry(light.data(), W, H, 0.5, out.data());
    ASSERT_TRUE(rc == 0, "photscal=0.5: 返回值=0 (成功)");

    bool allOk = true;
    for (int i = 0; i < W * H; i++) {
        if (std::fabs(out[i] - (float)i * 0.5f) > 1e-5f) { allOk = false; break; }
    }
    ASSERT_TRUE(allOk, "photscal=0.5: 像素值减半");
}

// ============================================================================
// 测试 5: 参数校验 - nullptr / 非法尺寸 / NaN photscal
// ============================================================================
static void test_invalid_args() {
    std::vector<float> light(4);
    std::vector<float> out(4);

    int rc1 = calibration::apply_photometry(nullptr, 2, 2, 1.0, out.data());
    ASSERT_TRUE(rc1 < 0, "参数校验: light=nullptr 返回错误");

    int rc2 = calibration::apply_photometry(light.data(), 2, 2, 1.0, nullptr);
    ASSERT_TRUE(rc2 < 0, "参数校验: out=nullptr 返回错误");

    int rc3 = calibration::apply_photometry(light.data(), 0, 2, 1.0, out.data());
    ASSERT_TRUE(rc3 < 0, "参数校验: w=0 返回错误");

    int rc4 = calibration::apply_photometry(light.data(), 2, 0, 1.0, out.data());
    ASSERT_TRUE(rc4 < 0, "参数校验: h=0 返回错误");

    int rc5 = calibration::apply_photometry(light.data(), 2, 2, NAN, out.data());
    ASSERT_TRUE(rc5 < 0, "参数校验: photscal=NaN 返回错误");

    int rc6 = calibration::apply_photometry(light.data(), 2, 2, INFINITY, out.data());
    ASSERT_TRUE(rc6 < 0, "参数校验: photscal=Inf 返回错误");
}

// ============================================================================
// 测试 5b: in-place 操作 (light == out)
// ============================================================================
static void test_inplace() {
    const int W = 4, H = 4;
    std::vector<float> buf(W * H);
    for (int i = 0; i < W * H; i++) buf[i] = (float)(i + 1);

    int rc = calibration::apply_photometry(buf.data(), W, H, 3.0, buf.data());
    ASSERT_TRUE(rc == 0, "in-place: 返回值=0 (成功)");

    bool allOk = true;
    for (int i = 0; i < W * H; i++) {
        if (std::fabs(buf[i] - (float)(i + 1) * 3.0f) > 1e-5f) { allOk = false; break; }
    }
    ASSERT_TRUE(allOk, "in-place: 像素值正确×3.0");
}

// ============================================================================
// 测试 5c: NaN/Inf 像素透传 (下游 Drizzle 会跳过)
// ============================================================================
static void test_nan_inf_passthrough() {
    const int W = 4, H = 1;
    std::vector<float> light = { 1.0f, NAN, INFINITY, -INFINITY };
    std::vector<float> out(W * H);

    int rc = calibration::apply_photometry(light.data(), W, H, 2.0, out.data());
    ASSERT_TRUE(rc == 0, "NaN/Inf 透传: 返回值=0 (成功)");

    // 正常像素 ×2
    ASSERT_FLOAT_NEAR(out[0], 2.0f, 1e-6f, "NaN/Inf 透传: 正常像素×2.0");

    // NaN 仍为 NaN
    ASSERT_TRUE(std::isnan(out[1]), "NaN/Inf 透传: NaN 像素仍为 NaN");

    // Inf 仍为 Inf (2.0 * Inf = Inf)
    ASSERT_TRUE(std::isinf(out[2]), "NaN/Inf 透传: +Inf 像素仍为 Inf");

    // -Inf 仍为 -Inf (2.0 * -Inf = -Inf)
    ASSERT_TRUE(std::isinf(out[3]) && out[3] < 0.0f, "NaN/Inf 透传: -Inf 像素仍为 -Inf");
}

// ============================================================================
// 测试 5d: 大动态范围 photscal (验证 double 精度计算)
// photscal=1e-7, light=1e6 → out=0.1 (float 直接乘会损失精度)
// ============================================================================
static void test_large_dynamic_range() {
    const int W = 1, H = 1;
    float light = 1.0e6f;
    float out = 0.0f;

    int rc = calibration::apply_photometry(&light, W, H, 1e-7, &out);
    ASSERT_TRUE(rc == 0, "大动态范围: 返回值=0 (成功)");

    // 1e6 * 1e-7 = 0.1
    // double 精度: (double)1e6f * 1e-7 = 0.1 (精确)
    // float 精度: 1e6f * 1e-7f ≈ 0.1 (可能有误差)
    ASSERT_FLOAT_NEAR(out, 0.1f, 1e-5f, "大动态范围: 1e6 * 1e-7 = 0.1 (double 精度)");
}

// ============================================================================
// 以下是 Writer 元数据一致性校验测试
//
// 策略: 直接调用真实 hiss::HissWriter::open() 验证元数据一致性校验逻辑
// (链接 ../../astro_image_io/src/hiss_writer.cpp 等真实生产代码)
// ============================================================================

// 测试 6: Writer: apply_photometry=true + BUNIT=ASTROCS_RELATIVE_FLUX → 成功
static void test_writer_photappl_true_relative_flux() {
    hiss::HissGridSpec grid;
    grid.nside = 16;
    grid.tile_nside = 16;
    grid.ordering = 1;  // NESTED
    grid.radesys = 0;   // ICRS
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = 16;
    meta.tile_nside = 16;
    meta.ordering = 1;
    meta.radesys = 0;
    meta.pixfrac = 1.0;
    meta.photappl = 1;
    meta.photscal = 1.234e-5;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    hiss::HissWriter writer;
    std::string tmp_path = "test_photometry_apply_tmp_partial.hiss";
    int rc = writer.open(tmp_path, grid, meta);
    ASSERT_TRUE(rc == 0, "Writer: apply_photometry=true + BUNIT=ASTROCS_RELATIVE_FLUX → open 成功");
    writer.cancel();  // 清理临时文件
    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);
    std::filesystem::remove(tmp_path + ".partial", ec);
}

// 测试 7: Writer: apply_photometry=false + BUNIT=ASTROCS_RELATIVE_FLUX → 拒绝
static void test_writer_photappl_false_relative_flux() {
    hiss::HissGridSpec grid;
    grid.nside = 16;
    grid.tile_nside = 16;
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = 16;
    meta.tile_nside = 16;
    meta.ordering = 1;
    meta.radesys = 0;
    meta.pixfrac = 1.0;
    meta.photappl = 0;  // apply_photometry=false
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ASTROCS_RELATIVE_FLUX");

    hiss::HissWriter writer;
    std::string tmp_path = "test_photometry_apply_tmp_reject.hiss";
    int rc = writer.open(tmp_path, grid, meta);
    ASSERT_TRUE(rc == -2, "Writer: apply_photometry=false + BUNIT=ASTROCS_RELATIVE_FLUX → open 返回 -2 (拒绝)");
    // open 失败时不创建 .partial, 无需清理
}

// 测试 8: Writer: apply_photometry=false + BUNIT=ADU → 成功 (允许非测光数据)
static void test_writer_photappl_false_adu() {
    hiss::HissGridSpec grid;
    grid.nside = 16;
    grid.tile_nside = 16;
    grid.ordering = 1;
    grid.radesys = 0;
    grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = 16;
    meta.tile_nside = 16;
    meta.ordering = 1;
    meta.radesys = 0;
    meta.pixfrac = 1.0;
    meta.photappl = 0;
    meta.photscal = 1.0;
    std::snprintf(meta.bunit, sizeof(meta.bunit), "ADU");

    hiss::HissWriter writer;
    std::string tmp_path = "test_photometry_apply_tmp_adu.hiss";
    int rc = writer.open(tmp_path, grid, meta);
    ASSERT_TRUE(rc == 0, "Writer: apply_photometry=false + BUNIT=ADU → open 成功 (允许非测光数据)");
    writer.cancel();
    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);
    std::filesystem::remove(tmp_path + ".partial", ec);
}

// ============================================================================
// main: 运行所有测试并输出结果
// ============================================================================
int main() {
    printf("=== Photometry Apply 单元测试 (WP-C 步骤9) ===\n\n");

    printf("--- 测光应用模块 (calibration::apply_photometry) ---\n");
    test_photscal_2x();
    test_photscal_1x();
    test_photscal_0();
    test_photscal_half();
    test_invalid_args();
    test_inplace();
    test_nan_inf_passthrough();
    test_large_dynamic_range();

    printf("\n--- Writer 元数据一致性校验 (hiss::HissWriter::open) ---\n");
    test_writer_photappl_true_relative_flux();
    test_writer_photappl_false_relative_flux();
    test_writer_photappl_false_adu();

    printf("\n=== 测试结果 ===\n");
    printf("通过: %d\n", g_pass);
    printf("失败: %d\n", g_fail);
    printf("总计: %d\n", g_pass + g_fail);

    printf("\n(Writer 元数据一致性校验直接调用真实 hiss::HissWriter::open())\n");

    return (g_fail == 0) ? 0 : 1;
}

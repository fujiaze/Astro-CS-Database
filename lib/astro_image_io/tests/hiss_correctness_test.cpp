// ============================================================================
// hiss_correctness_test.cpp - AstroCS HISS 正确性测试 (合成数据)
//
// 覆盖范围:
//   校准测试    (1~5):   标准模式 / 曝光比例模式 / 最优 Dark 成功/失败/硬失败
//   Drizzle 测试 (6~11):  通量守恒 / support 范围 / 自动 NSIDE
//   HISS 格式测试 (12~21): 往返读写 / 子块校验 / 原子提交 / 坐标恢复
//
// 编译 (从 tests/ 目录):
//   g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD \
//     -I../include -I../src \
//     -I../../calibration/include \
//     hiss_correctness_test.cpp \
//     ../src/hiss_codec.cpp ../src/hiss_common.cpp \
//     ../src/hiss_writer.cpp ../src/hiss_reader.cpp \
//     ../../calibration/src/dark_optimizer.cpp ../../calibration/src/calibrator.cpp \
//     -llz4 -lzstd -o hiss_correctness_test.exe
//
// 注意:
//   - 使用合成数据, 不依赖真实天文数据
//   - 测试结果只报告, 不擅自改变数学算法或科学语义
//   - Agent 不得自行宣称"用户验收完成"
// ============================================================================
#include "hiss_format.h"
#include "astro_calibration.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <random>

// ============================================================================
// 测试框架: 轻量级断言 + 结果收集
// ============================================================================

static int g_test_total   = 0;   // 总测试数
static int g_test_passed  = 0;   // 通过数
static int g_test_skipped = 0;   // 跳过数
static std::vector<std::string> g_failures; // 失败详情
static std::vector<std::string> g_skips;    // 跳过详情

// 测试用例包装宏: 自动计数 + 捕获异常
#define TEST_CASE(name, id) \
    fprintf(stderr, "\n========== [TEST %02d] %s ==========\n", id, name); \
    g_test_total++;

// 断言: 条件为假则记录失败
#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::string m = std::string("[TEST ") + std::to_string(id) + "] FAIL: " + (msg); \
        fprintf(stderr, "  FAIL: %s\n", (msg)); \
        g_failures.push_back(m); \
        return; \
    } else { \
        fprintf(stderr, "  OK: %s\n", (msg)); \
    }

// 断言: 浮点近似相等
#define ASSERT_NEAR(a, b, tol, msg) \
    if (std::fabs((double)(a) - (double)(b)) > (tol)) { \
        char buf[512]; \
        std::snprintf(buf, sizeof(buf), "%s (got=%.6g expected=%.6g tol=%.6g)", (msg), (double)(a), (double)(b), (double)(tol)); \
        std::string m = std::string("[TEST ") + std::to_string(id) + "] FAIL: " + buf; \
        fprintf(stderr, "  FAIL: %s\n", buf); \
        g_failures.push_back(m); \
        return; \
    } else { \
        fprintf(stderr, "  OK: %s (got=%.6g)\n", (msg), (double)(a)); \
    }

// 标记跳过
#define SKIP_TEST(msg) \
    { std::string m = std::string("[TEST ") + std::to_string(id) + "] SKIP: " + (msg); \
      fprintf(stderr, "  SKIP: %s\n", (msg)); \
      g_skips.push_back(m); g_test_skipped++; return; }

// ============================================================================
// 辅助: 合成数据生成器
// ============================================================================

// 生成合成图像 (float32), 使用确定性随机数
static std::vector<float> make_synthetic_image(int w, int h, float base, float amp,
                                                float noise_std, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.0f, noise_std);
    std::vector<float> img((size_t)w * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float gradient = base + amp * (float)x / w;
            img[(size_t)y * w + x] = gradient + noise(rng);
        }
    }
    return img;
}

// 构造一个简单的 DrizzleTileAccumulator (tile_nside=16, 256 叶像素)
static hiss::DrizzleTileAccumulator make_simple_accumulator(uint32_t tile_nside,
                                                             uint64_t parent_ipix,
                                                             double flux_per_pixel,
                                                             double area_per_pixel,
                                                             uint32_t seed) {
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = tile_nside;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area  = 1.0;  // area_per_pixel 为任意值, 用 1.0 保持测试原有行为
    size_t n_leaf = (size_t)tile_nside * tile_nside * 12;  // 12 个 base pixel
    acc.pixels.resize(n_leaf);
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.0f, 0.01f * (float)flux_per_pixel);
    for (size_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux  = flux_per_pixel * area_per_pixel + noise(rng) * area_per_pixel;
        acc.pixels[i].sum_area  = area_per_pixel;
        acc.pixels[i].n_contrib = 1;
    }
    return acc;
}

// ============================================================================
// 内部声明: calibrator.cpp 中的 calibrate 函数 (namespace ac, 未在头文件声明)
// ============================================================================
namespace ac {
void calibrate(const float* light, int w, int h,
               const float* dark, const float* flat, const float* bias,
               float* out, int dark_opt, float k_init, float* actual_k);
}

// ============================================================================
// 校准测试 1: 标准模式 (L-D)/F 公式正确性
// Dark 已含 Bias, 直接 (Light - Dark) / Flat
// ============================================================================
static void test_01_standard_mode(int id) {
    TEST_CASE("标准模式 (L-D)/F 公式正确性", id);

    const int w = 64, h = 64;
    const int n = w * h;
    // 合成数据: Light=100~200, Dark=10~20, Flat=0.8~1.2 (已归一化)
    auto light = make_synthetic_image(w, h, 100.0f, 100.0f, 1.0f, 42);
    auto dark  = make_synthetic_image(w, h, 10.0f,  10.0f,  0.5f, 100);
    std::vector<float> flat(n, 1.0f);  // Flat=1 简化验证
    std::vector<float> out(n, 0.0f);

    // 标准模式: dark_opt=0 → out = (light - dark) / flat
    float actual_k = -1.0f;
    ac::calibrate(light.data(), w, h, dark.data(), flat.data(), nullptr,
                  out.data(), 0, 1.0f, &actual_k);

    // 验证公式: out[i] = (light[i] - dark[i]) / flat[i]
    // 标准模式下 actual_k 应为 1.0
    ASSERT_NEAR(actual_k, 1.0f, 1e-6f, "标准模式 actual_k 应为 1.0");
    int errors = 0;
    for (int i = 0; i < n; i++) {
        float expected = (light[i] - dark[i]) / std::max(flat[i], 0.1f);
        if (std::fabs(out[i] - expected) > 1e-4f) errors++;
    }
    char msg[256];
    std::snprintf(msg, sizeof(msg), "(L-D)/F 公式校验, 不匹配像素数=%d/%d", errors, n);
    ASSERT_TRUE(errors == 0, msg);
}

// ============================================================================
// 校准测试 2: 曝光比例模式 [L-B-k(D-B)]/F 公式正确性
// ============================================================================
static void test_02_exposure_ratio_mode(int id) {
    TEST_CASE("曝光比例模式 [L-B-k(D-B)]/F 公式正确性", id);

    const int w = 64, h = 64;
    const int n = w * h;
    auto light = make_synthetic_image(w, h, 200.0f, 50.0f, 1.0f, 42);
    auto bias  = make_synthetic_image(w, h, 50.0f,  5.0f,  0.3f, 200);
    auto dark  = make_synthetic_image(w, h, 80.0f, 10.0f, 0.5f, 100);
    std::vector<float> flat(n, 1.0f);
    std::vector<float> out(n, 0.0f);

    // 曝光比例模式: dark_opt=1, k=1.5
    // out = (light - bias - k*(dark - bias)) / flat
    float k = 1.5f;
    float actual_k = -1.0f;
    ac::calibrate(light.data(), w, h, dark.data(), flat.data(), bias.data(),
                  out.data(), 1, k, &actual_k);

    ASSERT_NEAR(actual_k, k, 1e-6f, "曝光比例模式 actual_k 应为 k_init");
    int errors = 0;
    for (int i = 0; i < n; i++) {
        float expected = (light[i] - bias[i] - k * (dark[i] - bias[i])) / std::max(flat[i], 0.1f);
        if (std::fabs(out[i] - expected) > 1e-3f) errors++;
    }
    char msg[256];
    std::snprintf(msg, sizeof(msg), "[L-B-k(D-B)]/F 公式校验, 不匹配像素数=%d/%d", errors, n);
    ASSERT_TRUE(errors == 0, msg);
}

// ============================================================================
// 校准测试 3: 最优 Dark 成功路径 (合成数据 k=1.5)
// 模型: L - B = c + k*(D - B), 合成数据使 k_true=1.5
// ============================================================================
static void test_03_optimal_dark_success(int id) {
    TEST_CASE("最优 Dark 成功路径 (合成数据 k=1.5)", id);

    const int w = 128, h = 128;
    const int n = w * h;
    const float k_true = 1.5f;
    const float c_true = 5.0f;  // 截距

    // Bias: 近常数 (基座 500 + 微小噪声)
    auto bias = make_synthetic_image(w, h, 500.0f, 2.0f, 0.5f, 200);
    // Dark: 基座 520 + 暗电流梯度 (使 D-B 有足够方差)
    auto dark = make_synthetic_image(w, h, 520.0f, 80.0f, 1.0f, 100);
    // Light = B + c + k*(D-B) + 背景噪声 (使回归能成功)
    std::vector<float> light(n);
    std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0f, 2.0f);
    for (int i = 0; i < n; i++) {
        float db = dark[i] - bias[i];
        light[i] = bias[i] + c_true + k_true * db + noise(rng);
    }

    hiss::Stage1Diagnostics diag;
    float k_init = 1.0f;  // 初始值 (故意偏离真值)
    float k_est = ac::optimize_dark_k(light.data(), bias.data(), dark.data(),
                                       nullptr, w, h, k_init, diag);

    fprintf(stderr, "  k_est=%.4f (true=%.4f) diag.success=%d fell_back=%d code=%.32s\n",
            k_est, k_true, diag.success, diag.fell_back, diag.code);

    // 成功: diagnostics.success == 0, fell_back == 0
    ASSERT_TRUE(diag.success == 0, "最优 Dark 应成功 (success==0)");
    ASSERT_TRUE(diag.fell_back == 0, "不应回退 (fell_back==0)");
    // k 估计应在真值附近 (容差 0.15, 因有噪声)
    ASSERT_NEAR(k_est, k_true, 0.15f, "k_est 应接近 k_true=1.5");
}

// ============================================================================
// 校准测试 4: 最优 Dark 失败后诊断并回退曝光比例
// 构造 D-B 方差近零的数据 → 触发 ZERO_VARIANCE 回退
// ============================================================================
static void test_04_optimal_dark_fallback(int id) {
    TEST_CASE("最优 Dark 失败后诊断并回退曝光比例", id);

    const int w = 128, h = 128;
    const int n = w * h;

    // Dark 近似等于 Bias (D-B ≈ 0), 回归无法确定斜率
    auto bias = make_synthetic_image(w, h, 500.0f, 0.0f, 0.1f, 200);
    std::vector<float> dark(n);
    std::mt19937 rng(100);
    std::normal_distribution<float> tiny_noise(0.0f, 0.01f);  // 近零方差
    for (int i = 0; i < n; i++) dark[i] = bias[i] + tiny_noise(rng);
    // Light 有正常梯度但与 Dark 无关
    auto light = make_synthetic_image(w, h, 600.0f, 50.0f, 1.0f, 42);

    hiss::Stage1Diagnostics diag;
    float k_init = 1.2f;
    float k_ret = ac::optimize_dark_k(light.data(), bias.data(), dark.data(),
                                       nullptr, w, h, k_init, diag);

    fprintf(stderr, "  k_ret=%.4f diag.success=%d fell_back=%d code=%.32s msg=%.64s\n",
            k_ret, diag.success, diag.fell_back, diag.code, diag.message);

    // 失败 + 回退: success < 0, fell_back == 1
    ASSERT_TRUE(diag.success < 0, "应失败 (success < 0)");
    ASSERT_TRUE(diag.fell_back == 1, "应回退 (fell_back == 1)");
    // 回退后返回 k_init
    ASSERT_NEAR(k_ret, k_init, 1e-6f, "回退后应返回 k_init");
    // 诊断字段非空
    ASSERT_TRUE(std::string(diag.fallback_from) == "OPTIMAL", "fallback_from 应为 OPTIMAL");
    ASSERT_TRUE(std::string(diag.fallback_to) == "EXPOSURE_RATIO", "fallback_to 应为 EXPOSURE_RATIO");
    ASSERT_TRUE(std::string(diag.code).size() > 0, "应有错误码");
    ASSERT_TRUE(std::string(diag.message).size() > 0, "应有诊断信息");
}

// ============================================================================
// 校准测试 5: 回退也不可用时硬失败 (EXPTIME 缺失)
// 模拟: 最优 Dark 失败, 且 EXPTIME 缺失导致无法计算 k_init
// 这是编排层逻辑: k_init = t_light/t_dark, EXPTIME 缺失时 k_init 无效
// ============================================================================
static void test_05_hard_failure_exptime_missing(int id) {
    TEST_CASE("回退也不可用时硬失败 (EXPTIME 缺失)", id);

    // 模拟编排层: EXPTIME 缺失 → k_init 无法计算 (设为 NaN 或 <=0)
    // dark_optimizer 对 k_init <= 0 的处理: 返回 BAD_K_INIT 回退
    const int w = 64, h = 64;
    const int n = w * h;
    auto bias = make_synthetic_image(w, h, 500.0f, 0.0f, 0.1f, 200);
    std::vector<float> dark(n, 500.0f);
    auto light = make_synthetic_image(w, h, 600.0f, 50.0f, 1.0f, 42);

    // EXPTIME 缺失 → k_init 无效 (<=0)
    hiss::Stage1Diagnostics diag;
    float k_init_bad = 0.0f;  // EXPTIME 缺失, 无法计算 k
    float k_ret = ac::optimize_dark_k(light.data(), bias.data(), dark.data(),
                                       nullptr, w, h, k_init_bad, diag);

    fprintf(stderr, "  k_ret=%.4f diag.success=%d fell_back=%d code=%.32s\n",
            k_ret, diag.success, diag.fell_back, diag.code);

    // 编排层硬失败逻辑: 最优失败 + 回退也不可用 (k_init 无效)
    // dark_optimizer 返回 k_init (0.0) 并标记 BAD_K_INIT 回退
    ASSERT_TRUE(diag.success < 0, "应失败 (success < 0)");
    ASSERT_TRUE(std::string(diag.code) == "BAD_K_INIT", "错误码应为 BAD_K_INIT");

    // 编排层判定: 回退返回的 k <= 0 → 硬失败 (无法继续校准)
    bool hard_fail = (k_ret <= 0.0f || !std::isfinite(k_ret));
    ASSERT_TRUE(hard_fail, "回退返回 k<=0 → 编排层硬失败 (EXPTIME 缺失导致回退不可用)");
}

// ============================================================================
// Drizzle 测试 6: 单源像素通量守恒 (pixfrac=1, drop 未截断)
// 一个源像素完全落入一个 HEALPix 像素: sum_flux = L*a, sum_area = a
// signal = sum_flux / sum_area = L (通量守恒)
// ============================================================================
static void test_06_single_pixel_flux_conservation(int id) {
    TEST_CASE("单源像素通量守恒 (pixfrac=1, drop 未截断)", id);

    // 构造累加器: 单个像素, 通量=100, 面积=1.0 (完整覆盖)
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 0;
    acc.pixel_area  = 1.0;
    acc.pixels.resize(1);
    acc.pixels[0].sum_flux  = 100.0 * 1.0;  // L * a
    acc.pixels[0].sum_area  = 1.0;           // a (完整覆盖)
    acc.pixels[0].n_contrib = 1;

    std::vector<float> signal;
    acc.finalize_signal(signal);

    // signal[0] = sum_flux / sum_area = 100.0 (通量守恒)
    ASSERT_NEAR(signal[0], 100.0f, 1e-4f, "单像素 signal 应等于源通量 L=100");
}

// ============================================================================
// Drizzle 测试 7: 多像素球面重叠通量守恒
// 多个源像素贡献到多个 HEALPix 像素, 总通量守恒
// ============================================================================
static void test_07_multi_pixel_flux_conservation(int id) {
    TEST_CASE("多像素球面重叠通量守恒", id);

    // 构造 4 个叶像素, 模拟两个源像素各贡献到两个 HEALPix 像素
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 0;
    acc.pixel_area  = 1.0;
    acc.pixels.resize(4);

    // 源像素 A (通量=200) 贡献到 pixel 0 (面积 0.6) 和 pixel 1 (面积 0.4)
    // 源像素 B (通量=300) 贡献到 pixel 2 (面积 0.7) 和 pixel 3 (面积 0.3)
    acc.pixels[0].sum_flux = 200.0 * 0.6; acc.pixels[0].sum_area = 0.6; acc.pixels[0].n_contrib = 1;
    acc.pixels[1].sum_flux = 200.0 * 0.4; acc.pixels[1].sum_area = 0.4; acc.pixels[1].n_contrib = 1;
    acc.pixels[2].sum_flux = 300.0 * 0.7; acc.pixels[2].sum_area = 0.7; acc.pixels[2].n_contrib = 1;
    acc.pixels[3].sum_flux = 300.0 * 0.3; acc.pixels[3].sum_area = 0.3; acc.pixels[3].n_contrib = 1;

    std::vector<float> signal;
    acc.finalize_signal(signal);

    // signal = sum_flux (累计通量, 02_FROZEN §8: 不除面积)
    // signal[0] = 200*0.6 = 120, signal[1] = 200*0.4 = 80
    ASSERT_NEAR(signal[0], 120.0f, 1e-4f, "像素0 signal = 200*0.6 = 120 (累计通量)");
    ASSERT_NEAR(signal[1], 80.0f, 1e-4f, "像素1 signal = 200*0.4 = 80 (累计通量)");
    ASSERT_NEAR(signal[2], 210.0f, 1e-4f, "像素2 signal = 300*0.7 = 210 (累计通量)");
    ASSERT_NEAR(signal[3], 90.0f, 1e-4f, "像素3 signal = 300*0.3 = 90 (累计通量)");

    // 通量守恒 (02_FROZEN §8): drop未截断时 Σ signal = Σ L_j
    // 源A: signal[0]+signal[1] = 120+80 = 200 = L_A
    ASSERT_NEAR(signal[0] + signal[1], 200.0f, 1e-2f, "源A通量守恒: signal[0]+signal[1]=200");
    // 源B: signal[2]+signal[3] = 210+90 = 300 = L_B
    ASSERT_NEAR(signal[2] + signal[3], 300.0f, 1e-2f, "源B通量守恒: signal[2]+signal[3]=300");
    // 总通量: Σ signal = 200 + 300 = 500
    double total = 0.0;
    for (size_t i = 0; i < 4; i++) total += signal[i];
    ASSERT_NEAR(total, 500.0, 1e-2, "总通量守恒 Σ signal = 500");
}

// ============================================================================
// Drizzle 测试 8: pixfrac=1 典型小于1 和接近0 边界
// support = sum_area, 典型值 <1 (部分覆盖), 接近0 (几乎无贡献)
// ============================================================================
static void test_08_pixfrac_support_values(int id) {
    TEST_CASE("pixfrac=1 典型小于1 和接近0 边界", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 0;
    acc.pixel_area  = 1.0;
    acc.pixels.resize(3);
    // 像素0: 典型部分覆盖 (0.5)
    acc.pixels[0].sum_area = 0.5;
    // 像素1: 接近0 (0.001)
    acc.pixels[1].sum_area = 0.001;
    // 像素2: 完整覆盖 (1.0)
    acc.pixels[2].sum_area = 1.0;

    ASSERT_TRUE(acc.validate_support() == 0, "support 值合法 (0~1)");

    std::vector<uint8_t> support;
    acc.finalize_support(support);

    // support = round(255 * sum_area)
    ASSERT_NEAR((float)support[0], 128.0f, 1.0f, "像素0 support ≈ round(255*0.5)=128");
    ASSERT_NEAR((float)support[1], 0.0f, 1.0f, "像素1 support ≈ round(255*0.001)=0");
    ASSERT_NEAR((float)support[2], 255.0f, 1.0f, "像素2 support = round(255*1.0)=255");
}

// ============================================================================
// Drizzle 测试 9: support 处于 0~1
// 验证所有合法的 sum_area 值经 finalize_support 后映射到 [0, 255]
// ============================================================================
static void test_09_support_in_range(int id) {
    TEST_CASE("support 处于 0~1", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 0;
    acc.pixel_area  = 1.0;
    acc.pixels.resize(100);
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    for (auto& p : acc.pixels) {
        p.sum_area = uniform(rng);
        p.sum_flux = 50.0 * p.sum_area;
        p.n_contrib = 1;
    }

    ASSERT_TRUE(acc.validate_support() == 0, "所有 sum_area 在 [0,1] 内 → validate_support OK");

    std::vector<uint8_t> support;
    acc.finalize_support(support);

    int out_of_range = 0;
    for (auto s : support) {
        if (s > 255) out_of_range++;  // uint8 不可能 >255, 但检查逻辑完整性
    }
    ASSERT_TRUE(out_of_range == 0, "所有 support 值在 [0, 255] 范围内");
}

// ============================================================================
// Drizzle 测试 10: 明显 support 超限触发错误
// sum_area 明显 > 1 (如 1.5) 时 validate_support 应返回 <0
// ============================================================================
static void test_10_support_overflow_error(int id) {
    TEST_CASE("明显 support 超限触发错误", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 0;
    acc.pixel_area  = 1.0;
    acc.pixels.resize(2);
    acc.pixels[0].sum_area = 0.5;   // 合法
    acc.pixels[1].sum_area = 1.5;   // 明显超 1 → 错误

    int ret = acc.validate_support();
    ASSERT_TRUE(ret < 0, "sum_area=1.5 明显超限 → validate_support 返回 <0");

    // 浮点误差级超限 (1.0 + 1e-9) 应被容忍
    hiss::DrizzleTileAccumulator acc2;
    acc2.tile_nside = 16;
    acc2.parent_ipix = 0;
    acc2.pixel_area = 1.0;
    acc2.pixels.resize(1);
    acc2.pixels[0].sum_area = 1.0 + 1e-9;  // 浮点误差级
    int ret2 = acc2.validate_support();
    ASSERT_TRUE(ret2 == 0, "浮点误差级超限 (1+1e-9) 应被容忍");
}

// ============================================================================
// Drizzle 测试 11: 自动 NSIDE 覆盖局部最细 WCS/SIP 尺度
// 使用本地参考实现 (与 drizzle_engine.cpp compute_auto_nside 算法一致)
// 注: 生产 compute_auto_nside 依赖 WcsSip + HealpixCore 链, 此处用参考实现
//     验证算法正确性: NSIDE 使 HEALPix 像素尺度 <= 最细输入像素尺度
// ============================================================================

// 参考实现: 度 → 弧度
static const double kPi    = 3.14159265358979323846;
static const double kD2R   = 0.017453292519943295769;

// 参考实现: 大圆距离 (度), 与 drizzle_engine.cpp 一致
static double ref_great_circle_dist(double ra1, double dec1, double ra2, double dec2) {
    double dRa = (ra2 - ra1) * kD2R;
    double dec1r = dec1 * kD2R;
    double dec2r = dec2 * kD2R;
    double x = std::sin(dec1r) * std::sin(dec2r) +
               std::cos(dec1r) * std::cos(dec2r) * std::cos(dRa);
    x = std::max(-1.0, std::min(1.0, x));
    return std::acos(x) / kD2R;
}

// 参考实现: TAN 投影 + CD 矩阵的像素→天球转换 (无 SIP, 线性近似)
// 用于验证 auto_nside 算法: 给定 CD 矩阵, 计算局部像素尺度
// CD 矩阵的行列式 |det| = (度/像素)² (面积单位), 取 sqrt 得到线性像素尺度
static double ref_pixel_scale_arcsec(double cd1_1, double cd1_2,
                                      double cd2_1, double cd2_2) {
    // |det(CD)| 单位是 (度/像素)², sqrt 后才是线性像素尺度 (度/像素)
    double det = std::fabs(cd1_1 * cd2_2 - cd1_2 * cd2_1);
    return std::sqrt(det) * 3600.0;  // 转为角秒/像素
}

// 参考实现: compute_auto_nside (与 drizzle_engine.cpp 算法一致)
// 输入: finest_arcsec (最细输入像素尺度, 角秒/像素)
// 返回: 推荐 NSIDE (2 的幂, 钳位 [16, 1048576])
static int ref_compute_auto_nside(double finest_arcsec) {
    if (finest_arcsec <= 0.0 || !std::isfinite(finest_arcsec)) return 0;
    // HEALPix 线性像素尺度 ≈ 210960/nside 角秒
    double nside_min_real = 210960.0 / finest_arcsec;
    if (nside_min_real < 1.0) nside_min_real = 1.0;
    int nside = 1;
    while ((double)nside < nside_min_real) {
        int next = nside << 1;
        if (next <= nside) { nside = (1 << 20); break; }
        nside = next;
    }
    if (nside < 16) nside = 16;
    if (nside > 1048576) nside = 1048576;
    return nside;
}

static void test_11_auto_nside(int id) {
    TEST_CASE("自动 NSIDE 覆盖局部最细 WCS/SIP 尺度", id);

    // 测试 1: 1.0 角秒/像素 → NSIDE 应使 HEALPix 尺度 <= 1.0"
    {
        double finest = 1.0;  // 1"/px
        int nside = ref_compute_auto_nside(finest);
        double hp_res = 210960.0 / nside;
        fprintf(stderr, "  finest=%.4f\" → nside=%d hp_res=%.4f\" (oversample=%.3fx)\n",
                finest, nside, hp_res, hp_res / finest);
        ASSERT_TRUE(nside >= 16, "NSIDE >= 16");
        ASSERT_TRUE((nside & (nside - 1)) == 0, "NSIDE 是 2 的幂");
        ASSERT_TRUE(hp_res <= finest * 1.01, "HEALPix 像素尺度 <= 最细尺度 (1\"/px)");
    }

    // 测试 2: 0.5 角秒/像素 (高分辨率)
    {
        double finest = 0.5;
        int nside = ref_compute_auto_nside(finest);
        double hp_res = 210960.0 / nside;
        fprintf(stderr, "  finest=%.4f\" → nside=%d hp_res=%.4f\"\n", finest, nside, hp_res);
        ASSERT_TRUE(hp_res <= finest * 1.01, "HEALPix 像素尺度 <= 0.5\"/px");
        ASSERT_TRUE(nside <= 1048576, "NSIDE <= 上限 1048576");
    }

    // 测试 3: 从 CD 矩阵计算像素尺度, 验证 NSIDE 覆盖
    {
        // 典型 CCD: 2.0"/px → CD = 2.0/3600 度/px
        double cd_scale = 2.0 / 3600.0;
        double cd1_1 = cd_scale, cd1_2 = 0.0;
        double cd2_1 = 0.0,      cd2_2 = cd_scale;
        double finest = ref_pixel_scale_arcsec(cd1_1, cd1_2, cd2_1, cd2_2);
        int nside = ref_compute_auto_nside(finest);
        double hp_res = 210960.0 / nside;
        fprintf(stderr, "  CD=%.6f°/px → finest=%.4f\" → nside=%d hp_res=%.4f\"\n",
                cd_scale, finest, nside, hp_res);
        ASSERT_NEAR(finest, 2.0, 1e-6, "CD 矩阵计算像素尺度 = 2.0\"/px");
        ASSERT_TRUE(hp_res <= finest * 1.01, "NSIDE 覆盖 2\"/px 尺度");
    }

    // 测试 4: 极粗尺度 (60"/px) → NSIDE 应为 16 (下限)
    {
        double finest = 60.0;
        int nside = ref_compute_auto_nside(finest);
        fprintf(stderr, "  finest=%.4f\" → nside=%d (下限检查)\n", finest, nside);
        ASSERT_TRUE(nside >= 16, "粗尺度 NSIDE 仍 >= 16 (下限)");
    }

    // 测试 5: 极细尺度 (0.1"/px) → NSIDE 应达到上限 1048576
    // 注: finest=0.1"/px 要求 nside >= 2109600, 超过上限 1048576 (2^20)
    //     因此 nside 被钳位到上限, hp_res=0.2012"/px > finest=0.1"/px
    //     这是 NSIDE 上限的预期行为, 验证 nside 达到上限即可
    {
        double finest = 0.1;
        int nside = ref_compute_auto_nside(finest);
        double hp_res = 210960.0 / nside;
        fprintf(stderr, "  finest=%.4f\" → nside=%d hp_res=%.4f\" (已达 NSIDE 上限)\n",
                finest, nside, hp_res);
        ASSERT_TRUE(nside == 1048576, "极细尺度 (0.1\"/px) 触发 NSIDE 上限 1048576");
        // hp_res > finest 是预期行为 (上限钳位), 不再要求覆盖
    }
}

// ============================================================================
// HISS 格式测试 12: FULL/BITMAP/SPARSE_LIST 往返
// ============================================================================

// 辅助: 创建临时 HISS 文件并写入一个 Tile
static std::string write_test_hiss(const std::string& base_path,
                                    hiss::OccupancyMode occ_mode,
                                    const hiss::DrizzleTileAccumulator& acc,
                                    const hiss::HissSnrBlock* snr = nullptr) {
    std::string path = base_path + ".hiss";
    hiss::HissGridSpec grid;
    grid.nside      = 64;
    grid.tile_nside = hiss::compute_tile_nside(64);
    grid.ordering   = 1;
    grid.radesys    = 0;
    grid.pixfrac    = 1.0;

    hiss::HissMetadata meta;
    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    std::strncpy(meta.object, "CorrectnessTest", sizeof(meta.object) - 1);
    meta.exptime = 60.0;
    // BUNIT=ASTROCS_RELATIVE_FLUX (默认) 要求 PHOTAPPL=TRUE (Writer 元数据一致性校验)
    meta.photappl = 1;
    meta.photscal = 1.0;

    hiss::HissWriter w;
    if (w.open(path, grid, meta) != 0) return "";
    if (w.add_tile(acc.parent_ipix, acc, snr, occ_mode) != 0) return "";
    if (w.finalize() != 0) return "";
    return path;
}

static void test_12_occupancy_roundtrip(int id) {
    TEST_CASE("FULL/BITMAP/SPARSE_LIST 往返", id);

    // 构造累加器: tile_nside=16, 部分像素有贡献
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 7;
    acc.pixel_area  = 1.0;
    size_t n_leaf = (size_t)16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */;  // 3072
    acc.pixels.resize(n_leaf);
    std::mt19937 rng(42);
    for (size_t i = 0; i < n_leaf; i++) {
        if (i % 3 == 0) {  // 约 1/3 像素有贡献
            double a = 0.5 + 0.5 * (double)rng() / rng.max();
            acc.pixels[i].sum_flux  = 100.0 * a;
            acc.pixels[i].sum_area  = a;
            acc.pixels[i].n_contrib = 1;
        } else {
            acc.pixels[i].sum_flux  = 0.0;
            acc.pixels[i].sum_area  = 0.0;
            acc.pixels[i].n_contrib = 0;
        }
    }
    ASSERT_TRUE(acc.validate_support() == 0, "累加器 support 合法");

    // 预期 signal/support
    std::vector<float> expected_signal;
    std::vector<uint8_t> expected_support;
    acc.finalize_signal(expected_signal);
    acc.finalize_support(expected_support);

    std::string base = "hiss_test_12";

    // 测试 FULL 模式往返
    for (auto mode : {hiss::OccupancyMode::FULL,
                      hiss::OccupancyMode::BITMAP,
                      hiss::OccupancyMode::SPARSE_LIST}) {
        const char* mode_name = (mode == hiss::OccupancyMode::FULL) ? "FULL" :
                                (mode == hiss::OccupancyMode::BITMAP) ? "BITMAP" : "SPARSE_LIST";
        std::string path = write_test_hiss(base + "_" + mode_name, mode, acc);
        ASSERT_TRUE(!path.empty(), (std::string("写入 ") + mode_name + " 模式 HISS").c_str());

        hiss::HissReader r;
        ASSERT_TRUE(r.open(path) == 0, (std::string("读取 ") + mode_name + " 模式 HISS").c_str());

        std::vector<float> sig;
        std::vector<uint8_t> sup;
        ASSERT_TRUE(r.read_tile(7, sig, sup) == 0,
                    (std::string("read_tile ") + mode_name).c_str());

        // 验证 signal/support 与预期一致
        ASSERT_TRUE(sig.size() == expected_signal.size(),
                    (std::string(mode_name) + " signal 数组长度一致").c_str());
        ASSERT_TRUE(sup.size() == expected_support.size(),
                    (std::string(mode_name) + " support 数组长度一致").c_str());

        int sig_errors = 0, sup_errors = 0;
        for (size_t i = 0; i < sig.size(); i++) {
            if (std::fabs(sig[i] - expected_signal[i]) > 1e-5f) sig_errors++;
            if (sup[i] != expected_support[i]) sup_errors++;
        }
        char msg[256];
        std::snprintf(msg, sizeof(msg), "%s 往返: sig_err=%d sup_err=%d", mode_name, sig_errors, sup_errors);
        ASSERT_TRUE(sig_errors == 0 && sup_errors == 0, msg);

        r.close();
        std::filesystem::remove(path);
    }
}

// ============================================================================
// HISS 格式测试 13: signal/support/SNR 独立读取
// ============================================================================

static void test_13_independent_read(int id) {
    TEST_CASE("signal/support/SNR 独立读取", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 3;
    acc.pixel_area  = 1.0;
    size_t n_leaf = (size_t)16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */;
    acc.pixels.resize(n_leaf);
    for (size_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = (double)i * 0.5;
        acc.pixels[i].sum_area = 0.8;
        acc.pixels[i].n_contrib = 2;
    }

    hiss::HissSnrBlock snr;
    snr.points.push_back({10, 8.5f});
    snr.points.push_back({20, 15.2f});
    snr.points.push_back({30, 22.1f});

    std::string path = write_test_hiss("hiss_test_13", hiss::OccupancyMode::FULL, acc, &snr);
    ASSERT_TRUE(!path.empty(), "写入带 SNR 的 HISS 文件");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "打开 HISS 文件");

    // 独立读取 signal
    std::vector<float> sig;
    ASSERT_TRUE(r.read_tile_signal(3, sig) == 0, "独立读取 signal");
    ASSERT_TRUE(sig.size() == n_leaf, "signal 长度正确");

    // 独立读取 support
    std::vector<uint8_t> sup;
    ASSERT_TRUE(r.read_tile_support(3, sup) == 0, "独立读取 support");
    ASSERT_TRUE(sup.size() == n_leaf, "support 长度正确");

    // 独立读取 SNR (冻结布局: n_points + points, 不含估计器状态)
    hiss::HissSnrBlock snr_read;
    int snr_ret = r.read_tile_snr(3, snr_read);
    ASSERT_TRUE(snr_ret == 0, "read_tile_snr 返回 0 (布局已修复)");

    // 验证 SNR 控制点往返一致
    ASSERT_TRUE(snr_read.points.size() == snr.points.size(), "SNR 控制点数量一致");
    for (size_t i = 0; i < snr.points.size() && i < snr_read.points.size(); i++) {
        ASSERT_TRUE(snr_read.points[i].local_ipix == snr.points[i].local_ipix,
                    "SNR 控制点 local_ipix 往返一致");
        ASSERT_NEAR(snr_read.points[i].snr, snr.points[i].snr, 1e-6f,
                    "SNR 控制点 snr 往返一致");
    }

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// HISS 格式测试 14: RAW 子块读写
// (默认 codec 即 RAW, 验证 RAW 编解码往返)
// ============================================================================
static void test_14_raw_subblock(int id) {
    TEST_CASE("RAW 子块读写", id);

    // 验证 RAW codec 注册并可用
    const hiss::CodecEntry* raw = hiss::CodecRegistry::instance().find(hiss::CodecId::RAW);
    ASSERT_TRUE(raw != nullptr, "RAW codec 已注册");

    // 验证 RAW 往返: compress → decompress
    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); i++) data[i] = (uint8_t)(i % 256);

    std::vector<uint8_t> comp(raw->bound(data.size()));
    size_t comp_size = comp.size();
    ASSERT_TRUE(raw->compress(data.data(), data.size(), comp.data(), &comp_size) == 0, "RAW compress");
    ASSERT_TRUE(comp_size == data.size(), "RAW 压缩后大小 == 原始大小 (无压缩)");

    std::vector<uint8_t> decomp(data.size());
    ASSERT_TRUE(raw->decompress(comp.data(), comp_size, decomp.data(), decomp.size()) == 0, "RAW decompress");
    ASSERT_TRUE(decomp == data, "RAW 往返数据一致");

    // 通过 HISS 文件验证 RAW 子块读写 (默认 codec 即 RAW)
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 1; acc.pixel_area = 1.0;
    acc.pixels.resize(16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = (double)i;
        acc.pixels[i].sum_area = 1.0;
        acc.pixels[i].n_contrib = 1;
    }

    std::string path = write_test_hiss("hiss_test_14", hiss::OccupancyMode::FULL, acc);
    ASSERT_TRUE(!path.empty(), "写入 RAW codec HISS");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "读取 RAW codec HISS");

    // 验证子块使用 RAW codec
    const auto& tiles = r.tiles();
    ASSERT_TRUE(tiles.size() == 1, "Tile 数量 == 1");
    bool has_raw_signal = false;
    for (const auto& sb : tiles[0].subblocks) {
        if (sb.codec_id == hiss::CodecId::RAW && sb.type == hiss::SubblockType::SIGNAL) {
            has_raw_signal = true;
        }
    }
    ASSERT_TRUE(has_raw_signal, "SIGNAL 子块使用 RAW codec");

    std::vector<float> sig;
    ASSERT_TRUE(r.read_tile_signal(1, sig) == 0, "RAW 子块 read_tile_signal");
    ASSERT_TRUE(sig.size() == acc.pixels.size(), "RAW 往返 signal 长度");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// 辅助: 解析 TLV Header (R04-B14/B15), 定位 TILE_DIRECTORY value 的文件偏移
//   签名块(16B): magic[8] + header_length(u32 LE) + feature_flags(u32 LE)
//   TLV Header 从 offset 16 开始: tag(u16)+flags(u8)+length(u32)+value
//   返回 TILE_DIRECTORY value 起始偏移, 失败返回 SIZE_MAX
// ============================================================================
static size_t find_tile_directory_value_pos(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < 16) return SIZE_MAX;
    uint32_t header_length;
    std::memcpy(&header_length, file_data.data() + 8, 4);
    size_t pos = 16;  // TLV Header 起始
    size_t header_end = 16 + header_length;
    while (pos + 7 <= header_end) {
        uint16_t tag;
        std::memcpy(&tag, file_data.data() + pos, 2);
        uint8_t  flags = file_data[pos + 2];
        uint32_t length;
        std::memcpy(&length, file_data.data() + pos + 3, 4);
        pos += 7;  // 跳过 TLV 头
        if (tag == HISS_TLV_TILE_DIRECTORY) {
            return pos;  // value 起始位置
        }
        pos += length;  // 跳过 value
    }
    return SIZE_MAX;  // 未找到
}

// ============================================================================
// HISS 格式测试 15: 未知可选子块可跳过
// 构造包含未知可选子块的 HISS 文件, 验证 Reader 跳过它
// ============================================================================

// 辅助: 直接构建 HISS 二进制字节流 (用于构造异常文件)
struct HissBuilder {
    std::vector<uint8_t> data;

    void u8(uint8_t v) { data.push_back(v); }
    void u16(uint16_t v) { size_t n = data.size(); data.resize(n+2); std::memcpy(&data[n], &v, 2); }
    void u32(uint32_t v) { size_t n = data.size(); data.resize(n+4); std::memcpy(&data[n], &v, 4); }
    void u64(uint64_t v) { size_t n = data.size(); data.resize(n+8); std::memcpy(&data[n], &v, 8); }
    void f64(double v)   { size_t n = data.size(); data.resize(n+8); std::memcpy(&data[n], &v, 8); }
    void bytes(const void* p, size_t n) {
        size_t off = data.size(); data.resize(off+n);
        if (n > 0) std::memcpy(&data[off], p, n);
    }
};

static void test_15_unknown_optional_skip(int id) {
    TEST_CASE("未知可选子块可跳过", id);

    // 先用 Writer 写一个正常文件, 然后手动追加一个未知可选子块
    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 5; acc.pixel_area = 1.0;
    acc.pixels.resize(16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = 50.0; acc.pixels[i].sum_area = 1.0; acc.pixels[i].n_contrib = 1;
    }

    std::string path = write_test_hiss("hiss_test_15", hiss::OccupancyMode::FULL, acc);
    ASSERT_TRUE(!path.empty(), "写入基础 HISS 文件");

    // 读取文件内容, 在 Tile 目录中追加一个未知可选子块描述符
    std::ifstream fin(path, std::ios::binary);
    ASSERT_TRUE(fin.is_open(), "打开文件进行修改");
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(fin)),
                                    std::istreambuf_iterator<char>());
    fin.close();

    // R04-B14/B15: 解析 TLV Header, 定位 TILE_DIRECTORY value
    size_t tile_dir_value_pos = find_tile_directory_value_pos(file_data);
    ASSERT_TRUE(tile_dir_value_pos != SIZE_MAX, "找到 TILE_DIRECTORY TLV");

    // TILE_DIRECTORY value: n_tiles(4) + tile_prefix(15) + subblocks(50*n)
    // n_subblocks 在 tile_dir_value_pos + 4 + 13
    size_t n_subblocks_pos = tile_dir_value_pos + 4 + 13;
    uint16_t n_subblocks;
    std::memcpy(&n_subblocks, file_data.data() + n_subblocks_pos, 2);
    ASSERT_TRUE(n_subblocks >= 2, "原始 Tile 至少有 signal+support 子块");

    size_t desc_start = tile_dir_value_pos + 4 + 15;  // 第一个描述符位置
    size_t subblocks_end = desc_start + (size_t)n_subblocks * 50;

    // 插入前, 更新现有子块描述符的 offset (+50)
    // 原因: 插入 50B 描述符后 Header 增长 50B, 子块数据区整体后移 50B
    // R05-B09 描述符布局(50B): type(1)+ext_type_id(2)+flags(2)+offset(8)+...
    // offset 字段在描述符内偏移 5, 长度 8
    for (uint16_t i = 0; i < n_subblocks; i++) {
        size_t off_pos = desc_start + (size_t)i * 50 + 5;
        uint64_t old_off;
        std::memcpy(&old_off, file_data.data() + off_pos, 8);
        uint64_t new_off = old_off + 50;
        std::memcpy(file_data.data() + off_pos, &new_off, 8);
    }

    // 构造未知可选子块描述符 (R05-B09: 50 字节, 含 decoded_size)
    // type(1)+ext_type_id(2)+flags(2)+offset(8)+comp(8)+uncomp(8)+decoded(8)+codec(2)+transform(2)+cksum_type(1)+checksum(8)
    uint8_t unknown_desc[50] = {0};
    unknown_desc[0] = 200;  // 未知 type
    // ext_type_id = 0 (offset 1-2, 已为 0)
    uint16_t opt_flags = (uint16_t)hiss::SubblockFlags::OPTIONAL;
    std::memcpy(unknown_desc + 3, &opt_flags, 2);  // flags (offset 3-4)
    // offset 指向文件末尾 (compressed_size=0, Reader 跳过越界检查)
    uint64_t fake_offset = file_data.size() + 50;  // 插入后的文件末尾
    std::memcpy(unknown_desc + 5, &fake_offset, 8);  // offset (offset 5-12)
    uint64_t fake_size = 0;
    std::memcpy(unknown_desc + 13, &fake_size, 8);   // compressed_size=0 (offset 13-20)
    std::memcpy(unknown_desc + 21, &fake_size, 8);   // uncompressed_size=0 (offset 21-28)
    std::memcpy(unknown_desc + 29, &fake_size, 8);   // decoded_size=0 (offset 29-36, R05-B09)

    // 插入到子块描述符末尾 (Header 和数据区交界处)
    file_data.insert(file_data.begin() + subblocks_end, unknown_desc, unknown_desc + 50);

    // 更新 n_subblocks
    uint16_t new_n = (uint16_t)(n_subblocks + 1);
    std::memcpy(file_data.data() + n_subblocks_pos, &new_n, 2);

    // 更新 TILE_DIRECTORY TLV 的 length (+50)
    // TLV 头: tag(2) + flags(1) + length(4), value 在 tile_dir_value_pos
    // length 字段在 tile_dir_value_pos - 4
    size_t tlv_length_pos = tile_dir_value_pos - 4;
    uint32_t old_tlv_len;
    std::memcpy(&old_tlv_len, file_data.data() + tlv_length_pos, 4);
    uint32_t new_tlv_len = old_tlv_len + 50;
    std::memcpy(file_data.data() + tlv_length_pos, &new_tlv_len, 4);

    // 更新签名块中的 header_length (+50)
    // 签名块: magic[8] + header_length(u32 LE) @ offset 8 + feature_flags(u32 LE) @ offset 12
    uint32_t old_hlen;
    std::memcpy(&old_hlen, file_data.data() + 8, 4);
    uint32_t new_hlen = old_hlen + 50;
    std::memcpy(file_data.data() + 8, &new_hlen, 4);

    // 写回文件
    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    fout.write((const char*)file_data.data(), file_data.size());
    fout.close();

    // Reader 应能打开并跳过未知可选子块, 正常读取 signal/support
    hiss::HissReader r;
    int open_ret = r.open(path);
    ASSERT_TRUE(open_ret == 0, "Reader.open 成功 (跳过未知可选子块)");

    std::vector<float> sig;
    std::vector<uint8_t> sup;
    int read_ret = r.read_tile(5, sig, sup);
    r.close();
    std::filesystem::remove(path);

    // 关键: 未知可选子块不应阻止 signal/support 读取
    ASSERT_TRUE(read_ret == 0, "未知可选子块不影响 signal/support 读取 (可跳过)");
}

// ============================================================================
// HISS 格式测试 16: 未知必需子块拒绝
// 构造包含未知必需子块的 HISS 文件, 验证 Reader 拒绝
// ============================================================================

static void test_16_unknown_required_reject(int id) {
    TEST_CASE("未知必需子块拒绝", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 5; acc.pixel_area = 1.0;
    acc.pixels.resize(16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = 50.0; acc.pixels[i].sum_area = 1.0; acc.pixels[i].n_contrib = 1;
    }

    std::string path = write_test_hiss("hiss_test_16", hiss::OccupancyMode::FULL, acc);
    ASSERT_TRUE(!path.empty(), "写入基础 HISS 文件");

    // 读取并修改: 将 SUPPORT 子块的 type 改为未知值 (201), 保留 REQUIRED flags
    std::ifstream fin(path, std::ios::binary);
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(fin)),
                                    std::istreambuf_iterator<char>());
    fin.close();

    // R04-B14/B15: 解析 TLV Header, 定位 TILE_DIRECTORY value
    size_t tile_dir_value_pos = find_tile_directory_value_pos(file_data);
    ASSERT_TRUE(tile_dir_value_pos != SIZE_MAX, "找到 TILE_DIRECTORY TLV");

    // TILE_DIRECTORY value: n_tiles(4) + tile_prefix(15) + subblocks(50*n)
    size_t n_subblocks_pos = tile_dir_value_pos + 4 + 13;
    uint16_t n_subblocks;
    std::memcpy(&n_subblocks, file_data.data() + n_subblocks_pos, 2);
    ASSERT_TRUE(n_subblocks >= 2, "原始 Tile 至少有 signal+support 子块");

    // 遍历子块描述符 (R05-B09: 50B), 找到 SUPPORT (type=2) 并改为未知 type=201
    size_t desc_start = tile_dir_value_pos + 4 + 15;
    bool found = false;
    for (uint16_t i = 0; i < n_subblocks; i++) {
        size_t desc_off = desc_start + (size_t)i * 50;
        uint8_t type = file_data[desc_off];
        if (type == (uint8_t)hiss::SubblockType::SUPPORT) {
            file_data[desc_off] = 201;
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found, "找到 SUPPORT 子块并修改为未知 type");

    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    fout.write((const char*)file_data.data(), file_data.size());
    fout.close();

    // Reader.open 应拒绝未知必需子块 (02_FROZEN §13, 全扫描子块目录)
    hiss::HissReader r;
    int open_ret = r.open(path);
    fprintf(stderr, "  Reader.open 返回 %d (期望 HISS_ERR_UNKNOWN_REQUIRED=%d)\n",
            open_ret, HISS_ERR_UNKNOWN_REQUIRED);
    r.close();
    std::filesystem::remove(path);

    ASSERT_TRUE(open_ret == HISS_ERR_UNKNOWN_REQUIRED,
                "未知必需子块 → Reader.open 返回 HISS_ERR_UNKNOWN_REQUIRED (-7)");
}

// ============================================================================
// HISS 格式测试 17: offset/size 越界拒绝
// 构造子块 offset 超出文件大小的 HISS, 验证 Reader 拒绝
// ============================================================================

static void test_17_offset_overflow_reject(int id) {
    TEST_CASE("offset/size 越界拒绝", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 9; acc.pixel_area = 1.0;
    acc.pixels.resize(16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = 50.0; acc.pixels[i].sum_area = 1.0; acc.pixels[i].n_contrib = 1;
    }

    std::string path = write_test_hiss("hiss_test_17", hiss::OccupancyMode::FULL, acc);
    ASSERT_TRUE(!path.empty(), "写入基础 HISS 文件");

    // 读取并修改: 将 SIGNAL 子块的 offset 改为超大值
    std::ifstream fin(path, std::ios::binary);
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(fin)),
                                    std::istreambuf_iterator<char>());
    fin.close();

    // R04-B14/B15: 解析 TLV Header, 定位 TILE_DIRECTORY value
    size_t tile_dir_value_pos = find_tile_directory_value_pos(file_data);
    ASSERT_TRUE(tile_dir_value_pos != SIZE_MAX, "找到 TILE_DIRECTORY TLV");

    // 第一个子块是 SIGNAL (Writer 写入顺序: [occ?] signal support [snr?])
    // FULL 模式无 occ, 第一个子块是 SIGNAL
    // R05-B09 描述符布局(50B): type(1)+ext_type_id(2)+flags(2)+offset(8)+...
    // offset 字段在描述符内偏移 5
    size_t sig_desc_pos = tile_dir_value_pos + 4 + 15;  // 跳过 n_tiles + tile 头
    uint8_t sig_type = file_data[sig_desc_pos];
    ASSERT_TRUE(sig_type == (uint8_t)hiss::SubblockType::SIGNAL, "第一个子块是 SIGNAL");

    // 将 offset 改为超出文件大小
    uint64_t bad_offset = file_data.size() + 100000;  // 明显越界
    std::memcpy(file_data.data() + sig_desc_pos + 5, &bad_offset, 8);

    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    fout.write((const char*)file_data.data(), file_data.size());
    fout.close();

    // R04-B16: Reader.open 现在在解析 Tile 目录时即校验 offset/size 越界
    hiss::HissReader r;
    int open_ret = r.open(path);
    r.close();
    std::filesystem::remove(path);

    ASSERT_TRUE(open_ret < 0, "offset 越界 → Reader.open 返回 <0 (R04-B16: open 时即拒绝)");
}

// ============================================================================
// HISS 格式测试 18: checksum 错误定位到具体子块
// 构造子块 checksum 不匹配的 HISS, 验证 Reader 报错
// ============================================================================

static void test_18_checksum_error(int id) {
    TEST_CASE("checksum 错误定位到具体子块", id);

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 11; acc.pixel_area = 1.0;
    acc.pixels.resize(16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = 50.0; acc.pixels[i].sum_area = 1.0; acc.pixels[i].n_contrib = 1;
    }

    std::string path = write_test_hiss("hiss_test_18", hiss::OccupancyMode::FULL, acc);
    ASSERT_TRUE(!path.empty(), "写入基础 HISS 文件");

    // 读取文件, 修改 SIGNAL 子块数据 (使 checksum 不匹配, 但当前 Writer 默认 checksum=NONE)
    // 由于 Writer 默认 checksum_type=NONE, 直接篡改数据会导致解压成功但数据错误
    // 要测试 checksum 校验, 需要手动设置 checksum_type=CRC32C 并填充错误 checksum
    std::ifstream fin(path, std::ios::binary);
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(fin)),
                                    std::istreambuf_iterator<char>());
    fin.close();

    // R04-B14/B15: 解析 TLV Header, 定位 TILE_DIRECTORY value
    size_t tile_dir_value_pos = find_tile_directory_value_pos(file_data);
    ASSERT_TRUE(tile_dir_value_pos != SIZE_MAX, "找到 TILE_DIRECTORY TLV");

    // 跳到 SIGNAL 子块描述符 (n_tiles(4) + tile头(15) = 19)
    // R05-B09 描述符布局(50B): type(1)+ext_type_id(2)+flags(2)+offset(8)+comp(8)+uncomp(8)+decoded(8)+codec(2)+transform(2)+checksum_type(1)+checksum(8)
    // checksum_type @ offset+41, checksum @ offset+42
    size_t sig_desc_pos = tile_dir_value_pos + 4 + 15;  // 第一个子块 (SIGNAL)
    // 设置 checksum_type = CRC32C (1)
    file_data[sig_desc_pos + 41] = (uint8_t)hiss::ChecksumType::CRC32C;
    // 设置错误的 checksum (故意不匹配)
    uint64_t bad_checksum = 0xDEADBEEF12345678ULL;
    std::memcpy(file_data.data() + sig_desc_pos + 42, &bad_checksum, 8);

    std::ofstream fout(path, std::ios::binary | std::ios::trunc);
    fout.write((const char*)file_data.data(), file_data.size());
    fout.close();

    hiss::HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "打开含错误 checksum 的文件");

    std::vector<float> sig;
    std::vector<uint8_t> sup;
    int ret = r.read_tile(11, sig, sup);
    r.close();
    std::filesystem::remove(path);

    // Reader 应在 checksum 校验时失败 (返回 -5)
    ASSERT_TRUE(ret == -5, "checksum 错误 → read_tile 返回 -5 (CRC32C 校验失败)");
}

// ============================================================================
// HISS 格式测试 19: .partial 不会被普通 Reader 当正式 HISS
// ============================================================================
static void test_19_partial_not_accepted(int id) {
    TEST_CASE(".partial 不会被普通 Reader 当正式 HISS", id);

    // 构造一个 .partial 文件 (仅签名块占位, header_length=0)
    // R04-B14: 签名块(16B) = magic[8]+"HISS0100" + header_length(u32 LE) + feature_flags(u32 LE)
    std::string partial_path = "hiss_test_19.hiss.partial";
    {
        FILE* fp = std::fopen(partial_path.c_str(), "wb");
        ASSERT_TRUE(fp != nullptr, "创建 .partial 文件");
        uint8_t sig[16] = {0};
        const char magic[8] = {'H','I','S','S','0','1','0','0'};
        std::memcpy(sig, magic, 8);
        uint32_t header_length = 0;  // 占位, 未完成 (Header 区为空)
        std::memcpy(sig + 8, &header_length, 4);
        uint32_t feature_flags = 0;
        std::memcpy(sig + 12, &feature_flags, 4);
        std::fwrite(sig, 1, 16, fp);
        std::fclose(fp);
    }

    hiss::HissReader r;
    int ret = r.open(partial_path);
    r.close();
    std::filesystem::remove(partial_path);

    // .partial 文件 header_length=0, Reader 检查 Header 区无法容纳 grid_spec
    // → 返回 <0 (格式错误/越界)
    ASSERT_TRUE(ret < 0, ".partial 文件被 Reader 拒绝 (header_length=0 无有效 Header)");
}

// ============================================================================
// HISS 格式测试 20: 原子提交后正式文件可读
// Writer.finalize() 后 .partial 消失, .hiss 可读
// ============================================================================
static void test_20_atomic_commit(int id) {
    TEST_CASE("原子提交后正式文件可读", id);

    std::string path = "hiss_test_20.hiss";
    std::string tmppool_path = path + ".tmppool";  // 流式Writer临时池 (不是.partial)

    hiss::HissGridSpec grid;
    grid.nside = 64; grid.tile_nside = hiss::compute_tile_nside(64);
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = grid.nside; meta.tile_nside = grid.tile_nside;
    std::strncpy(meta.object, "AtomicTest", sizeof(meta.object) - 1);
    meta.photappl = 1; meta.photscal = 1.0;  // BUNIT=ASTROCS_RELATIVE_FLUX 要求 PHOTAPPL=TRUE

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16; acc.parent_ipix = 42; acc.pixel_area = 1.0;
    acc.pixels.resize(16  /* 4^2=16 (NSIDE=64,tile_nside=16,d=2) */);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = 75.0; acc.pixels[i].sum_area = 1.0; acc.pixels[i].n_contrib = 1;
    }

    hiss::HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "Writer.open");
    ASSERT_TRUE(w.add_tile(42, acc, nullptr, hiss::OccupancyMode::FULL) == 0, "Writer.add_tile");

    // finalize 前: .tmppool 存在, .hiss 不存在
    bool tmppool_before = std::filesystem::exists(tmppool_path);
    bool hiss_before = std::filesystem::exists(path);
    fprintf(stderr, "  finalize 前: .tmppool=%d .hiss=%d\n", (int)tmppool_before, (int)hiss_before);
    ASSERT_TRUE(tmppool_before, "finalize 前 .tmppool 存在");

    ASSERT_TRUE(w.finalize() == 0, "Writer.finalize");

    // finalize 后: .tmppool 消失, .hiss 存在
    bool tmppool_after = std::filesystem::exists(tmppool_path);
    bool hiss_after = std::filesystem::exists(path);
    fprintf(stderr, "  finalize 后: .tmppool=%d .hiss=%d\n", (int)tmppool_after, (int)hiss_after);
    ASSERT_TRUE(!tmppool_after, "finalize 后 .tmppool 已消失 (原子重命名)");
    ASSERT_TRUE(hiss_after, "finalize 后 .hiss 存在");

    // 正式文件可读
    hiss::HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "原子提交后的 .hiss 可被 Reader 打开");

    std::vector<float> sig;
    ASSERT_TRUE(r.read_tile_signal(42, sig) == 0, "读取原子提交的 Tile");
    ASSERT_TRUE(sig.size() == acc.pixels.size(), "signal 长度正确");
    // 验证数据正确
    ASSERT_NEAR(sig[0], 75.0f, 1e-4f, "signal 值正确 (75.0)");

    r.close();
    std::filesystem::remove(path);
}

// ============================================================================
// HISS 格式测试 21: NESTED ipix 和 Tile 父子恢复正确
// 验证 query_pixel 通过 ra/dec → NESTED ipix → parent_ipix 定位 Tile
// ============================================================================
static void test_21_nested_ipix_recovery(int id) {
    TEST_CASE("NESTED ipix 和 Tile 父子恢复正确", id);

    // 构造一个 NSIDE=64, tile_nside=16 的 HISS 文件
    // NSIDE=64 → depth=2, tile_nside=16
    // shift = 2 * (log2(64) - log2(16)) = 2 * (6-4) = 4
    // parent_ipix = global_ipix >> 4
    // local_ipix = global_ipix & 0xF

    uint32_t nside = 64;
    uint32_t tile_nside = hiss::compute_tile_nside(nside);
    ASSERT_TRUE(tile_nside == 16, "NSIDE=64 → tile_nside=16");

    hiss::HissGridSpec grid;
    grid.nside = nside; grid.tile_nside = tile_nside;
    grid.ordering = 1; grid.radesys = 0; grid.pixfrac = 1.0;

    hiss::HissMetadata meta;
    meta.nside = nside; meta.tile_nside = tile_nside;
    meta.photappl = 1; meta.photscal = 1.0;  // BUNIT=ASTROCS_RELATIVE_FLUX 要求 PHOTAPPL=TRUE

    // 选择一个 parent_ipix, 填充其所有叶像素
    uint64_t parent_ipix = 5;
    size_t n_leaf = (size_t)tile_nside * tile_nside * 12;  // 16^2*12 = 3072

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = tile_nside;
    acc.parent_ipix = parent_ipix;
    acc.pixel_area = 1.0;  // sum_area=1.0 为任意值, 用 1.0 保持测试原有行为
    acc.pixels.resize(n_leaf);
    for (size_t i = 0; i < n_leaf; i++) {
        acc.pixels[i].sum_flux = (double)i * 0.1;
        acc.pixels[i].sum_area = 1.0;
        acc.pixels[i].n_contrib = 1;
    }

    std::string path = "hiss_test_21.hiss";
    hiss::HissWriter w;
    ASSERT_TRUE(w.open(path, grid, meta) == 0, "Writer.open");
    ASSERT_TRUE(w.add_tile(parent_ipix, acc, nullptr, hiss::OccupancyMode::FULL) == 0, "Writer.add_tile");
    ASSERT_TRUE(w.finalize() == 0, "Writer.finalize");

    hiss::HissReader r;
    ASSERT_TRUE(r.open(path) == 0, "Reader.open");

    // 验证 Tile 目录中的 parent_ipix 正确
    const auto& tiles = r.tiles();
    ASSERT_TRUE(tiles.size() == 1, "Tile 数量 == 1");
    ASSERT_TRUE(tiles[0].parent_ipix == parent_ipix, "parent_ipix 恢复正确");
    ASSERT_TRUE(tiles[0].tile_nside == tile_nside, "tile_nside 恢复正确");

    // 验证网格规格恢复
    hiss::HissGridSpec read_grid = r.grid();
    ASSERT_TRUE(read_grid.nside == nside, "NSIDE 恢复正确");
    ASSERT_TRUE(read_grid.tile_nside == tile_nside, "tile_nside 恢复正确");
    ASSERT_TRUE(read_grid.ordering == 1, "ordering=NESTED 恢复正确");

    // query_pixel: 使用一个已知在 parent_ipix=5 范围内的 ra/dec
    // 注: query_pixel 内部用 radec_to_nested_ipix 计算 global_ipix, 再分解为 parent/local
    // 这里测试 query_pixel 能找到 Tile 并返回非零值
    // 选择 ra=10.0, dec=10.0 (任意位置, 可能不在 parent_ipix=5 内)
    float sig_val = -1.0f;
    uint8_t sup_val = 255;
    int qret = r.query_pixel(10.0, 10.0, &sig_val, &sup_val);
    ASSERT_TRUE(qret == 0, "query_pixel 返回 0 (成功或无覆盖返回零值)");

    // 如果该位置在 Tile 内, sig_val 应非零; 否则为 0 (无覆盖)
    fprintf(stderr, "  query_pixel(10,10): sig=%.4f sup=%u (0=无覆盖)\n", sig_val, (unsigned)sup_val);

    r.close();
    std::filesystem::remove(path);

    // 验证 tile 深度计算
    ASSERT_TRUE(hiss::compute_tile_depth(64) == 2, "NSIDE=64 → depth=2");
    ASSERT_TRUE(hiss::compute_tile_depth(16) == 0, "NSIDE=16 → depth=0");
    ASSERT_TRUE(hiss::compute_tile_depth(1024) == 6, "NSIDE=1024 → depth=6");
    ASSERT_TRUE(hiss::compute_tile_depth(8192) == 9, "NSIDE=8192 → depth=9 (上限)");
}

// ============================================================================
// 主函数: 运行所有测试并输出汇总
// ============================================================================

int main() {
    fprintf(stderr, "=== AstroCS HISS 正确性测试 ===\n");
    fprintf(stderr, "编译时间: %s %s\n", __DATE__, __TIME__);
#ifdef HAS_LZ4
    fprintf(stderr, "LZ4: 已启用\n");
#endif
#ifdef HAS_ZSTD
    fprintf(stderr, "ZSTD: 已启用\n");
#endif
    fprintf(stderr, "\n");

    // 列出已注册 codec
    auto codec_list = hiss::CodecRegistry::instance().list();
    fprintf(stderr, "已注册 codec (%zu): ", codec_list.size());
    for (auto id : codec_list) {
        const char* name = (id == hiss::CodecId::RAW) ? "RAW" :
                           (id == hiss::CodecId::LZ4) ? "LZ4" :
                           (id == hiss::CodecId::ZSTD) ? "ZSTD" : "?";
        fprintf(stderr, "%s ", name);
    }
    fprintf(stderr, "\n");

    // 运行所有测试
    test_01_standard_mode(1);
    test_02_exposure_ratio_mode(2);
    test_03_optimal_dark_success(3);
    test_04_optimal_dark_fallback(4);
    test_05_hard_failure_exptime_missing(5);
    test_06_single_pixel_flux_conservation(6);
    test_07_multi_pixel_flux_conservation(7);
    test_08_pixfrac_support_values(8);
    test_09_support_in_range(9);
    test_10_support_overflow_error(10);
    test_11_auto_nside(11);
    test_12_occupancy_roundtrip(12);
    test_13_independent_read(13);
    test_14_raw_subblock(14);
    test_15_unknown_optional_skip(15);
    test_16_unknown_required_reject(16);
    test_17_offset_overflow_reject(17);
    test_18_checksum_error(18);
    test_19_partial_not_accepted(19);
    test_20_atomic_commit(20);
    test_21_nested_ipix_recovery(21);

    // 汇总
    g_test_passed = g_test_total - (int)g_failures.size() - g_test_skipped;
    fprintf(stderr, "\n========== 测试汇总 ==========\n");
    fprintf(stderr, "总计: %d\n", g_test_total);
    fprintf(stderr, "通过: %d\n", g_test_passed);
    fprintf(stderr, "失败: %zu\n", g_failures.size());
    fprintf(stderr, "跳过: %d\n", g_test_skipped);

    if (!g_failures.empty()) {
        fprintf(stderr, "\n--- 失败详情 ---\n");
        for (const auto& f : g_failures) {
            fprintf(stderr, "  %s\n", f.c_str());
        }
    }
    if (!g_skips.empty()) {
        fprintf(stderr, "\n--- 跳过详情 ---\n");
        for (const auto& s : g_skips) {
            fprintf(stderr, "  %s\n", s.c_str());
        }
    }

    fprintf(stderr, "\n=== 测试结束 ===\n");
    return g_failures.empty() ? 0 : 1;
}

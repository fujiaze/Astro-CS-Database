// B13-R13-7 单元测试: generate_master_flat 负中位数拒绝
// (修复前: 负中位数帧被逐像素除以负数 → 全负 → 0.1 地板钳成常数假主帧)
// SCIENCE 契约 (docs/science/CALIBRATION.md §flat_norm): median<=0 不可归一化;
// 0.1 地板本身是契约行为, 测试同时固化正输入下 floor 语义不被回退。
#include "astro_calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)
#define CHECK_MSG(cond, msg)                                              \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

namespace {
constexpr int W = 8, H = 8, NPIX = W * H;

// 全负常值帧 → 帧中位数 < 0 → 必须拒绝, out 保持哨兵值不被写
void test_negative_median_rejected_f32() {
    std::vector<float> flat(2 * NPIX, -1.0f);
    std::vector<float> out(NPIX, 7.0f);  // 哨兵: 拒绝路径不得写 out
    const int rc = ac_generate_master_flat(flat.data(), 2, W, H, nullptr,
                                           out.data(), 3.0f, 3.0f, 5);
    CHECK(rc == AC_ERR_PARAM);
    CHECK(out[0] == 7.0f && out[NPIX / 2] == 7.0f);
}

// f64 wrapper 同语义: 拒绝 + 不回拷
void test_negative_median_rejected_f64() {
    std::vector<double> flat(2 * NPIX, -1.0);
    std::vector<double> out(NPIX, 7.0);
    const int rc = ac_generate_master_flat_f64(flat.data(), 2, W, H, nullptr,
                                               out.data(), 3.0, 3.0, 5);
    CHECK(rc == AC_ERR_PARAM);
    CHECK(out[0] == 7.0 && out[NPIX / 2] == 7.0);
}

// 正输入回归: 正常 flat 仍成功, 中位数归一到 1.0, floor 0.1 保持, 非常数场
void test_normal_flat_still_ok() {
    std::vector<float> flat(2 * NPIX);
    for (int i = 0; i < NPIX; ++i) {
        flat[i] = 1000.0f + static_cast<float>(i % 3) * 100.0f;
        flat[NPIX + i] = 2000.0f + static_cast<float>(i % 5) * 100.0f;
    }
    std::vector<float> out(NPIX, 0.0f);
    const int rc = ac_generate_master_flat(flat.data(), 2, W, H, nullptr,
                                           out.data(), 3.0f, 3.0f, 5);
    CHECK(rc == AC_OK);
    std::vector<float> sorted(out);
    std::sort(sorted.begin(), sorted.end());
    const float med = 0.5f * (sorted[NPIX / 2 - 1] + sorted[NPIX / 2]);
    CHECK(std::fabs(med - 1.0f) < 0.01f);       // SCIENCE: median→1.0
    CHECK(*std::min_element(sorted.begin(), sorted.end()) >= 0.1f);  // floor 契约
    CHECK(sorted.back() - sorted.front() > 1e-4f);  // 非常数场 (未塌缩)
}

// DISP-CAL-010: 帧级 median 必须跳过 NaN (与 generate_master 逐像素路径同一策略);
// 全 NaN 帧无有效中位数 → fail-closed (AC_ERR_PARAM, 不落伪主帧)。
// 修复前: median_of(nth_element) 含 NaN 属未定义序 → frame_med 可成 NaN 被兜底
// 成 1.0 (该帧不归一化), 全 NaN 帧则静默放行。
void test_nan_frame_median_skips_nan() {
    // f0=1000 常量; f1=2000 常量但 idx10=NaN。
    // 修复后: 两帧 median 分别 1000/2000 → 各归一为 1.0 → out 全 1.0。
    // 修复前: f1 median(nth_element 含 NaN)=NaN → 兜底 1.0 → f1 不归一化,
    //         全局尺度被 step3 重定后 idx10 被压到 0.1 floor (明显错值)。
    std::vector<float> flat(2 * NPIX);
    for (int i = 0; i < NPIX; ++i) { flat[i] = 1000.0f; flat[NPIX + i] = 2000.0f; }
    flat[NPIX + 10] = NAN;
    std::vector<float> out(NPIX, 7.0f);
    const int rc = ac_generate_master_flat(flat.data(), 2, W, H, nullptr,
                                           out.data(), 3.0f, 3.0f, 5);
    CHECK(rc == AC_OK);
    for (int i = 0; i < NPIX; ++i) {
        CHECK_MSG(std::isfinite(out[i]) && std::fabs(out[i] - 1.0f) < 1e-4f,
                  ("NaN frame median must skip NaN; out must normalize to 1.0 (idx " +
                   std::to_string(i) + " = " + std::to_string(out[i]) + ")").c_str());
    }
}

// 像素级全 NaN (该像素所有帧 NaN) → 输出该像素 NaN, 但非退化像素仍归一 1.0,
// 整帧不得被误判为退化 (与 generate_master 逐像素 NaN 策略一致)。
void test_all_nan_pixel_outputs_nan_not_reject() {
    std::vector<float> flat(2 * NPIX, 0.0f);
    for (int i = 0; i < NPIX; ++i) { flat[i] = 1000.0f; flat[NPIX + i] = 2000.0f; }
    flat[5] = NAN; flat[NPIX + 5] = NAN;   // 同一像素两帧都 NaN
    std::vector<float> out(NPIX, 7.0f);
    const int rc = ac_generate_master_flat(flat.data(), 2, W, H, nullptr,
                                           out.data(), 3.0f, 3.0f, 5);
    CHECK(rc == AC_OK);
    CHECK(std::isnan(out[5]));
    for (int i = 0; i < NPIX; ++i) {
        if (i == 5) continue;
        CHECK(std::isfinite(out[i]) && std::fabs(out[i] - 1.0f) < 1e-4f);
    }
}

// 全 NaN 帧 → 无法归一化 → fail-closed (AC_ERR_PARAM), out 保持哨兵不写。
void test_all_nan_frame_rejected() {
    std::vector<float> flat(2 * NPIX, 1000.0f);
    for (int i = 0; i < NPIX; ++i) flat[NPIX + i] = NAN;
    std::vector<float> out(NPIX, 7.0f);
    const int rc = ac_generate_master_flat(flat.data(), 2, W, H, nullptr,
                                           out.data(), 3.0f, 3.0f, 5);
    CHECK(rc == AC_ERR_PARAM);
    CHECK(out[0] == 7.0f && out[NPIX / 2] == 7.0f);
}

// f64 包装同语义: 全 NaN 帧 → 拒绝且不回拷。
void test_all_nan_frame_rejected_f64() {
    std::vector<double> flat(2 * NPIX, 1000.0);
    for (int i = 0; i < NPIX; ++i) flat[NPIX + i] = NAN;
    std::vector<double> out(NPIX, 7.0);
    const int rc = ac_generate_master_flat_f64(flat.data(), 2, W, H, nullptr,
                                               out.data(), 3.0, 3.0, 5);
    CHECK(rc == AC_ERR_PARAM);
    CHECK(out[0] == 7.0 && out[NPIX / 2] == 7.0);
}
}  // namespace

int main() {
    test_negative_median_rejected_f32();
    test_negative_median_rejected_f64();
    test_normal_flat_still_ok();
    test_nan_frame_median_skips_nan();
    test_all_nan_pixel_outputs_nan_not_reject();
    test_all_nan_frame_rejected();
    test_all_nan_frame_rejected_f64();
    if (failures == 0) {
        std::printf("B13-R13-7 MASTER FLAT MEDIAN TESTS PASS\n");
        return 0;
    }
    std::fprintf(stderr, "B13-R13-7 MASTER FLAT MEDIAN TESTS FAIL (%d)\n", failures);
    return 1;
}

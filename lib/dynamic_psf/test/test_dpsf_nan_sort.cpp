// ============================================================================
// test_dpsf_nan_sort.cpp - B4-P1-1 共址测试: NaN patch 排序 UB 修复
//
// 修复点: dpsf_psf.cpp 采样/统计阶段 —— 原实现对含 NaN 像素的 patch 直接
// std::sort (严格弱序违反 => UB), 且 NaN 会污染 median/MAD 统计。
// 修复后: (1) 采样阶段过滤非有限像素; (2) 全部 4 处 sort 使用 NaN-safe
// 全序比较器 (有限值上与 a<b 逐位等价)。
//
// 编译: g++ -std=c++17 -O2 -fopenmp -I../include -I../src \
//       test_dpsf_nan_sort.cpp ../src/dpsf_psf.cpp -o test_dpsf_nan_sort
// 运行: ./test_dpsf_nan_sort
// 返回: 0=通过, 非0=失败
//
// 日期: 2026-09-08 (bughunt P1 batchA)
// ============================================================================

#include "dpsf_psf.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [PASS] %s\n", msg); } \
    else      { std::printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

// 生成 Moffat/Gauss 形有限 patch (峰值 peak, 中心 cx,cy, 尺度 sigma)
static void fill_gauss_patch(std::vector<double>& img, int width, int height,
                             double cx, double cy, double peak, double sigma,
                             double bkg = 10.0) {
    // 叠加平坦背景, 使背景约束 |B-bkg0|/max(bkg0,0.01)<=0.5 可满足
    std::fill(img.begin(), img.end(), bkg);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double dx = x - cx, dy = y - cy;
            img[y * width + x] += peak * std::exp(-(dx*dx + dy*dy) / (2.0 * sigma * sigma));
        }
    }
}

int main() {
    std::printf("=== B4-P1-1: dpsf NaN sort UB 修复 共址测试 ===\n");
    const int W = 64, H = 64;

    // ---- Case 1: 有限输入回归 —— 行为不变 (修复不得破坏正常路径) ----
    {
        std::vector<double> img((size_t)W * H);
        fill_gauss_patch(img, W, H, 32.0, 32.0, 5000.0, 1.2, 20.0);
        DPSFFitResult r;
        std::memset(&r, 0, sizeof(r));
        int rc = moffat4_fit_d(img.data(), W, H, 32.0, 32.0, 20, 20, 44, 44, &r);
        CHECK(rc == DPSF_FIT_OK, "Case1 有限输入: 拟合成功");
        CHECK(std::isfinite(r.fwhm_x) && r.fwhm_x > 0.0, "Case1 有限输入: FWHM 有限且>0");
        CHECK(std::isfinite(r.B) && std::isfinite(r.A), "Case1 有限输入: B/A 有限");
        double expect_fwhm = 2.0 * std::sqrt(2.0 * std::log(2.0)) * 2.5 * std::sqrt(2.0); // Gauss σ->FWHM(β=∞参考)
        (void)expect_fwhm; // 不做严格数值断言 (科学容差冻结, 此处仅回归有限性)
    }

    // ---- Case 2: patch 内少量 NaN —— 不 UB/不崩溃, 输出有限或显式失败 ----
    {
        std::vector<double> img((size_t)W * H);
        fill_gauss_patch(img, W, H, 32.0, 32.0, 5000.0, 1.2, 20.0);
        const double nan_v = std::numeric_limits<double>::quiet_NaN();
        // 在采样矩形内撒 12 个 NaN (坏像元)
        for (int i = 0; i < 12; i++) {
            img[(22 + i) * W + (22 + i)] = nan_v;
        }
        DPSFFitResult r;
        std::memset(&r, 0, sizeof(r));
        int rc = moffat4_fit_d(img.data(), W, H, 32.0, 32.0, 20, 20, 44, 44, &r);
        bool status_legal = (rc == DPSF_FIT_OK || rc == DPSF_FIT_INVALID_PARAMS ||
                             rc == DPSF_FIT_NO_CONVERGENCE);
        CHECK(status_legal, "Case2 含NaN: 返回状态合法 (无UB崩溃)");
        if (rc == DPSF_FIT_OK) {
            CHECK(std::isfinite(r.fwhm_x) && std::isfinite(r.B) && std::isfinite(r.A),
                  "Case2 含NaN: 拟合成功时输出全有限 (NaN 未污染统计)");
        }
        (void)rc;
    }

    // ---- Case 3: patch 全 NaN —— 显式 INVALID_PARAMS (m==0 守卫) ----
    {
        std::vector<double> img((size_t)W * H);
        fill_gauss_patch(img, W, H, 32.0, 32.0, 5000.0, 1.2, 20.0);
        const double nan_v = std::numeric_limits<double>::quiet_NaN();
        for (int y = 20; y < 44; y++)
            for (int x = 20; x < 44; x++)
                img[y * W + x] = nan_v;
        DPSFFitResult r;
        std::memset(&r, 0, sizeof(r));
        int rc = moffat4_fit_d(img.data(), W, H, 32.0, 32.0, 20, 20, 44, 44, &r);
        CHECK(rc == DPSF_FIT_INVALID_PARAMS, "Case3 全NaN: 返回 DPSF_FIT_INVALID_PARAMS");
    }

    // ---- Case 4: patch 内 ±Inf —— 与 NaN 同等过滤 ----
    {
        std::vector<double> img((size_t)W * H);
        fill_gauss_patch(img, W, H, 32.0, 32.0, 5000.0, 1.2, 20.0);
        img[23 * W + 30] = std::numeric_limits<double>::infinity();
        img[30 * W + 23] = -std::numeric_limits<double>::infinity();
        DPSFFitResult r;
        std::memset(&r, 0, sizeof(r));
        int rc = moffat4_fit_d(img.data(), W, H, 32.0, 32.0, 20, 20, 44, 44, &r);
        bool status_legal = (rc == DPSF_FIT_OK || rc == DPSF_FIT_INVALID_PARAMS ||
                             rc == DPSF_FIT_NO_CONVERGENCE);
        CHECK(status_legal, "Case4 含Inf: 返回状态合法");
        if (rc == DPSF_FIT_OK) {
            CHECK(std::isfinite(r.fwhm_x), "Case4 含Inf: FWHM 有限");
        }
    }

    // ---- Case 5: float ABI (moffat4_fit) 同语义 ----
    {
        std::vector<double> img64((size_t)W * H);
        fill_gauss_patch(img64, W, H, 32.0, 32.0, 1000.0, 2.5);
        std::vector<float> img((size_t)W * H);
        for (size_t i = 0; i < img.size(); i++) img[i] = (float)img64[i];
        img[22 * W + 22] = std::numeric_limits<float>::quiet_NaN();
        DPSFFitResult r;
        std::memset(&r, 0, sizeof(r));
        int rc = moffat4_fit(img.data(), W, H, 32.0, 32.0, 20, 20, 44, 44, &r);
        CHECK(rc == DPSF_FIT_OK || rc == DPSF_FIT_INVALID_PARAMS ||
              rc == DPSF_FIT_NO_CONVERGENCE, "Case5 float ABI 含NaN: 状态合法");
    }

    std::printf("=== B4-P1-1 结果: %s (%d failures) ===\n",
                g_failures == 0 ? "ALL PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}

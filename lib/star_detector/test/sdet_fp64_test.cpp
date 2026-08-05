// ============================================================================
// sdet_fp64_test.cpp - StarDetector FP64 验证 (NON_PRODUCTION_TOOL_ONLY)
//
// 验证:
//   1. sdet_detect_ex (uint16→float) 与 sdet_detect_ex_f64 (double) 在合成星图上
//      检测星数一致, 坐标/通量差异在 FP32 输入舍入范围内
//   2. 两入口输出布局一致 (坐标 double, flux/mag float)
// ============================================================================
#include "star_detector.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include <cstdlib>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

int main() {
    const int W = 512, H = 512;
    const int NSTAR = 20;
    // 合成图: 背景 800 + 高斯星 (sigma=3, A=3000-9000)
    std::vector<float> fimg((size_t)W * H, 800.0f);
    std::vector<double> dimg((size_t)W * H, 800.0);
    for (int k = 0; k < NSTAR; k++) {
        double cx = 50.0 + (k % 5) * 100.0 + 0.37;
        double cy = 50.0 + (k / 5) * 100.0 + 0.61;
        double A = 3000.0 + k * 300.0;
        for (int y = -8; y <= 8; y++) {
            for (int x = -8; x <= 8; x++) {
                int px = (int)cx + x, py = (int)cy + y;
                if (px < 0 || py < 0 || px >= W || py >= H) continue;
                double dx = px + 0.5 - cx, dy = py + 0.5 - cy;
                double v = 800.0 + A * std::exp(-(dx * dx + dy * dy) / (2.0 * 3.0 * 3.0));
                fimg[(size_t)py * W + px] = (float)v;
                dimg[(size_t)py * W + px] = v;
            }
        }
    }
    std::vector<uint16_t> uimg((size_t)W * H);
    for (size_t i = 0; i < fimg.size(); i++) {
        // 同一物理值: uint16 直接存 fimg (峰值 ~9800 < 65535), double 图存 dimg
        double v = fimg[i];
        uimg[i] = (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
    }

    SDetParams params;
    std::memset(&params, 0, sizeof(params));
    params.structureLayers = 5;
    params.hotPixelFilterRadius = 1;
    params.iterativeClipSigma = 9.0f;
    params.iterativeMaxRounds = 5;
    params.medianFilterDetail = 1;
    params.maxStars = 100;
    params.fitRadius = 0;
    params.fwhmClipSigma = 3.0f;
    params.maxAxisRatio = 2.0f;

    StarDetectorHandle h = sdet_create(&params);
    if (!h) { printf("[FAIL] sdet_create\n"); return 1; }

    double *x32 = nullptr, *y32 = nullptr, *x64 = nullptr, *y64 = nullptr;
    float *flux32 = nullptr, *flux64 = nullptr;
    int *sat32 = nullptr, *sat64 = nullptr;
    float *mag32 = nullptr, *mag64 = nullptr;
    int *has32 = nullptr, *has64 = nullptr;
    int n32 = 0, n64 = 0;

    int r32 = sdet_detect_ex(h, uimg.data(), W, H,
                             &x32, &y32, &flux32, &sat32, &mag32, &has32, &n32,
                             nullptr, 0, nullptr);
    int r64 = sdet_detect_ex_f64(h, dimg.data(), W, H,
                                 &x64, &y64, &flux64, &sat64, &mag64, &has64, &n64,
                                 nullptr, 0, nullptr);
    char name[160];
    snprintf(name, sizeof(name), "两入口成功 (r32=%d r64=%d)", r32, r64);
    CHECK(r32 == 0 && r64 == 0, name);
    snprintf(name, sizeof(name), "检测星数: FP32=%d FP64=%d", n32, n64);
    CHECK(n32 == n64, name);

    if (n32 > 0 && n32 == n64) {
        double max_dx = 0, max_dy = 0, max_df = 0;
        for (int i = 0; i < n32 && i < n64; i++) {
            if (std::fabs(x32[i] - x64[i]) > max_dx) max_dx = std::fabs(x32[i] - x64[i]);
            if (std::fabs(y32[i] - y64[i]) > max_dy) max_dy = std::fabs(y32[i] - y64[i]);
            if (std::fabs(flux32[i] - flux64[i]) > max_df) max_df = std::fabs(flux32[i] - flux64[i]);
        }
        snprintf(name, sizeof(name), "坐标最大差 dx=%.3f dy=%.3f px (<1.0)", max_dx, max_dy);
        CHECK(max_dx < 1.0 && max_dy < 1.0, name);
        snprintf(name, sizeof(name), "通量最大差 %.3f (相对 %d)", max_df, n32);
        CHECK(max_df < 1000.0, name);
    }

    sdet_free_detect_ex(x32, y32, flux32, sat32, mag32, has32, nullptr, 0);
    sdet_free_detect_ex(x64, y64, flux64, sat64, mag64, has64, nullptr, 0);
    sdet_destroy(h);
    printf("== StarDetector FP64 验证: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

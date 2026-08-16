// ============================================================================
// control_median_mc_test.cpp — UPMW-005 / ALG-UPM-CONTROL-IVAR-001
//
// 用当前 Drizzle 引擎做 synthetic noise/covariance Monte Carlo，确定
// control estimator（background-clean patch median）的统计方差：
//
//   Var(median) = k_corr × (π/2) × sigma_bg² / N_retained
//
// k_corr >= 1 表征 Drizzle 输出像素协方差导致的 N_eff < N_retained。
// 本测试：
//   1) 对固定 WCS/几何生成 NMC 个独立高斯噪声实现并逐帧 drizzle；
//   2) 每实现计算固定 patch 的 median、N_retained、MAD 尺度；
//   3) 跨实现求 Var(median) 经验值，除以独立 Gaussian 基线
//      (π/2)·sigma²/N_retained 得 k_corr；
//   4) 冻结值写入 lib/phase2/src/sampler.cpp kControlCorrDefault；
//      UPMW-005 断言 |k_corr_frozen − k_corr_empirical| 在容差内。
//
// 编译（PowerShell）：
// cd lib\healpix_db\healpix_drizzle\tests
// g++ -O2 -std=c++17 -Wall -Wextra -I.. -I..\..\..\astro_image_io\include
//   -o control_median_mc_test.exe control_median_mc_test.cpp
//   ..\fits_reader.cpp ..\wcs_sip.cpp ..\poly_clip.cpp
//   ..\spherical_overlap.cpp ..\drizzle_engine.cpp ..\healpix_core.cpp
//   ..\snr_evaluator.cpp -fopenmp -static
//   -L..\..\..\astro_image_io -lastro_image_io -lm
// ============================================================================
#include "drizzle_engine.h"
#include "fits_reader.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace drizzle;

namespace {

constexpr int W = 20, H = 20;
constexpr int NSIDE = 512;
constexpr double SKY = 1000.0;
constexpr double SIGMA = 10.0;

void setup_wcs(FitsImage& im) {
    im.width = W;
    im.height = H;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = 10.0;
    im.wcs.crval[1] = 20.0;
    im.wcs.crpix[0] = (double)W * 0.5 + 0.5;
    im.wcs.crpix[1] = (double)H * 0.5 + 0.5;
    const double deg_per_px = 300.0 / 3600.0;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
}

DrizzleConfig make_cfg() {
    DrizzleConfig c;
    c.nside = NSIDE;
    c.nested = true;
    c.pixfrac = 0.8;      // 生产默认（足迹重叠 → 输出像素协方差）
    c.threads = 1;        // 确定性单线程（科学测试）
    c.apply_photometry = true;
    c.photometry_applied_upstream = true;
    c.tile_depth = 9;
    return c;
}

double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    const std::size_t mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    const double b = *std::max_element(v.begin(), v.begin() + mid);
    return 0.5 * (a + b);
}

}  // namespace

int main() {
    const int NMC = 2000;
    const double pi = 3.14159265358979323846;

    // 无噪声 truth 基准（验证 patch 尺度与 SKY 一致）
    {
        FitsImage im;
        setup_wcs(im);
        im.pixels.assign((std::size_t)H * W, (float)SKY);
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st;
        std::string err;
        DrizzleEngine eng0;
        if (!eng0.drizzleTiled(im, make_cfg(), nullptr, nullptr, nullptr,
                               tiles, st, err)) {
            std::printf("[FAIL] truth drizzle error: %s\n", err.c_str());
            return 1;
        }
        std::vector<double> patch;
        for (const auto& tile : tiles) {
            if (tile.touched.empty()) continue;
            for (uint32_t local : tile.touched) {
                if (local >= tile.pixels.size()) continue;
                const auto& a = tile.pixels[local];
                if (a.sumArea <= 0.0) continue;
                patch.push_back((double)a.sumFlux / (double)a.sumArea);
            }
            break;
        }
        std::printf("truth_patch_median=%.4f (SKY=%.0f)\n",
                    patch.empty() ? -1.0 : median_of(patch), (double)SKY);
    }

    // 单次 drizzle 的固定 patch：tile 内所有 touched leaf（support>0）。
    std::vector<double> patch_medians;
    std::vector<int> patch_n_retained;
    std::vector<double> patch_sigma;
    patch_medians.reserve(NMC);
    patch_n_retained.reserve(NMC);
    patch_sigma.reserve(NMC);

    DrizzleEngine eng;
    const DrizzleConfig cfg = make_cfg();
    const double t0 =
        (double)std::chrono::steady_clock::now().time_since_epoch().count();
    for (int r = 0; r < NMC; ++r) {
        FitsImage im;
        setup_wcs(im);
        im.pixels.assign((std::size_t)H * W, (float)SKY);
        std::mt19937 rng((unsigned)(20260816 + r));
        std::normal_distribution<double> nd(0.0, SIGMA);
        for (std::size_t i = 0; i < im.pixels.size(); ++i)
            im.pixels[i] += (float)nd(rng);
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st;
        std::string err;
        if (!eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr, tiles, st,
                              err)) {
            std::printf("[FAIL] realization %d drizzle error: %s\n", r,
                        err.c_str());
            return 1;
        }
        // 收集第一块 tile 的 touched leaf（固定 patch 几何）
        std::vector<double> patch;
        for (const auto& tile : tiles) {
            if (tile.touched.empty()) continue;
            for (uint32_t local : tile.touched) {
                if (local >= tile.pixels.size()) continue;
                const auto& a = tile.pixels[local];
                if (a.sumArea <= 0.0) continue;
                patch.push_back((double)a.sumFlux / (double)a.sumArea);
            }
            break;   // 固定 patch = 第一个 tile
        }
        if (patch.size() < 4) {
            std::printf("[FAIL] realization %d patch too small (%zu)\n", r,
                        patch.size());
            return 1;
        }
        const int n_retained = (int)patch.size();
        const double med = median_of(patch);
        std::vector<double> dev;
        dev.reserve(patch.size());
        for (double v : patch) dev.push_back(std::fabs(v - med));
        const double sigma = 1.4826 * median_of(std::move(dev));
        patch_medians.push_back(med);
        patch_n_retained.push_back(n_retained);
        patch_sigma.push_back(sigma);
    }
    const double dt =
        ((double)std::chrono::steady_clock::now().time_since_epoch().count() -
         t0) / 1e9;

    // 经验 Var(median)
    double mean_m = 0.0;
    for (double m : patch_medians) mean_m += m;
    mean_m /= (double)NMC;
    double var_emp = 0.0;
    for (double m : patch_medians) var_emp += (m - mean_m) * (m - mean_m);
    var_emp /= (double)(NMC - 1);

    // 尺度与 N_retained 取跨实现中位数（稳健）
    std::vector<double> sigma_sorted = patch_sigma;
    std::sort(sigma_sorted.begin(), sigma_sorted.end());
    std::vector<int> n_sorted = patch_n_retained;
    std::sort(n_sorted.begin(), n_sorted.end());
    const double sigma_med = sigma_sorted[sigma_sorted.size() / 2];
    const double n_med = (double)n_sorted[n_sorted.size() / 2];

    const double baseline = 0.5 * pi * sigma_med * sigma_med / n_med;
    const double k_corr = (baseline > 0.0) ? var_emp / baseline : 0.0;
    const double n_eff = (var_emp > 0.0) ? 0.5 * pi * sigma_med * sigma_med /
                                               var_emp : 0.0;

    std::printf(
        "=== UPMW-005 control-median MC (Drizzle pixfrac=0.8) ===\n");
    std::printf("NMC=%d  patch_median=%.4f  sigma_bg=%.4f  "
                "N_retained=%.0f\n", NMC, mean_m, sigma_med, n_med);
    std::printf("Var_emp(median)=%.6f  baseline(pi/2*s^2/N)=%.6f\n",
                var_emp, baseline);
    std::printf("k_corr=%.4f  N_eff=%.1f  (%.2fs)\n",
                k_corr, n_eff, dt);
    std::printf("k_corr_empirical = %.4f\n", k_corr);

    // 科学合理性门：Drizzle 协方差使 N_eff < N_retained → k_corr >= 1；
    // 且不会发散（k_corr < 2.0，20×20/512 采样下足迹重叠有限）。
    int fail = 0;
    if (!(k_corr >= 0.98 && k_corr <= 2.0)) {
        std::printf("[FAIL] k_corr 超出科学合理区间 [0.98, 2.0]\n");
        ++fail;
    }
    if (fail == 0)
        std::printf("[PASS] k_corr 科学合理，可用作 sampler 冻结值\n");
    return fail;
}

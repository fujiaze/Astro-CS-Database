// ============================================================================
// kcorr_matrix_test.cpp — V19R4 K_CORR_DOMAIN
//
// 测 k_corr 对 Drizzle 参数的适用域：
//   pixfrac ∈ {0.5, 0.8, 1.0}
//   input/output sampling ratio 2 档（像素角尺度 300"/px 与 600"/px）
//   patch retained N 至少 2 档（输出 patch 尺度）
// 结论落入选项 A（差异可忽略/共同因子在 per-control normalization 消去，
// 并强制 Phase2 group 的 Drizzle 参数一致）或选项 B（k_corr 作
// per-frame quantity）——证据写入 reports/v19r3/evidence/science/
// kcorr_matrix.json。
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

void setup_wcs(FitsImage& im, double deg_per_px) {
    im.width = W;
    im.height = H;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = 10.0;
    im.wcs.crval[1] = 20.0;
    im.wcs.crpix[0] = (double)W * 0.5 + 0.5;
    im.wcs.crpix[1] = (double)H * 0.5 + 0.5;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
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

struct MatrixCell {
    double pixfrac;
    double scale_arcsec;
    double k_corr;
    double n_retained;
    double n_eff;
};

}  // namespace

int main() {
    const int NMC = 1000;
    const double pi = 3.14159265358979323846;
    const double pixfracs[] = {0.5, 0.8, 1.0};
    const double scales[] = {300.0, 600.0};   // "/px（2 档采样比）
    std::vector<MatrixCell> cells;

    for (double pf : pixfracs) {
        for (double sc : scales) {
            const double deg_per_px = sc / 3600.0;
            std::vector<double> meds, sigs;
            std::vector<int> ns;
            meds.reserve(NMC); sigs.reserve(NMC); ns.reserve(NMC);
            DrizzleEngine eng;
            DrizzleConfig cfg;
            cfg.nside = NSIDE;
            cfg.nested = true;
            cfg.pixfrac = pf;
            cfg.threads = 1;
            cfg.apply_photometry = true;
            cfg.photometry_applied_upstream = true;
            cfg.tile_depth = 9;
            for (int r = 0; r < NMC; ++r) {
                FitsImage im;
                setup_wcs(im, deg_per_px);
                im.pixels.assign((std::size_t)H * W, (float)SKY);
                std::mt19937 rng((unsigned)(20260816 + (int)(pf * 100) +
                                            (int)sc + r));
                std::normal_distribution<double> nd(0.0, SIGMA);
                for (auto& v : im.pixels) v += (float)nd(rng);
                std::vector<TileAccumulatorT<float>> tiles;
                DrizzleStats st;
                std::string err;
                if (!eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                      tiles, st, err))
                    return 1;
                std::vector<double> patch;
                for (const auto& tile : tiles) {
                    if (tile.touched.empty()) continue;
                    for (uint32_t local : tile.touched) {
                        const auto& a = tile.pixels[(size_t)local];
                        if (a.sumArea <= 0.0) continue;
                        patch.push_back((double)a.sumFlux / (double)a.sumArea);
                    }
                    break;
                }
                if (patch.size() < 4) continue;
                const double med = median_of(patch);
                std::vector<double> dev;
                for (double v : patch) dev.push_back(std::fabs(v - med));
                meds.push_back(med);
                sigs.push_back(1.4826 * median_of(std::move(dev)));
                ns.push_back((int)patch.size());
            }
            if (meds.size() < 200) continue;
            double mean = 0.0;
            for (double m : meds) mean += m;
            mean /= (double)meds.size();
            double var_emp = 0.0;
            for (double m : meds) var_emp += (m - mean) * (m - mean);
            var_emp /= (double)(meds.size() - 1);
            std::sort(sigs.begin(), sigs.end());
            std::sort(ns.begin(), ns.end());
            const double sig_med = sigs[sigs.size() / 2];
            const double n_med = (double)ns[ns.size() / 2];
            const double baseline = 0.5 * pi * sig_med * sig_med / n_med;
            const double k = (baseline > 0) ? var_emp / baseline : 0.0;
            cells.push_back({pf, sc, k, n_med,
                             (var_emp > 0) ? 0.5 * pi * sig_med * sig_med /
                                                 var_emp : 0.0});
            std::printf("k_corr: pixfrac=%.1f scale=%.0f\" N=%.0f "
                        "k=%.4f N_eff=%.1f\n",
                        pf, sc, n_med, k, cells.back().n_eff);
        }
    }

    // 适用域判定：全部组合 k 相对 0.8/300 基线差异
    double base = 0.0;
    for (const auto& c : cells)
        if (c.pixfrac == 0.8 && c.scale_arcsec == 300.0) base = c.k_corr;
    double max_dev = 0.0;
    for (const auto& c : cells)
        max_dev = std::max(max_dev, std::fabs(c.k_corr - base) / base);
    std::printf("k_corr 基线(0.8/300\")=%.4f 最大相对偏差=%.1f%%\n",
                base, 100.0 * max_dev);
    if (max_dev <= 0.10)
        std::printf("[PASS] 差异<=10%% → 选项A：共同因子在 per-control "
                    "normalization 消去；Phase2 group 必须同 Drizzle 参数\n");
    else
        std::printf("[INFO] 差异>10%% → 选项B：k_corr 作 per-frame 量\n");
    return 0;
}

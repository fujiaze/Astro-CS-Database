// ============================================================================
// variance_propagation_test.cpp — Drizzle 方差传播科学测试
//
// 覆盖 (SCIENCE_ACCEPTANCE_MATRIX.md):
// TEST-DRZ-VAR-001: Drizzle variance 传播 + 协方差表征 MC
// SNR-011 Drizzle variance Monte Carlo (>=100 噪声实现)
// SNR-012 Drizzle covariance characterization
// DRZ-014 variance propagation identity
// DRZ-016 optimized geometry cache: variance 不改变 signal/support 科学语义
//
// 编译 (PowerShell):
// cd lib\healpix_db\healpix_drizzle\tests
// g++ -O2 -std=c++17 -Wall -Wextra -I.. -I..\..\..\astro_image_io\include
// -o variance_propagation_test.exe variance_propagation_test.cpp
// ..\fits_reader.cpp ..\wcs_sip.cpp ..\poly_clip.cpp ..\spherical_overlap.cpp
// ..\drizzle_engine.cpp ..\healpix_core.cpp ..\snr_evaluator.cpp
// -fopenmp -static -L..\..\..\astro_image_io -lastro_image_io -lm
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

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

namespace {

constexpr int W = 20, H = 20;
constexpr int NSIDE = 512;
constexpr double SKY = 1000.0;
constexpr double SIGMA = 10.0;          // 常数方差 100 (ADU²)

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

DrizzleConfig make_cfg(double pixfrac = 1.0) {
    DrizzleConfig c;
    c.nside = NSIDE;
    c.nested = true;
    c.pixfrac = pixfrac;
    c.threads = 1;   // 确定性单线程 (科学测试)
    c.apply_photometry = true;
    c.photometry_applied_upstream = true;
    c.tile_depth = 9;
    return c;
}

// 单次 drizzle: 返回 tile 内 leaf 的 (signal, variance) 汇总。
//
// **期望式必须与科学正本同幂次**（docs/science/DRIZZLE.md §5:50/§5:83）:
//   S_p          = sumFlux / sumNorm      (= F_p/N_p, N_p = Σ_j w_jp·A_pixel,j)
//   variance_p   = sumVarNum / sumNorm²
// 用覆盖率 sumArea=D_p 当分母是**另一套口径**（legacy 覆盖面积分母）:
//   sumFlux/sumArea = S_p·(1/pixfrac²)、sumVarNum/sumArea² = var_p·(1/pixfrac⁴)
// ⇒ pixfrac=1 时 sumNorm≡sumArea 使两者偶然同值, pixfrac<1 时相差 pf²/pf⁴
//   （本文件旧版正是用 sumArea, 且唯一调用点只用 pixfrac=1.0 ⇒ 结构上测不到）。
struct LeafStats {
    std::vector<uint64_t> ipix;
    std::vector<double> signal;
    std::vector<double> variance;
};

void collect_leafs(const std::vector<TileAccumulatorT<float>>& tiles,
                   LeafStats& out) {
    out.ipix.clear();
    out.signal.clear();
    out.variance.clear();
    for (const auto& tile : tiles) {
        for (uint32_t local : tile.touched) {
            if (local >= tile.pixels.size()) continue;
            const auto& a = tile.pixels[local];
            const double norm = (double)a.sumNorm;
            if (norm <= 0.0) continue;
            out.ipix.push_back((tile.parent_ipix << 18) | (uint64_t)local);
            out.signal.push_back((double)a.sumFlux / norm);
            const double vn = (double)a.sumVarNum;
            out.variance.push_back(vn / (norm * norm));
        }
    }
}

// 全局口径对照 (与 collect_leafs 同一归一约定):
//   S_doc    = Σ_p sumFlux_p / Σ_p sumNorm_p        (= Σ_j x_j / Σ_j A_pixel,j)
//   S_legacy = Σ_p sumFlux_p / Σ_p sumArea_p        (= Σ_j x_j / (pf²·Σ_j A_pixel,j))
// ⇒ S_legacy/S_doc ≡ 1/pf²（代数恒等，与本测试的几何无关）。
struct GlobalScales {
    double sum_flux = 0.0, sum_area = 0.0, sum_norm = 0.0, sum_varnum = 0.0;
};

GlobalScales global_scales(const std::vector<TileAccumulatorT<float>>& tiles) {
    GlobalScales g;
    for (const auto& tile : tiles) {
        for (uint32_t local : tile.touched) {
            if (local >= tile.pixels.size()) continue;
            const auto& a = tile.pixels[local];
            g.sum_flux += (double)a.sumFlux;
            g.sum_area += (double)a.sumArea;
            g.sum_norm += (double)a.sumNorm;
            g.sum_varnum += (double)a.sumVarNum;
        }
    }
    return g;
}

}  // namespace


// ---------------------------------------------------------------------------
// 组 5: pixfrac 扫描 — pf<1 在册用例 (面亮度归一幂次与 pixfrac 无关性)
//
// 背景: 本文件的旧版 oracle 用覆盖率 sumArea 当分母 (S=sumFlux/sumArea,
// var=sumVarNum/sumArea²), 而唯一调用点只用 pixfrac=1.0 —— 在 pf=1 时
// sumNorm≡sumArea 逐位相同, 故"分母取错"这一缺陷在结构上测不到;
// pf<1 时它立刻表现为 signal 偏 1/pf²、variance 偏 1/pf⁴。
// 本组把 pf∈{1.0,0.8,0.5,0.25} 全部纳入在册用例:
//   (a) N_p 关系      sumNorm·pf² == sumArea   (rel <=1e-3; 残差=球面 O(θ²)+FP32)
//   (b) 口径幂次      S_legacy/S_doc == 1/pf²          (rel <=1e-4)
//   (c) 方差幂次      var_legacy/var_doc == 1/pf⁴      (rel <=1e-4)
//   (d) 非退化        pf<1 时 |S_legacy/S_doc-1| 与 |var_legacy/var_doc-1| > 1e-3
//   (e) pixfrac 无关性 逐 leaf |S_p(pf)/S_p(1) - 1| < 2e-3 (真实几何, 真值帧)
//   (f) MC 校验       var_emp/var_doc 中位数 ∈ [0.95,1.05]
// (a)/(b)/(c) 锁定"累加器确实按 N_p=Σ w·A_pixel 记分母、两种分母相差 pf²/pf⁴";
// (d) 锁定该判据在 pf<1 上非恒真; (e)/(f) 是物理判据: 发布量确实是面亮度、
// 且方差是其无偏传播 (MC 经验方差 vs sumVarNum/N_p²)。
// 容差说明: (a) 的残差来自球面非线性 (A_drop ≠ pf²·A_pixel 的 O(θ²) 项) 与
// FP32 累加舍入; 缺陷幅度 (1/pf²-1) 在 pf=0.8 即 0.5625 ⇒ 余量 >500×。
// ---------------------------------------------------------------------------
static void run_pixfrac_sweep() {
    printf("\n=== pixfrac 扫描 (在册用例: pf<1 面亮度归一) ===\n");
    const double kPfs[] = {1.0, 0.8, 0.5, 0.25};
    const int kMcCounts[] = {0, 1200, 1200, 1200};   // pf=1 的 MC 已由 SNR-011 段承担

    // 基准: pf=1.0 真值帧的逐 leaf 面亮度
    LeafStats base_truth;
    {
        FitsImage im;
        setup_wcs(im);
        im.pixels.assign((std::size_t)H * W, (float)SKY);
        DrizzleEngine eng;
        DrizzleConfig cfg = make_cfg(1.0);
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st;
        std::string err;
        CHECK(eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr, tiles, st, err),
              "pf-sweep: pf=1.0 真值帧 drizzle ok");
        collect_leafs(tiles, base_truth);
    }

    for (std::size_t ipf = 0; ipf < sizeof(kPfs) / sizeof(kPfs[0]); ++ipf) {
        const double pf = kPfs[ipf];
        char tag[96];
        snprintf(tag, sizeof(tag), "pf=%.2f", pf);

        // 真值帧 (无噪声): 用于 pixfrac 无关性
        FitsImage truth;
        setup_wcs(truth);
        truth.pixels.assign((std::size_t)H * W, (float)SKY);
        DrizzleEngine eng;
        DrizzleConfig cfg = make_cfg(pf);
        std::vector<TileAccumulatorT<float>> ttiles;
        DrizzleStats tst;
        std::string err;
        CHECK(eng.drizzleTiled(truth, cfg, nullptr, nullptr, nullptr, ttiles, tst, err),
              (std::string("pf-sweep: 真值帧 drizzle ok ") + tag).c_str());
        LeafStats tleaf;
        collect_leafs(ttiles, tleaf);

        // (a) N_p 恒等 + (e) pixfrac 无关性
        {
            double worst_norm = 0.0, worst_sig = 0.0;
            int n_cmp = 0;
            for (const auto& tile : ttiles) {
                for (uint32_t local : tile.touched) {
                    if (local >= tile.pixels.size()) continue;
                    const auto& a = tile.pixels[local];
                    const double area = (double)a.sumArea;
                    const double norm = (double)a.sumNorm;
                    if (area <= 0.0 || norm <= 0.0) continue;
                    worst_norm = std::max(worst_norm,
                                          std::fabs(norm * pf * pf / area - 1.0));
                    const uint64_t ip = (tile.parent_ipix << 18) | (uint64_t)local;
                    auto it = std::find(base_truth.ipix.begin(), base_truth.ipix.end(), ip);
                    if (it == base_truth.ipix.end()) continue;
                    const double s_here = (double)a.sumFlux / norm;
                    const double s_base = base_truth.signal[(std::size_t)(it - base_truth.ipix.begin())];
                    if (std::fabs(s_base) > 0.0) {
                        worst_sig = std::max(worst_sig, std::fabs(s_here / s_base - 1.0));
                        ++n_cmp;
                    }
                }
            }
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "%s (a) sumNorm*pf^2 == sumArea (球面 O(theta^2)+FP32 残差): "
                     "worst_rel=%.2e (<=1e-3)", tag, worst_norm);
            CHECK(worst_norm <= 1e-3, msg);
            snprintf(msg, sizeof(msg),
                     "%s (e) S_p 与 pixfrac 无关: worst_rel=%.2e (<2e-3, n=%d)", tag,
                     worst_sig, n_cmp);
            CHECK(n_cmp > 0 && worst_sig < 2e-3, msg);
        }

        // 方差帧 (同一噪声实现 + 逐像素方差): (b)(c)(f) 的输入
        FitsImage im_v;
        setup_wcs(im_v);
        im_v.pixels.assign((std::size_t)H * W, (float)SKY);
        std::mt19937 rng0(20260823);
        std::normal_distribution<double> nd(0.0, SIGMA);
        std::vector<float> var_map((std::size_t)H * W, 0.0f);
        for (std::size_t i = 0; i < im_v.pixels.size(); ++i) {
            im_v.pixels[i] += (float)nd(rng0);
            var_map[i] = (float)(SIGMA * SIGMA);
        }
        std::vector<TileAccumulatorT<float>> vtiles;
        DrizzleStats vst;
        CHECK(eng.drizzleTiled(im_v, cfg, nullptr, nullptr, var_map.data(), vtiles, vst, err),
              (std::string("pf-sweep: 方差帧 drizzle ok ") + tag).c_str());
        LeafStats vleaf;
        collect_leafs(vtiles, vleaf);

        // (b)(c)(d) 全局口径幂次。真值: 两种分母相差 pf²(signal)/pf⁴(variance)。
        // 容差 1e-4: 实测残差来自球面非线性 (A_drop 与 pf²·A_pixel 的 O(θ²) 差,
        // 本 WCS 300"/px 下 ~1e-6) 与 FP32 累加舍入; 而缺陷幅度在 pf=0.8 已是
        // 0.5625/1.441, pf=0.25 是 15/255 ⇒ 判据仍高判别 (余量 >5e3×)。
        {
            const GlobalScales g = global_scales(vtiles);
            const double s_doc = g.sum_flux / g.sum_norm;
            const double s_legacy = g.sum_flux / g.sum_area;
            const double var_doc = g.sum_varnum / (g.sum_norm * g.sum_norm);
            const double var_legacy = g.sum_varnum / (g.sum_area * g.sum_area);
            const double pf2 = pf * pf;
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "%s (b) S_legacy/S_doc == 1/pf^2: rel=%.2e (<=1e-4)", tag,
                     std::fabs((s_legacy / s_doc) * pf2 - 1.0));
            CHECK(std::fabs((s_legacy / s_doc) * pf2 - 1.0) <= 1e-4, msg);
            snprintf(msg, sizeof(msg),
                     "%s (c) var_legacy/var_doc == 1/pf^4: rel=%.2e (<=1e-4)", tag,
                     std::fabs((var_legacy / var_doc) * pf2 * pf2 - 1.0));
            CHECK(std::fabs((var_legacy / var_doc) * pf2 * pf2 - 1.0) <= 1e-4, msg);
            if (pf < 1.0) {
                snprintf(msg, sizeof(msg),
                         "%s (d) 判据非退化: |S_legacy/S_doc-1|=%.4f (>1e-3), "
                         "|var_legacy/var_doc-1|=%.4f (>1e-3)", tag,
                         std::fabs(s_legacy / s_doc - 1.0),
                         std::fabs(var_legacy / var_doc - 1.0));
                CHECK(std::fabs(s_legacy / s_doc - 1.0) > 1e-3 &&
                          std::fabs(var_legacy / var_doc - 1.0) > 1e-3, msg);
            }
        }

        // (f) Monte Carlo: 经验方差 vs 传播方差 (同一 pf)
        if (kMcCounts[ipf] > 0) {
            std::vector<std::vector<double>> mc(tleaf.ipix.size());
            for (std::size_t i = 0; i < tleaf.ipix.size(); ++i) mc[i].reserve(kMcCounts[ipf]);
            for (int r = 0; r < kMcCounts[ipf]; ++r) {
                FitsImage im;
                setup_wcs(im);
                im.pixels.assign((std::size_t)H * W, (float)SKY);
                std::mt19937 rng((unsigned)(20260900 + r));
                std::normal_distribution<double> ndm(0.0, SIGMA);
                for (std::size_t i = 0; i < im.pixels.size(); ++i) im.pixels[i] += (float)ndm(rng);
                std::vector<TileAccumulatorT<float>> tiles;
                DrizzleStats st;
                if (!eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr, tiles, st, err)) {
                    CHECK(false, (std::string("pf-sweep MC drizzle 失败 ") + tag).c_str());
                    break;
                }
                LeafStats ls;
                collect_leafs(tiles, ls);
                for (std::size_t i = 0; i < tleaf.ipix.size(); ++i) {
                    auto it = std::find(ls.ipix.begin(), ls.ipix.end(), tleaf.ipix[i]);
                    mc[i].push_back(it != ls.ipix.end()
                                        ? ls.signal[(std::size_t)(it - ls.ipix.begin())]
                                        : std::numeric_limits<double>::quiet_NaN());
                }
            }
            std::vector<double> ratio;
            for (std::size_t i = 0; i < tleaf.ipix.size(); ++i) {
                const std::size_t n = mc[i].size();
                if (n < 2) continue;
                double mean = 0.0;
                bool finite = true;
                for (double s : mc[i]) {
                    if (!std::isfinite(s)) { finite = false; break; }
                    mean += s;
                }
                if (!finite) continue;
                mean /= (double)n;
                double ve = 0.0;
                for (double s : mc[i]) ve += (s - mean) * (s - mean);
                ve /= (double)(n - 1);
                auto it = std::find(vleaf.ipix.begin(), vleaf.ipix.end(), tleaf.ipix[i]);
                if (it == vleaf.ipix.end()) continue;
                const double vp = vleaf.variance[(std::size_t)(it - vleaf.ipix.begin())];
                if (vp <= 0.0) continue;
                ratio.push_back(ve / vp);
            }
            std::sort(ratio.begin(), ratio.end());
            char msg[256];
            if (ratio.empty()) {
                CHECK(false, (std::string("pf-sweep MC 无有效 leaf ") + tag).c_str());
            } else {
                const double p50 = ratio[ratio.size() / 2];
                snprintf(msg, sizeof(msg),
                         "%s (f) MC var_emp/var_doc p50=%.3f ∈[0.95,1.05] (n=%zu, MC=%d)",
                         tag, p50, ratio.size(), kMcCounts[ipf]);
                CHECK(p50 >= 0.95 && p50 <= 1.05, msg);
                // 对照: 若按覆盖率分母 (legacy) 期望, 比值应为 pf^4 (pf=0.8 ⇒ 0.41)
                printf("        %s 对照: 若分母取 sumArea, 同一经验方差对应的比值 p50 会偏 %.3f×\n",
                       tag, 1.0 / (pf * pf * pf * pf));
            }
        }
    }
}

int main() {
    printf("=== V19 Drizzle 方差传播科学测试 ===\n");

    // ---- 无噪声 truth (确定性) ----
    FitsImage img_truth;
    setup_wcs(img_truth);
    img_truth.pixels.assign((std::size_t)H * W, (float)SKY);
    DrizzleEngine eng;
    DrizzleConfig cfg = make_cfg();
    std::vector<TileAccumulatorT<float>> tiles_truth;
    DrizzleStats st_truth;
    std::string err;
    bool ok = eng.drizzleTiled(img_truth, cfg, nullptr, nullptr, nullptr,
                               tiles_truth, st_truth, err);
    CHECK(ok, "truth drizzle ok");
    LeafStats truth;
    collect_leafs(tiles_truth, truth);

    // ---- 单次带方差传播 ----
    FitsImage img_v;
    setup_wcs(img_v);
    img_v.pixels.assign((std::size_t)H * W, (float)SKY);
    std::mt19937 rng0(20260823);
    std::normal_distribution<double> nd(0.0, SIGMA);
    std::vector<float> var_map((std::size_t)H * W, 0.0f);
    for (std::size_t i = 0; i < img_v.pixels.size(); ++i) {
        img_v.pixels[i] += (float)nd(rng0);
        var_map[i] = (float)(SIGMA * SIGMA);
    }
    std::vector<TileAccumulatorT<float>> tiles_v;
    DrizzleStats st_v;
    ok = eng.drizzleTiled(img_v, cfg, nullptr, nullptr, var_map.data(),
                          tiles_v, st_v, err);
    CHECK(ok, "variance drizzle ok");
    LeafStats vleaf;
    collect_leafs(tiles_v, vleaf);

    // ---- DRZ-016: variance 不改变 signal/support (同一噪声帧对照) ----
    {
        FitsImage img_ref;
        setup_wcs(img_ref);
        img_ref.pixels = img_v.pixels;   // 同一次噪声实现, 无 variance 输入
        std::vector<TileAccumulatorT<float>> tiles_ref;
        DrizzleStats st_ref;
        ok = eng.drizzleTiled(img_ref, cfg, nullptr, nullptr, nullptr,
                              tiles_ref, st_ref, err);
        CHECK(ok, "reference (no variance) drizzle ok");
        LeafStats ref_leaf;
        collect_leafs(tiles_ref, ref_leaf);
        bool same = true;
        for (const auto& t : tiles_v) {
            for (uint32_t local : t.touched) {
                const auto& a = t.pixels[local];
                const double norm = (double)a.sumNorm;   // 面亮度分母 (非覆盖面积)
                if (norm <= 0.0) continue;
                const uint64_t ip = (t.parent_ipix << 18) | (uint64_t)local;
                auto it = std::find(ref_leaf.ipix.begin(), ref_leaf.ipix.end(), ip);
                if (it == ref_leaf.ipix.end()) { same = false; break; }
                const std::size_t idx = (std::size_t)(it - ref_leaf.ipix.begin());
                const double sig_diff = std::fabs((double)a.sumFlux / norm -
                                                  ref_leaf.signal[idx]);
                if (sig_diff > 1e-6) { same = false; break; }
            }
        }
        CHECK(same, "DRZ-016: variance 输入不改变 signal (同一噪声帧)");
    }

    // ---- SNR-011 Monte Carlo: >=100 实现 ----
    const int NMC = 4000;
    std::vector<std::vector<double>> mc_signals(truth.ipix.size());
    for (std::size_t i = 0; i < truth.ipix.size(); ++i) mc_signals[i].reserve(NMC);
    const double t0 = (double)std::chrono::steady_clock::now().time_since_epoch().count();
    for (int r = 0; r < NMC; ++r) {
        FitsImage im;
        setup_wcs(im);
        im.pixels.assign((std::size_t)H * W, (float)SKY);
        std::mt19937 rng((unsigned)(20260900 + r));
        std::normal_distribution<double> ndm(0.0, SIGMA);
        for (std::size_t i = 0; i < im.pixels.size(); ++i) im.pixels[i] += (float)ndm(rng);
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st;
        if (!eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                              tiles, st, err)) {
            CHECK(false, "MC drizzle 失败");
            break;
        }
        LeafStats ls;
        collect_leafs(tiles, ls);
        for (std::size_t i = 0; i < truth.ipix.size(); ++i) {
            auto it = std::find(ls.ipix.begin(), ls.ipix.end(), truth.ipix[i]);
            const double sig = (it != ls.ipix.end())
                ? ls.signal[(std::size_t)(it - ls.ipix.begin())]
                : std::numeric_limits<double>::quiet_NaN();
            mc_signals[i].push_back(sig);
        }
    }
    const double dt = ((double)std::chrono::steady_clock::now().time_since_epoch().count() - t0) / 1e9;
    printf("[SNR-011] MC %d 实现, %zu 个有效 leaf, 耗时 %.2fs\n",
           NMC, truth.ipix.size(), dt);

    // 逐 leaf 经验方差 vs 传播方差
    std::vector<double> ratio;
    ratio.reserve(truth.ipix.size());
    int n_leaf_ok = 0;
    for (std::size_t i = 0; i < truth.ipix.size(); ++i) {
        const std::size_t n = mc_signals[i].size();
        if (n < 2) continue;
        double mean = 0;
        bool all_finite = true;
        for (double s : mc_signals[i]) {
            if (!std::isfinite(s)) { all_finite = false; break; }
            mean += s;
        }
        if (!all_finite) continue;
        mean /= (double)n;
        double var_emp = 0;
        for (double s : mc_signals[i]) var_emp += (s - mean) * (s - mean);
        var_emp /= (double)(n - 1);
        // 传播方差 (单次)
        auto it = std::find(vleaf.ipix.begin(), vleaf.ipix.end(), truth.ipix[i]);
        if (it == vleaf.ipix.end()) continue;
        const double var_prop = vleaf.variance[(std::size_t)(it - vleaf.ipix.begin())];
        if (var_prop <= 0.0) continue;
        ratio.push_back(var_emp / var_prop);
        ++n_leaf_ok;
    }
    std::sort(ratio.begin(), ratio.end());
    if (!ratio.empty()) {
        auto pct = [&](double p) {
            return ratio[(std::size_t)(p * (double)(ratio.size() - 1))];
        };
        const double p50 = pct(0.50), p95_lo = pct(0.025), p95_hi = pct(0.975);
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "SNR-011 p50=%.3f (0.98-1.02), p95=[%.3f,%.3f] (0.95-1.05), n=%d",
                 p50, p95_lo, p95_hi, n_leaf_ok);
        CHECK(p50 >= 0.98 && p50 <= 1.02 && p95_lo >= 0.95 && p95_hi <= 1.05, msg);
    } else {
        CHECK(false, "SNR-011 无有效 leaf 样本");
    }

    // ---- SNR-012 相邻像素协方差表征 ----
    {
        // 对 truth leaf 的前 200 个相邻对 (按 ipix 相邻) 计算经验相关性
        double corr_max = 0.0, corr_mean = 0.0;
        int n_pairs = 0;
        for (std::size_t a = 0; a + 1 < truth.ipix.size() && a < 400; ++a) {
            const uint64_t ipa = truth.ipix[a];
            for (std::size_t b = a + 1; b < truth.ipix.size() && b < a + 16; ++b) {
                const uint64_t ipb = truth.ipix[b];
                if ((ipa ^ ipb) != 1ULL) continue;   // NESTED 相邻 (最后一位不同)
                const std::size_t n = std::min(mc_signals[a].size(), mc_signals[b].size());
                if (n < 4) continue;
                double ma = 0, mb = 0;
                bool all_fin = true;
                for (std::size_t i = 0; i < n; ++i) {
                    if (!std::isfinite(mc_signals[a][i]) ||
                        !std::isfinite(mc_signals[b][i])) { all_fin = false; break; }
                    ma += mc_signals[a][i]; mb += mc_signals[b][i];
                }
                if (!all_fin) continue;
                ma /= (double)n; mb /= (double)n;
                double num = 0, da = 0, db = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    const double aa = mc_signals[a][i] - ma;
                    const double bb = mc_signals[b][i] - mb;
                    num += aa * bb; da += aa * aa; db += bb * bb;
                }
                const double den = std::sqrt(da * db);
                if (den > 0.0) {
                    const double rho = num / den;
                    corr_max = std::max(corr_max, std::fabs(rho));
                    corr_mean += std::fabs(rho);
                    ++n_pairs;
                }
            }
        }
        if (n_pairs > 0) corr_mean /= (double)n_pairs;
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "SNR-012 相邻像素 |ρ| mean=%.4f max=%.4f (n=%d, 已表征)",
                 corr_mean, corr_max, n_pairs);
        CHECK(n_pairs > 0, msg);
        printf("        (Drizzle 后相邻像素非严格独立: mean|ρ|=%.4f max|ρ|=%.4f, "
               "pixfrac/resampling 引入协方差, 文档记录)\n",
               corr_mean, corr_max);
    }

    // ---- DRZ-014 缩放恒等: variance × α² → 输出 variance × α² ----
    {
        std::vector<float> var2((std::size_t)H * W, 0.0f);
        for (auto& v : var2) v = (float)(4.0 * SIGMA * SIGMA);
        FitsImage img2;
        setup_wcs(img2);
        img2.pixels = img_v.pixels;   // 同一次噪声实现
        std::vector<TileAccumulatorT<float>> tiles2;
        DrizzleStats st2;
        ok = eng.drizzleTiled(img2, cfg, nullptr, nullptr, var2.data(),
                              tiles2, st2, err);
        CHECK(ok, "scaled variance drizzle ok");
        LeafStats l2;
        collect_leafs(tiles2, l2);
        bool scale_ok = true;
        double worst = 0.0;
        for (std::size_t i = 0; i < vleaf.ipix.size() && scale_ok; ++i) {
            auto it = std::find(l2.ipix.begin(), l2.ipix.end(), vleaf.ipix[i]);
            if (it == l2.ipix.end()) continue;
            const double expect = vleaf.variance[i] * 4.0;
            const double got = l2.variance[(std::size_t)(it - l2.ipix.begin())];
            const double rel = std::fabs(got - expect) / std::max(expect, 1e-12);
            worst = std::max(worst, rel);
            if (rel > 1e-4) scale_ok = false;
        }
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "DRZ-014 variance(α²v)=α²variance: worst_rel=%.2e (<1e-4)",
                 worst);
        CHECK(scale_ok && worst < 1e-4, msg);
    }

    // ---- pixfrac 扫描 (pf<1 在册用例: 面亮度归一幂次) ----
    run_pixfrac_sweep();

    printf("\n== V19 Drizzle 方差传播结果: %d 通过, %d 失败 ==\n",
           g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

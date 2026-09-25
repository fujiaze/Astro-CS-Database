// ============================================================================
// DRIZZLE-FIX-01 / DRZ-FLUX-FIX-01 · DISP-DRZ-009 回归门 (核按 drop 面积归一,
// 面亮度归一分母 N_p = Σ_j w_jp·A_pixel,j)
// ----------------------------------------------------------------------------
// 合同锚:
//   * SCI-DRZ-001 §5 (docs/science/DRIZZLE.md, 负责人裁决口径):
//     w_jp = a_jp/A_drop,j (F&H 2002 §7.2 式(7) 下方 "fractional area overlap of
//     **the drop**"; drizzlepac cdrizzlebox.c dover/=jaco),
//     F_p = Σ_j x_j·w_jp, D_p = Σ_j a_jp, N_p = Σ_j w_jp·A_pixel,j,
//     S_p = F_p/N_p = Σ_j B_j a_jp/Σ_j a_jp;
//   * SCI-DRZ-001 §7 常量场不变量 + FZ-GATE-CONST-SB (门 |S_p/B0−1| < 1e-3,
//     对全部 pixfrac∈(0,1] 成立);
//   * SCI-DRZ-001 §10 不可接受变化: "将 S_p 的**分母**取覆盖面积 D_p=Σ_j a_jp
//     而非面亮度归一分母 N_p"(pixfrac<1 偏 1/pixfrac²) — 本门即该条的回归锁;
//   * ALG-DRZ-001 §10 DISP-DRZ-009 (历史缺陷登记) + DRZ-FLUX-FIX-01 (口径订正).
//
// 缺陷口径 (历史, 实测): 核取 drop 分数交叠 a_jp/A_drop,j **而分母取覆盖面积
// Σ a_jp** ⇒ S_p = B0/pixfrac², 与解析式 1/pf²−1 逐位吻合
// (pf=0.8 → +56.25%, 0.6 → +177.8%, 0.5 → +300%); pixfrac=1 时 A_drop≡A_pixel
// 且 N_p≡D_p ⇒ 误差恰为 0 (默认值掩盖缺陷)。
//
// 本门的**两条**判据 (缺一不可, 保证非退化):
//   P (正例): 实测 max|S_p/B0 − 1| < 1e-3            ⇒ 必须绿;
//   N (负例控制): 用**独立 oracle 几何**给出的 A_pixel/A_drop 比值把实测 S_p
//     投影回"若分母取覆盖面积 D_p=Σ a_jp 会得到什么", 该投影必须**判红**。
//     ⇒ 证明该门的容差确有分辨两种归一分母的能力 (恒真门没有证据资格);
//       S_p^wrong = S_p·(A_pixel/A_drop) = B0/pf² ⇒ 本条判红。
// 本门不调用被测函数产生期望值: 期望值来自 SCI §5 恒等式 + p1drz_oracle.hpp
// 的独立几何 (Van Oosterom 立体角, 与生产 Van Oosterom 扇形不同式)。
//
// 用法:
//   p1drz_disp009_gate                 # 判据 (ctest p1drz_disp009)
//   p1drz_disp009_gate dump <outdir>   # 判据 + 逐 pixfrac 产物转储 (逐字节证据)
// ============================================================================
#include "drizzle_engine.h"

#include "p1drz_fixtures.hpp"
#include "p1drz_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using drizzle::DrizzleConfig;
using drizzle::DrizzleEngine;
using drizzle::DrizzleMeta;
using drizzle::DrizzleStats;
using drizzle::FitsImage;
using drizzle::TileAccumulatorT;

namespace {

constexpr int W = 16, H = 16;
constexpr int NSIDE = 512;
constexpr double SCALE = 300.0;
constexpr double B0 = 1000.0;
constexpr double GATE_TOL = 1e-3;   // FZ-GATE-CONST-SB (SCI-DRZ-001 §7 冻结)

int g_failures = 0;

void check(bool ok, const char* what, const std::string& detail) {
    if (ok) {
        std::printf("[p1drz-disp009] PASS  %s | %s\n", what, detail.c_str());
    } else {
        std::printf("[p1drz-disp009] FAIL  %s | %s\n", what, detail.c_str());
        std::fprintf(stderr, "[p1drz-disp009] FAIL %s | %s\n", what,
                     detail.c_str());
        ++g_failures;
    }
}

struct CaseResult {
    double pixfrac = 0.0;
    std::size_t n_leaf = 0;
    double max_abs_dev = 0.0;   // 实测 max|S_p/B0 − 1|
    double rel_std = 0.0;
    double defect_dev = 0.0;    // 投影回 A_drop 分母后的 max|S/B0 − 1|
    double area_ratio = 0.0;    // oracle A_pixel/A_drop (独立几何)
    bool measured_green = false;
    bool defect_red = false;
};

template <typename Scalar>
bool run_case(double pixfrac, std::vector<TileAccumulatorT<Scalar>>& tiles,
              const FitsImage& img, DrizzleStats& st) {
    const DrizzleConfig cfg = p1drz::make_cfg(NSIDE, pixfrac, 1, true);
    DrizzleEngine eng;
    std::string err;
    const bool ok = eng.drizzleTiled_f64(img, cfg, nullptr, nullptr, nullptr,
                                         tiles, st, err);
    if (!ok) std::fprintf(stderr, "drizzle failed: %s\n", err.c_str());
    return ok;
}

// 判据本体: 给定逐 leaf 信号, 是否满足 |S_p/B0 − 1| < tol
bool gate_green(const std::vector<p1drz::LeafRec>& leafs, double b0,
                double tol) {
    if (leafs.empty()) return false;
    for (const auto& l : leafs) {
        if (!std::isfinite(l.signal)) return false;
        if (std::fabs(l.signal / b0 - 1.0) >= tol) return false;
    }
    return true;
}

// 逐 pixfrac 实测 + 负例控制投影
CaseResult evaluate(double pixfrac, const char* dump_dir) {
    CaseResult r;
    r.pixfrac = pixfrac;
    const FitsImage img = p1drz::fix_drz_a_const_sb(W, H, B0, SCALE, false);
    std::vector<TileAccumulatorT<double>> tiles;
    DrizzleStats st;
    if (!run_case<double>(pixfrac, tiles, img, st)) {
        std::fprintf(stderr, "[p1drz-disp009] pf=%.3f run failed\n", pixfrac);
        ++g_failures;
        return r;
    }
    const std::vector<p1drz::LeafRec> leafs = p1drz::extract_leafs(tiles, 9);
    r.n_leaf = leafs.size();
    const p1drz::UniformityOracle u = p1drz::oracle_const_sb(leafs, B0);
    r.max_abs_dev = u.max_abs_dev;
    r.rel_std = u.rel_std;
    r.measured_green = gate_green(leafs, B0, GATE_TOL);

    // 负例控制: 用独立 oracle 几何的 A_pixel/A_drop = 1/pf² 把实测值投影回
    // "分母取覆盖面积 D_p=Σ_j a_jp" 的口径 (N_p = D_p/pf², 只差该比值)。
    const int cx = W / 2, cy = H / 2;
    const double a_pix = p1drz::oracle_pixel_area(img.wcs, cx, cy);
    const double a_drop = p1drz::oracle_drop_area(img.wcs, (double)cx,
                                                  (double)cy, pixfrac);
    r.area_ratio = (a_drop > 0.0) ? (a_pix / a_drop) : 0.0;
    double worst = 0.0;
    for (const auto& l : leafs) {
        const double s_defect = l.signal * r.area_ratio;
        worst = std::max(worst, std::fabs(s_defect / B0 - 1.0));
    }
    r.defect_dev = worst;
    // "分母取覆盖面积 D_p" 的预测必须被本门判红 (非退化证据)
    r.defect_red = !(worst < GATE_TOL);

    std::printf("[p1drz-disp009] pf=%.3f leaf=%zu max|S/B0-1|=%.6e rel_std=%.3e "
                "A_pix/A_drop=%.9f | gate=%s defect_pred_dev=%.6e defect_gate=%s "
                "| 1/pf^2-1=%.6e\n",
                pixfrac, r.n_leaf, r.max_abs_dev, r.rel_std, r.area_ratio,
                r.measured_green ? "GREEN" : "RED", r.defect_dev,
                r.defect_red ? "RED" : "GREEN",
                1.0 / (pixfrac * pixfrac) - 1.0);
    std::fflush(stdout);

    // 逐字节证据: 同一 fixture 的产物转储 (elapsedSec 归零 → 只留科学载荷)
    if (dump_dir != nullptr) {
        char prefix[512];
        std::snprintf(prefix, sizeof(prefix), "%s/disp009_pf%04d", dump_dir,
                      (int)std::lround(pixfrac * 1000.0));
        DrizzleMeta meta;
        meta.filter = "TEST";
        meta.exposure_s = 1.0;
        meta.obs_time = "2026-01-01T00:00:00Z";
        DrizzleStats st_norm = st;
        st_norm.elapsedSec = 0.0;
        const DrizzleConfig cfg = p1drz::make_cfg(NSIDE, pixfrac, 1, true);
        DrizzleEngine eng;
        std::string werr;
        const std::string hiss = std::string(prefix) + ".norm.hiss";
        if (!eng.writeHisTilesT<double>(tiles, st_norm, img.wcs, cfg, meta, "",
                                        hiss, nullptr, nullptr, werr)) {
            std::fprintf(stderr, "[p1drz-disp009] write failed: %s\n",
                         werr.c_str());
            ++g_failures;
        }
        const std::string canon = std::string(prefix) + ".canon";
        FILE* f = std::fopen(canon.c_str(), "w");
        if (!f) {
            std::fprintf(stderr, "[p1drz-disp009] cannot open %s\n",
                         canon.c_str());
            ++g_failures;
        } else {
            std::vector<std::pair<uint64_t, const TileAccumulatorT<double>*>>
                sorted;
            for (const auto& t : tiles) {
                if (t.touched.empty()) continue;
                sorted.push_back({t.parent_ipix, &t});
            }
            std::sort(sorted.begin(), sorted.end(),
                      [](const auto& a, const auto& b) {
                          return a.first < b.first;
                      });
            for (const auto& [parent, t] : sorted) {
                std::vector<uint32_t> locals = t->touched;
                std::sort(locals.begin(), locals.end());
                for (uint32_t local : locals) {
                    const auto& acc = t->pixels[local];
                    uint64_t bf = 0, ba = 0, bv = 0;
                    std::memcpy(&bf, &acc.sumFlux, sizeof(double));
                    std::memcpy(&ba, &acc.sumArea, sizeof(double));
                    std::memcpy(&bv, &acc.sumVarNum, sizeof(double));
                    std::fprintf(f, "%llu %u %016llx %016llx %016llx %u\n",
                                 (unsigned long long)parent, local,
                                 (unsigned long long)bf,
                                 (unsigned long long)ba,
                                 (unsigned long long)bv, acc.nContrib);
                }
            }
            std::fclose(f);
        }
        std::printf("[p1drz-disp009] dumped %s.{norm.hiss,canon}\n",
                    prefix);
    }
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    const char* dump_dir = nullptr;
    if (argc >= 3 && std::strcmp(argv[1], "dump") == 0) dump_dir = argv[2];

    std::printf("[p1drz-disp009] DISP-DRZ-009 回归门: w_jp = a_jp/A_drop,j + "
                "N_p = Sum w_jp*A_pixel,j (SCI-DRZ-001 §5/§7/§10; "
                "FZ-GATE-CONST-SB tol=%.0e)\n",
                GATE_TOL);
    const double pixfracs[] = {1.0, 0.8, 0.6, 0.5};
    for (double pf : pixfracs) {
        const CaseResult r = evaluate(pf, dump_dir);
        char what[160];
        char det[256];
        std::snprintf(what, sizeof(what),
                      "P pf=%.3f 实测常量面亮度门 |S/B0-1|<1e-3", pf);
        std::snprintf(det, sizeof(det),
                      "max|S/B0-1|=%.6e (<%.0e), leaf=%zu", r.max_abs_dev,
                      GATE_TOL, r.n_leaf);
        check(r.measured_green, what, det);

        if (pf < 1.0) {
            // 负例控制只在 pixfrac<1 有意义: 两种归一分母相差 1/pf²。
            std::snprintf(what, sizeof(what),
                          "N pf=%.3f 负例控制: 分母取覆盖面积 D_p 必须判红", pf);
            std::snprintf(det, sizeof(det),
                          "defect_pred_dev=%.6e (须 >%.0e), 1/pf^2-1=%.6e",
                          r.defect_dev, GATE_TOL, 1.0 / (pf * pf) - 1.0);
            check(r.defect_red, what, det);
        } else {
            // pixfrac=1 端点: A_drop ≡ A_pixel ⇒ N_p ≡ D_p, 两种归一**恒等**,
            // 缺陷在此处必然零效应 (这正是默认值掩盖缺陷的机制, 也是向后兼容硬约束)。
            std::snprintf(what, sizeof(what),
                          "E pf=1.000 端点退化: A_pixel/A_drop == 1 (缺陷零效应)");
            std::snprintf(det, sizeof(det),
                          "A_pix/A_drop=%.12f (须 ==1), defect_pred_dev=%.3e",
                          r.area_ratio, r.defect_dev);
            check(std::fabs(r.area_ratio - 1.0) < 1e-12 && r.measured_green,
                  what, det);
        }
    }

    if (g_failures == 0) {
        std::printf("[p1drz-disp009] == PASS (0 失败) ==\n");
        return 0;
    }
    std::printf("[p1drz-disp009] == FAIL (%d 失败) ==\n", g_failures);
    return 1;
}

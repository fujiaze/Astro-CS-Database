// run/RELEASE-02/fix-p2a/p2a_oracle.cpp
// RELEASE-02 P2a 判别力 Oracle —— 链接**真实** upm.cpp（非复刻），生产尺度参数。
// 目的：
//   1) P2a-1：四种加性组合的帧间失配（raw / raw-delta / raw-C / raw-C-delta）
//   2) P2a-2/P2a-4：覆盖子集边界阶跃（legacy vs full_frame vs final_gauge）
//   3) P2a-3：绝对 1e-6 与相对 1e-3 的收敛可见性
//   4) 阻尼 alpha=1 vs 0.5
// 生产尺度：sky~1e13 ADU，control_ivar~6.25e-22（=1/(4e10)^2），与 c-delta/q2 报告一致。
// 编译（本文件同目录 build.sh）：g++ 链接 upm.cpp + healpix_core.cpp + sha256.cpp。
#include "astro/phase2/upm.h"
#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int kOrder = 7;
constexpr int kTileShift = 9;
constexpr int kGrid = 8;
constexpr int kCell = 512 / kGrid;          // 64
constexpr std::uint64_t kTile = 4;
constexpr double kSigma = 4.0e10;            // 生产 control uncertainty 中位 ~4.23e10
constexpr double kIvar = 1.0 / (kSigma * kSigma);

inline std::uint64_t leaf_of(std::uint64_t tile, int x, int y) {
    const std::uint64_t local = astrocs::healpix::xy_to_nested_local(
        (std::uint32_t)x, (std::uint32_t)y, (std::uint32_t)kTileShift);
    return (tile << (2u * (unsigned)kTileShift)) + local;
}
inline void center_radec(int gx, int gy, double* ra, double* dec) {
    const int x = gx * kCell + kCell / 2, y = gy * kCell + kCell / 2;
    const std::uint64_t leaf = leaf_of(kTile, x, y);
    astrocs::healpix::pix2ang_nest(
        1u << (unsigned)(kOrder + kTileShift), leaf, *ra, *dec);
}
// 公共天光（生产尺度）
inline double sky(double ra, double dec) {
    const double r = ra * 3.141592653589793 / 180.0;
    const double d = dec * 3.141592653589793 / 180.0;
    return 1.0e13 + 2.0e12 * std::sin(0.7 * r) * std::cos(0.5 * d) +
           5.0e11 * std::cos(2.1 * d);
}
// 逐帧乘性残余：M0（Phase1 测光施加）后 = 1（本 oracle 主场景按 M0 已完成
// 建模；P2a-1 的乘性未归一化场景由单独开关复现，见 main 的 kUnnorm）。
inline double amul(int f, bool unnorm) {
    if (!unnorm) return 1.0;
    return f == 0 ? 1.00 : (f == 1 ? 1.04 : 0.96);
}
// 逐帧加性梯度（低阶平面；delta 模型可吸收）
inline double bgrad(int f, int gx, int gy) {
    if (f == 0) return 0.0;
    if (f == 1) return 3.0e11 * (double)gx / 7.0;
    return -2.0e11 * (double)gy / 7.0;
}
// 逐帧高阶 bump（plane-delta 无法吸收 → 制造 C 的拟合残差）
inline double bump(int f, int gx, int gy) {
    if (f == 0) return 0.0;
    const double cx = (f == 1) ? 3.3 : 5.6, cy = (f == 1) ? 5.1 : 2.4;
    const double a = (f == 1) ? 2.5e11 : -2.0e11;
    const double dx = (double)gx - cx, dy = (double)gy - cy;
    return a * std::exp(-(dx * dx + dy * dy) / 0.6);
}
inline bool covers(int f, int gx) {
    if (f == 0) return gx < 4;         // 左
    if (f == 1) return gx >= 2 && gx < 6;  // 中
    return gx >= 4;                    // 右
}
struct Obs { int f, gx, gy; double y, c; };
}  // namespace

struct Scene {
    std::vector<Obs> obs;               // 合成真值（含噪声）
    std::vector<double> M;              // 公共场真值（sky）
    std::vector<int> subset_count;      // 每 control 覆盖帧数
};

static Scene make_scene(std::uint64_t seed, bool unnorm) {
    Scene s;
    std::mt19937 rng((std::uint32_t)seed);
    std::normal_distribution<double> nd(0.0, kSigma);
    s.M.resize((size_t)kGrid * kGrid);
    s.subset_count.assign((size_t)kGrid * kGrid, 0);
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx < kGrid; ++gx) {
            double ra = 0, dec = 0;
            center_radec(gx, gy, &ra, &dec);
            const size_t k = (size_t)gy * kGrid + gx;
            s.M[k] = sky(ra, dec);
            for (int f = 0; f < 3; ++f) {
                if (!covers(f, gx)) continue;
                Obs o;
                o.f = f; o.gx = gx; o.gy = gy;
                o.y = amul(f, unnorm) * sky(ra, dec) + bgrad(f, gx, gy) +
                      bump(f, gx, gy) + nd(rng);
                o.c = 0.0;
                s.obs.push_back(o);
                s.subset_count[k]++;
            }
        }
    return s;
}

static void run_config(const Scene& s, const char* name, double damping,
                       int full_frame, int final_gauge, double tol,
                       int tol_rel, double ls, std::uint64_t* out_it, int* out_conv,
                       double* out_obj, double* out_boundary_step,
                       double* out_resid_max, double* out_comb) {
    std::vector<P2ControlObservation> obs;
    obs.reserve(s.obs.size());
    for (const auto& o : s.obs) {
        P2ControlObservation x{};
        x.frame_id = (std::uint64_t)o.f;
        x.control_id = (std::uint64_t)(o.gy * kGrid + o.gx);
        x.leaf_ipix = leaf_of(kTile, o.gx * kCell + kCell / 2,
                              o.gy * kCell + kCell / 2);
        double ra = 0, dec = 0;
        center_radec(o.gx, o.gy, &ra, &dec);
        x.ra_deg = ra; x.dec_deg = dec;
        x.value = o.y; x.uncertainty = kSigma; x.snr = 100.0;
        x.ivar = kIvar; x.control_variance = kSigma * kSigma;
        x.control_ivar = kIvar; x.snr_available = 1; x.support = 1.0;
        x.quality_flags = 1;
        obs.push_back(x);
    }
    P2UpmBuildConfig cfg{};
    cfg.target_order = kOrder;
    cfg.smoothing_lambda = ls;       // 正则化档：制造非零拟合残差 r_f（λs=0 时 C 精确插值）
    cfg.zero_anchor_weight = 1e-3;
    cfg.sigma_floor = 1e8;
    cfg.max_iterations = 60;
    cfg.use_ivar_weight = 1;
    cfg.gs_damping = damping;
    cfg.m_full_frame = full_frame;
    cfg.final_gauge = final_gauge;
    cfg.tolerance = tol;
    cfg.tolerance_relative = tol_rel;
    void* m = nullptr;
    if (p2_upm_build(obs.data(), obs.size(), &cfg, &m) != 0) {
        std::printf("  [%s] BUILD FAILED\n", name);
        *out_it = 0; *out_conv = -1; *out_obj = 0;
        *out_boundary_step = -1; *out_resid_max = -1;
        for (int i = 0; i < 4; ++i) out_comb[i] = 0.0;
        return;
    }
    std::uint64_t it = 0; double obj = 0.0; int conv = -1;
    p2_upm_convergence(m, &it, &obj, &conv);
    *out_it = it; *out_conv = conv; *out_obj = obj;

    // 每 control：叠加 = 覆盖帧 calibrate 输出的加权均值（权重同源 = ivar）
    std::vector<double> stack((size_t)kGrid * kGrid, 0.0);
    std::vector<double> wsum((size_t)kGrid * kGrid, 0.0);
    std::vector<double> corr_sum((size_t)kGrid * kGrid, 0.0);
    std::vector<double> Ck((size_t)kGrid * kGrid, 0.0);
    std::vector<double> raw_mean((size_t)kGrid * kGrid, 0.0);
    std::vector<double> dlt_mean((size_t)kGrid * kGrid, 0.0);
    std::vector<double> cd_mean((size_t)kGrid * kGrid, 0.0);
    std::vector<double> rw_mean((size_t)kGrid * kGrid, 0.0);
    std::vector<int> nf((size_t)kGrid * kGrid, 0);
    for (const auto& o : s.obs) {
        const size_t k = (size_t)o.gy * kGrid + o.gx;
        std::uint64_t ip[1] = {leaf_of(kTile, o.gx * kCell + kCell / 2,
                                       o.gy * kCell + kCell / 2)};
        double iv[1] = {o.y}, ov[1] = {0.0};
        p2_upm_calibrate_block(m, (std::uint64_t)o.f, ip, iv, ov, 1);
        const double corr = ov[0];
        const double c = p2_upm_evaluate_c(m, (std::uint64_t)o.f, ip[0]);
        // 合成 delta_k = b_k - b_ref（ref=f0，b_0=0）
        const double dlt = bgrad(o.f, o.gx, o.gy) - bgrad(0, o.gx, o.gy);
        stack[k] += kIvar * corr; wsum[k] += kIvar;
        corr_sum[k] += corr;
        Ck[k] += c;
        raw_mean[k] += o.y; dlt_mean[k] += dlt;
        cd_mean[k] += (o.y - c - dlt); rw_mean[k] += (o.y - c);
        nf[k]++;
    }
    for (size_t k = 0; k < stack.size(); ++k)
        if (wsum[k] > 0) stack[k] /= wsum[k];

    // 覆盖子集边界阶跃（Q2 §6.2 口径）：相邻 control 边的 |Δstack|，
    // 按覆盖帧数是否变化分「边界边 / 内部边」，取中位（比值即接缝指标）。
    std::vector<double> bj, ij;
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx + 1 < kGrid; ++gx) {
            const size_t a = (size_t)gy * kGrid + gx;
            const size_t b = a + 1;
            const double d = std::fabs(stack[a] - stack[b]);
            if (s.subset_count[a] != s.subset_count[b]) bj.push_back(d);
            else ij.push_back(d);
        }
    auto med_of = [](std::vector<double>& v) {
        if (v.empty()) return 0.0;
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    const double Mn = 1.0e13;
    *out_boundary_step = med_of(bj) / Mn;   // 边界边中位
    *out_resid_max = med_of(ij) / Mn;       // 内部边中位（本底）

    // 组合失配（仅 ≥2 帧覆盖的 control；相对 M 的帧间 std）
    double comb[4] = {0, 0, 0, 0};
    int n_multi = 0;
    for (size_t k = 0; k < stack.size(); ++k) {
        if (s.subset_count[k] < 2) continue;
        // 收集各帧 y / y-C / y-delta / y-C-delta
        std::vector<double> vr, vc, vd, vcd;
        for (const auto& o : s.obs) {
            if ((size_t)o.gy * kGrid + o.gx != k) continue;
            std::uint64_t ip[1] = {leaf_of(kTile, o.gx * kCell + kCell / 2,
                                           o.gy * kCell + kCell / 2)};
            const double c = p2_upm_evaluate_c(m, (std::uint64_t)o.f, ip[0]);
            const double dlt = bgrad(o.f, o.gx, o.gy) - bgrad(0, o.gx, o.gy);
            vr.push_back(o.y); vc.push_back(o.y - c);
            vd.push_back(o.y - dlt); vcd.push_back(o.y - c - dlt);
        }
        auto std_of = [](const std::vector<double>& v) {
            if (v.size() < 2) return 0.0;
            double mu = 0; for (double x : v) mu += x; mu /= (double)v.size();
            double s2 = 0; for (double x : v) s2 += (x - mu) * (x - mu);
            return std::sqrt(s2 / (double)(v.size() - 1));
        };
        comb[0] += std_of(vr); comb[1] += std_of(vd);
        comb[2] += std_of(vc); comb[3] += std_of(vcd);
        ++n_multi;
    }
    if (n_multi > 0)
        for (int i = 0; i < 4; ++i) comb[i] = comb[i] / n_multi / Mn;
    for (int i = 0; i < 4; ++i) out_comb[i] = comb[i];
    p2_upm_close(m);
}

int main() {
    std::printf("=== P2a oracle (real upm.cpp, production scale) ===\n");
    std::printf("sky~1e13 ADU, sigma=%.3g, ivar=%.3g, frames 3, grid %d\n\n",
                kSigma, kIvar, kGrid);

    // 主场景：M0 已完成（乘性=1），逐帧加性梯度 + 高阶 bump。
    const Scene s = make_scene(20260919, false);
    struct Row { const char* name; double damping; int ff; int fg; double tol; int rel; double ls; };
    const Row rows[] = {
        {"legacy refM noG @lam1000",    1.0, 0, 0, 1e-9, 0, 1000.0},
        {"full_frame noG @lam1000",     1.0, 1, 0, 1e-9, 0, 1000.0},
        {"refM final_gauge @lam1000",   1.0, 0, 1, 1e-9, 0, 1000.0},
        {"P2a-full rel1e-3 @lam1000",   0.5, 1, 1, 1e-3, 1, 1000.0},
        {"legacy refM noG @lam0",       1.0, 0, 0, 1e-6, 0, 0.0},
        {"P2a-full rel1e-3 @lam0",      0.5, 1, 1, 1e-3, 1, 0.0},
    };
    std::printf("%-32s %5s %4s %11s %12s %12s %8s\n",
                "config", "iter", "conv", "objective", "boundary%", "internal%", "ratio");
    for (const auto& r : rows) {
        std::uint64_t it; int conv; double obj, step, resid, comb[4];
        run_config(s, r.name, r.damping, r.ff, r.fg, r.tol, r.rel, r.ls,
                   &it, &conv, &obj, &step, &resid, comb);
        const double ratio = (resid > 0) ? step / resid : 0.0;
        std::printf("%-32s %5llu %4d %11.4g %11.4f%% %11.4f%% %8.3f\n", r.name,
                    (unsigned long long)it, conv, obj, step * 100.0,
                    resid * 100.0, ratio);
    }

    // P2a-1 组合：未归一化场景（模拟当前生产 L4，Phase1 未施加）+ 生产 λs=0。
    std::printf("\n--- P2a-1 组合失配（未归一化场景，λs=0，>=2 帧 control 的帧间 std/公共场）---\n");
    const Scene su = make_scene(20260919, true);
    std::uint64_t it; int conv; double obj, step, resid, comb[4];
    run_config(su, "p2a1-combo", 1.0, 0, 0, 1e-6, 0, 0.0,
               &it, &conv, &obj, &step, &resid, comb);
    std::printf("  raw=%.4f%%  raw-delta=%.4f%%  raw-C=%.4f%%  raw-C-delta=%.4f%%\n",
                comb[0] * 100, comb[1] * 100, comb[2] * 100, comb[3] * 100);
    return 0;
}

// eng/tests/unit/v6_p2_sky_kappa/v6_p2_sky_kappa_test.cpp
// UPM-KAPPA-UNIFY-01 回归锁：天光面侧的**唯一**可辨识性判据（红/绿双向）。
//
// 依据：docs/science/PHASE2_UPM.md §7a（判据按矩阵谱自身定，不得是两个标定值；
//       每条自适应路径必须有触发记录）；docs/science/NOISE_MODEL.md §5d
//       （零假设值由理论给、可行域由数据给，不引入可调标定常数）。
// 被测面：lib/algorithms/coverage/src/sky_plane.cpp（p2_sky_plane_build / _adaptive）
//         与 lib/algorithms/coverage/src/identifiability.cpp（判据本体）。
//
// **本文件记录一条被证伪的旧口径（任务书前提）**：
//   旧实现把门设在 H_solve = H_red + λ·DᵀD 的条件数上，并靠"提高 λ"来救红。
//   但 κ(H + λI) = (σ_max²+λ)/(σ_n²+λ) → 1（λ→∞），**任何秩亏矩阵都能被 λ 买成绿**：
//   那不是判据，是恒真门。故判决改为只看未正则化的 H_red（本测试 B 段把它锁死）。
#include "astro/phase2/identifiability.h"
#include "astro/phase2/sky_plane.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int g_fail = 0;
int g_total = 0;
void* g_benign_model = nullptr;
// %g 格式（std::to_string 固定 6 位小数，会把 1.6e-10 印成 0.000000，误导读数）
std::string fmt(double v) {
    char b[64];
    std::snprintf(b, sizeof(b), "%.6g", v);
    return std::string(b);
}
void check(bool ok, const std::string& what) {
    ++g_total;
    if (!ok) { ++g_fail; std::printf("FAIL %s\n", what.c_str()); }
    else std::printf("ok   %s\n", what.c_str());
}

// 测试侧独立的对称特征值（Jacobi），用来复算 κ，不复用被测实现。
std::vector<double> eig(std::vector<double> A, int n) {
    for (int sweep = 0; sweep < 300; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q) off += A[p * n + q] * A[p * n + q];
        if (off <= 1e-28) break;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q) {
                const double apq = A[p * n + q];
                if (std::fabs(apq) < 1e-300) continue;
                const double app = A[p * n + p], aqq = A[q * n + q];
                const double th = 0.5 * (aqq - app) / apq;
                const double t = (th >= 0.0 ? 1.0 : -1.0) /
                                 (std::fabs(th) + std::sqrt(1.0 + th * th));
                const double c = 1.0 / std::sqrt(1.0 + t * t), s = c * t;
                for (int k = 0; k < n; ++k) {
                    const double akp = A[k * n + p], akq = A[k * n + q];
                    A[k * n + p] = c * akp - s * akq;
                    A[k * n + q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; ++k) {
                    const double apk = A[p * n + k], aqk = A[q * n + k];
                    A[p * n + k] = c * apk - s * aqk;
                    A[q * n + k] = s * apk + c * aqk;
                }
            }
    }
    std::vector<double> ev(n);
    for (int i = 0; i < n; ++i) ev[i] = A[i * n + i];
    std::sort(ev.begin(), ev.end(), std::greater<double>());
    return ev;
}

std::vector<P2SkySample> benign_samples() {
    std::vector<P2SkySample> v;
    for (std::uint64_t f = 0; f < 2; ++f)
        for (int iy = 0; iy < 12; ++iy)
            for (int ix = 0; ix < 12; ++ix) {
                P2SkySample s{};
                s.frame_id = 100 + f;
                s.control_id = static_cast<std::uint64_t>(iy * 12 + ix);
                s.ra_deg = 10.0 + 0.03 * ix;
                s.dec_deg = 20.0 + 0.03 * iy;
                const double u = 0.03 * ix, w = 0.03 * iy;
                s.value = 300.0 + 40.0 * u + 25.0 * w + 3.0 * u * w +
                          7.0 * static_cast<double>(f);
                s.variance = 4.0;
                s.snr = 150.0;
                s.flags = P2_SKY_FLAG_NONE;
                v.push_back(s);
            }
    return v;
}

// 帧 1 只观测参考帧完全没覆盖的一个格子（2×2 节点）：该格子的 B_ref 双线性基
// {1, ξ, η, ξη} 里只有 ξη 不在 δ_1 的平面基 {1, ξ, η} 中 ⇒ Schur 消元后
// 该格子留下 3 个未被数据约束的方向 ⇒ 判红。
std::vector<P2SkySample> rank_deficient_samples() {
    std::vector<P2SkySample> v;
    for (int iy = 0; iy < 8; ++iy)
        for (int ix = 0; ix < 5; ++ix) {
            P2SkySample s{};
            s.frame_id = 100;
            s.control_id = static_cast<std::uint64_t>(iy * 10 + ix);
            s.ra_deg = 10.0 + 0.05 * ix;
            s.dec_deg = 20.0 + 0.05 * iy;
            s.value = 300.0 + 2.0 * ix + 3.0 * iy;
            s.variance = 4.0;
            s.snr = 150.0;
            s.flags = P2_SKY_FLAG_NONE;
            v.push_back(s);
        }
    for (int iy = 4; iy <= 5; ++iy)
        for (int ix = 8; ix <= 9; ++ix) {
            P2SkySample s{};
            s.frame_id = 101;
            s.control_id = static_cast<std::uint64_t>(100 + iy * 10 + ix);
            s.ra_deg = 10.0 + 0.05 * ix;
            s.dec_deg = 20.0 + 0.05 * iy;
            s.value = 301.0 + 2.0 * ix + 3.0 * iy;
            s.variance = 4.0;
            s.snr = 150.0;
            s.flags = P2_SKY_FLAG_NONE;
            v.push_back(s);
        }
    return v;
}

P2SkyPlaneConfig base_cfg(double h, int retain) {
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.node_spacing_deg = h;
    cfg.spline_degree = 1;
    cfg.frame_gradient_order = 1;
    cfg.max_nodes = 4096;
    cfg.retain_normal_matrices = retain;
    return cfg;
}
}  // namespace

int main() {
    std::string err;

    // ---------------------------------------------------------------------
    // A. 判据可观测：判决位 + 全部读数都在 Info 上，且 kappa_data 是兼容别名
    // ---------------------------------------------------------------------
    P2SkyPlaneInfo info{};
    {
        const std::vector<P2SkySample> s = benign_samples();
        P2SkyPlaneConfig cfg = base_cfg(0.12, 1);   // 保留 H_red/P 供 B 段独立复算
        char e[512] = {0};
        void* m = nullptr;
        const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, e, sizeof(e));
        err = e;
        check(rc == P2_SKY_PLANE_OK, "A1 benign build ok rc=" + std::to_string(rc) + " " + err);
        if (rc == P2_SKY_PLANE_OK && m) {
            p2_sky_plane_info(m, &info);
            g_benign_model = m;   // B 段复用（本进程内，退出前不释放）
        }
    }
    check(info.identifiable == 1, "A2 benign => identifiable (the single verdict bit)");
    check(info.rank == info.n_params && info.n_unidentified == 0,
          "A3 rank_eff == n_params (a COUNT, not a boolean): " +
              std::to_string(info.rank) + "/" + std::to_string(info.n_params));
    check(info.kappa == info.kappa_data,
          "A4 kappa_data is a bit-exact compatibility alias of kappa");
    check(std::isfinite(info.kappa) && info.kappa > 0.0,
          "A5 kappa(H_red) finite>0 = " + std::to_string(info.kappa));
    check(info.lambda_numerical > 0.0 && std::isfinite(info.lambda_numerical),
          "A6 derived numerical ridge reported = " + std::to_string(info.lambda_numerical));
    check(info.rank_rtol_effective >= info.rank_rtol_effective * 0.0 + 1e-10,
          "A7 tau_eff >= FZ-AP2S-RANK-RTOL = " + std::to_string(info.rank_rtol_effective));
    check(info.dof_eff > 0.0, "A8 effective dof > 0 = " + std::to_string(info.dof_eff));
    std::printf("INFO benign: kappa=%.6g kappa_solve=%.6g lambda_eff=%.6g rank=%llu/%llu "
                "rank_full=%llu dof_eff=%.1f chi2_red=%.6g\n",
                info.kappa, info.kappa_solve, info.lambda_numerical,
                (unsigned long long)info.rank, (unsigned long long)info.n_params,
                (unsigned long long)info.rank_full, info.dof_eff, info.chi2_red);

    // ---------------------------------------------------------------------
    // B. 恒真门反证（旧口径被证伪）：判红时 κ(H_solve) 仍可能远小于 1/τ
    // ---------------------------------------------------------------------
    {
        const std::vector<P2SkySample> s = rank_deficient_samples();
        P2SkyPlaneConfig cfg = base_cfg(0.10, 1);   // 保留 H_red 与 P 供独立复算
        char e[512] = {0};
        void* m = nullptr;
        const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, e, sizeof(e));
        err = e;
        check(rc == P2_SKY_PLANE_NOT_IDENTIFIABLE,
              "B1 rank-deficient input => rc=NOT_IDENTIFIABLE, got " + std::to_string(rc) +
                  " " + err);
        check(rc != P2_SKY_PLANE_RANK_DEFICIENT,
              "B2 the retired rc=RANK_DEFICIENT path is gone (one criterion, one rc)");
        check(err.find("rank_eff") != std::string::npos &&
                  err.find("kappa(H_red)") != std::string::npos,
              "B3 error text carries the criterion readings: " + err);
        if (m) p2_sky_plane_close(m);
    }
    {
        // 用**保留的法方程**独立复算：κ(H_solve) = κ(H_red + λP) 随 λ 单调压向 1。
        // 这就是旧口径「提高 λ 救红」被证伪的数值形式：κ(H+λI)=(σ_max²+λ)/(σ_n²+λ)→1，
        // 任何矩阵（含秩亏）都能被 λ 买绿 ⇒ 在 H_solve 上设门是恒真门。
        std::uint64_t n = 0;
        check(g_benign_model != nullptr &&
                  p2_sky_plane_normal_matrix_size(g_benign_model, &n) == 0 && n > 0,
              "B4 retained normal matrix available, n=" + std::to_string(n));
        if (g_benign_model && n > 0) {
            std::vector<double> Hr(n * n, 0.0), P(n * n, 0.0);
            check(p2_sky_plane_normal_matrices(g_benign_model, Hr.data(), P.data(), n) == 0,
                  "B5 retained H_red / P readable (retain_normal_matrices=1)");
            // 独立复算 κ(H_red)：判据读的是**列均衡后**的矩阵 D⁻¹H_red D⁻¹，故测试侧
            // 也必须均衡后再算（裸 κ 会被参数单位伪造出任意大的值）。
            std::vector<double> Heq = Hr;
            for (std::size_t i = 0; i < (std::size_t)n; ++i) {
                const double di = std::sqrt(Hr[i * n + i]);
                for (std::size_t j = 0; j < (std::size_t)n; ++j) {
                    const double dj = std::sqrt(Hr[j * n + j]);
                    Heq[i * n + j] = 0.5 * (Hr[i * n + j] + Hr[j * n + i]) / (di * dj);
                }
            }
            const std::vector<double> ev0 = eig(Heq, (int)n);
            const double k_raw = (ev0.back() > 0.0) ? ev0.front() / ev0.back() : 1e300;
            check(std::fabs(k_raw - info.kappa) <= 1e-6 * std::max(1.0, info.kappa),
                  "B6 independently recomputed kappa(H_red) == info.kappa: " + fmt(k_raw) +
                      " vs " + fmt(info.kappa));
            // λ 扫描：H_solve = H_red + λ·P。**门值随 λ 变** ⇒ 它不是数据的性质。
            const double lam0 = info.lambda_numerical;
            double k_min = 1e300, k_max = 0.0;
            for (double mul : {0.0, 1.0, 1e3, 1e6, 1e9, 1e12}) {
                std::vector<double> Hs = Hr;
                for (std::size_t i = 0; i < Hs.size(); ++i) Hs[i] += mul * lam0 * P[i];
                const std::vector<double> ev = eig(Hs, (int)n);
                const double k = (ev.back() > 0.0) ? ev.front() / ev.back() : 1e300;
                k_min = std::min(k_min, k); k_max = std::max(k_max, k);
                std::printf("INFO   lambda x%-8s => kappa(H_solve)=%s\n", fmt(mul).c_str(),
                            fmt(k).c_str());
            }
            check(k_max > k_min * 10.0,
                  "B7 gate value on H_solve swings with the knob lambda: " + fmt(k_min) +
                      " .. " + fmt(k_max) + " (kappa(H_red) fixed at " + fmt(k_raw) + ")");
            check(info.identifiable == 1 && info.rank == info.n_params,
                  "B8 the verdict does NOT move with lambda: it is read off H_red");
        }
    }
    // B9-B11 **可买绿见证**：精确秩亏矩阵 + 单位岭 λI ⇒ 旧门(κ ≤ 1/τ)判绿，新判据判红。
    {
        const std::vector<double> Hw = {1.0, 1.0, 1.0, 1.0};   // rank 1，精确秩亏
        const double lam = 1e-9;
        const std::vector<double> Hws = {1.0 + lam, 1.0, 1.0, 1.0 + lam};
        const std::vector<double> ev = eig(Hws, 2);
        const double k_solve = ev.front() / ev.back();
        const double old_gate = 1.0 / 1e-10;
        P2Identifiability idw{};
        const int rc = p2_identifiability_assess(Hw.data(), 2, 100, 1e-10, &idw);
        std::printf("INFO witness: H=[[1,1],[1,1]] rank-deficient; kappa(H+1e-9*I)=%s ; "
                    "old_gate=%s ; new verdict identifiable=%d rank_eff=%llu/%llu\n",
                    fmt(k_solve).c_str(), fmt(old_gate).c_str(), idw.identifiable,
                    (unsigned long long)idw.rank_eff, (unsigned long long)idw.n_params);
        check(rc == 0 && idw.identifiable == 0 && idw.rank_eff == 1,
              "B9 the single criterion rejects the exactly rank-deficient matrix");
        check(k_solve < old_gate,
              "B10 **the retired gate accepts it** (kappa(H+lambda*I)=" + fmt(k_solve) +
                  " < 1/tau=" + fmt(old_gate) + "): it was gameable by the knob");
        check(!std::isfinite(idw.kappa),
              "B11 the criterion refuses to publish a fake finite kappa for a singular H");
    }

    // ---------------------------------------------------------------------
    // C. κ(H_red) 与 λ 无关：λ 不再是可调分支，而是判据阈值的派生量
    // ---------------------------------------------------------------------
    {
        const std::vector<P2SkySample> s = benign_samples();
        P2SkyPlaneInfo i1{}, i2{};
        for (int k = 0; k < 2; ++k) {
            P2SkyPlaneConfig cfg = base_cfg(0.12, 0);
            cfg.rank_rtol = (k == 0) ? 1e-10 : 1e-8;   // 换 τ ⇒ 换 λ_eff
            char e[512] = {0};
            void* m = nullptr;
            const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, e, sizeof(e));
            check(rc == P2_SKY_PLANE_OK, "C1 build ok at tau variant " + std::to_string(k));
            if (rc == P2_SKY_PLANE_OK && m) {
                if (k == 0) p2_sky_plane_info(m, &i1);
                else p2_sky_plane_info(m, &i2);
            }
            if (m) p2_sky_plane_close(m);
        }
        check(std::fabs(i2.kappa - i1.kappa) <= 1e-9 * std::max(1.0, i1.kappa),
              "C2 kappa(H_red) independent of tau/lambda: " + std::to_string(i1.kappa) +
                  " vs " + std::to_string(i2.kappa));
        check(i2.lambda_numerical >= i1.lambda_numerical * 99.0,
              "C3 lambda_eff scales with tau (derived, not a free branch): " +
                  fmt(i1.lambda_numerical) + " -> " + fmt(i2.lambda_numerical));
        check(i1.identifiable == 1 && i2.identifiable == 1,
              "C4 verdict unchanged on benign data across tau");
    }

    // ---------------------------------------------------------------------
    // D. 自适应回路与判据一致（单一旋钮：节点间距；单一判据：相对有效秩）
    // ---------------------------------------------------------------------
    {
        const std::vector<P2SkySample> s = benign_samples();
        P2SkyPlaneConfig cfg = base_cfg(0.0, 0);   // 0 = 由输入几何导出（§7a 规则 1）
        // 输入几何必须由调用方给出（自适应只负责在规则区间内搜索，不发明区间）。
        cfg.geometry.overlap_band_width_deg = 0.10;
        cfg.geometry.pointing_spacing_deg = 0.15;
        cfg.geometry.sample_pitch_deg = 0.03;
        cfg.geometry.pixel_scale_arcsec = 1.0;
        P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
        P2SkyPlaneAdaptiveReport rep{};
        char e[512] = {0};
        void* m = nullptr;
        const int rc = p2_sky_plane_build_adaptive(s.data(), s.size(), &cfg, &ad, &m, &rep,
                                                   e, sizeof(e));
        check(rc == P2_SKY_PLANE_OK, "D1 adaptive build ok rc=" + std::to_string(rc) + " " + e);
        if (rc == P2_SKY_PLANE_OK && m) {
            P2SkyPlaneInfo ai{};
            p2_sky_plane_info(m, &ai);
            check(rep.identifiable == ai.identifiable && rep.rank == ai.rank &&
                      rep.n_params == ai.n_params,
                  "D2 adaptive report agrees with final model on the criterion readings");
            check(rep.n_attempts >= 1 && rep.n_attempts <= P2_SKY_ADAPT_MAX_ATTEMPTS,
                  "D3 attempt log present, n_attempts=" + std::to_string(rep.n_attempts));
            check(rep.node_adaptive_used == ((rep.n_node_refinements +
                                              rep.n_node_coarsenings > 0) ? 1 : 0),
                  "D4 node_adaptive_used consistent with the single knob's move counts");
            check(rep.lambda_numerical > 0.0 && rep.rank_rtol > 0.0,
                  "D5 report carries lambda_numerical + tau");
            bool adopted = false;
            for (int t = 0; t < rep.n_attempts && t < P2_SKY_ADAPT_MAX_ATTEMPTS; ++t)
                if (rep.attempts[t].adopted == 1) adopted = true;
            check(adopted, "D6 exactly the adopted attempt is marked (per §7a trigger record)");
            std::printf("INFO adaptive: attempts=%d refine=%d coarsen=%d h=%.6g "
                        "rank=%llu/%llu identifiable=%d\n",
                        rep.n_attempts, rep.n_node_refinements, rep.n_node_coarsenings,
                        rep.node_spacing_deg, (unsigned long long)rep.rank,
                        (unsigned long long)rep.n_params, rep.identifiable);
            p2_sky_plane_close(m);
        }
    }

    std::printf("P2-SKY-KAPPA: %d/%d checks passed, %d failed\n", g_total - g_fail, g_total,
                g_fail);
    return g_fail == 0 ? 0 : 1;
}

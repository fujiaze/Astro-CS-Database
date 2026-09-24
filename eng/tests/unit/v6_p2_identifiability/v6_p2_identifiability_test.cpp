// eng/tests/unit/v6_p2_identifiability/v6_p2_identifiability_test.cpp
//
// UPM-KAPPA-UNIFY-01：**唯一**「可辨识性 / 病态」判据的红/绿双向回归锁。
//
// 依据：
//   docs/science/PHASE2_UPM.md §7a:196-198（κ 上限不得是两个标定值；判据按矩阵谱
//     自身定，相对 rank_rtol 口径）；§7a:199-200（每条自适应路径必须有触发记录）。
//   docs/science/NOISE_MODEL.md §5d（范式：零假设值由理论给、可行域由数据给，
//     不引入可调标定常数）。
//   一手文献：Andrae, Schulze-Hartung & Melchior 2010, arXiv:1012.3754 式(8)(9)
//     （dof = N − rank(X)，不是 N − P）；Hansen, Regularization Tools v4.1 手册
//     §1 第 2 条 / §2.7.3（正则化 = 换一个"new problem"，其条件数不能判原问题）。
//   开源实现同族默认阈值：numpy.linalg.matrix_rank（tol = S.max()*max(M,N)*eps）、
//     scipy.linalg.lstsq/pinv（cond 默认 max(M,N)*eps）、Eigen FullPivLU/
//     ColPivHouseholderQR（eps*diagSize）、Ceres Covariance
//     （min_reciprocal_condition_number = 1e-14，作用在 Jacobian J 上）。
//
// 被测面：
//   lib/algorithms/coverage/src/identifiability.cpp（判据本体）
//   lib/algorithms/coverage/src/sky_plane.cpp（天光面侧接线，红/绿双向）
#include "astro/phase2/identifiability.h"
#include "astro/phase2/sky_plane.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_total = 0;
void check(bool ok, const std::string& what) {
    ++g_total;
    if (!ok) { ++g_fail; std::printf("  FAIL: %s\n", what.c_str()); }
    else std::printf("  ok  : %s\n", what.c_str());
}
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// ---------------------------------------------------------------------------
// 独立 Oracle：对称矩阵特征值的 Jacobi 实现（测试侧自带，不复用被测实现）
// ---------------------------------------------------------------------------
std::vector<double> oracle_eig(std::vector<double> A, int n) {
    for (int sweep = 0; sweep < 200; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q) off += A[p * n + q] * A[p * n + q];
        if (off <= 1e-30) break;
        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                const double apq = A[p * n + q];
                if (std::fabs(apq) < 1e-300) continue;
                const double app = A[p * n + p], aqq = A[q * n + q];
                const double theta = 0.5 * (aqq - app) / apq;
                const double t = (theta >= 0.0 ? 1.0 : -1.0) /
                                 (std::fabs(theta) + std::sqrt(1.0 + theta * theta));
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
    }
    std::vector<double> ev(n);
    for (int i = 0; i < n; ++i) ev[i] = A[i * n + i];
    std::sort(ev.begin(), ev.end(), std::greater<double>());
    return ev;
}

// 列均衡（测试侧独立重算）：H_eq = D⁻¹ H D⁻¹
std::vector<double> equilibrate(const std::vector<double>& H, int n) {
    std::vector<double> E((std::size_t)n * n, 0.0), D(n);
    for (int i = 0; i < n; ++i) D[i] = std::sqrt(H[(std::size_t)i * n + i]);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            E[(std::size_t)i * n + j] = H[(std::size_t)i * n + j] / (D[i] * D[j]);
    return E;
}

std::vector<double> diag_mat(const std::vector<double>& d) {
    const int n = (int)d.size();
    std::vector<double> H((std::size_t)n * n, 0.0);
    for (int i = 0; i < n; ++i) H[(std::size_t)i * n + i] = d[(std::size_t)i];
    return H;
}

// ===========================================================================
// A. 判据本体：正例（良性满秩）必须过
// ===========================================================================
void test_criterion_positive() {
    std::printf("[criterion_positive]\n");
    // 单位矩阵（列均衡后不变）：r_eff == n，κ == 1
    const std::vector<double> I = diag_mat({1.0, 1.0, 1.0, 1.0});
    P2Identifiability v{};
    check(p2_identifiability_assess(I.data(), 4, 100, 1e-10, &v) == 0, "A1 rc==0");
    check(v.identifiable == 1, "A2 identity matrix => identifiable");
    check(v.rank_eff == 4 && v.n_unidentified == 0, "A3 rank_eff==4, no unidentified");
    check(near(v.kappa, 1.0, 1e-9), "A4 kappa==1");
    // A5-A7 良性但**有相关结构**：2×2 单位对角、相关系数 ρ=0.3
    //   （对角矩阵在列均衡下退化成单位阵——那是均衡的功劳，不能用来测病态；
    //     病态必须来自"方向之间几乎共线"，即非对角项）
    const std::vector<double> C2 = {1.0, 0.3, 0.3, 1.0};
    P2Identifiability v2{};
    check(p2_identifiability_assess(C2.data(), 2, 100, 1e-10, &v2) == 0, "A5 rc==0");
    check(v2.identifiable == 1, "A6 rho=0.3 => identifiable");
    check(near(v2.kappa, 1.3 / 0.7, 1e-9), "A7 kappa == (1+rho)/(1-rho) == 1.857");
    // A8 病态但仍在门内：ρ = 0.9999 ⇒ κ ≈ 2e4 < 1e10 ⇒ 仍判绿（判据不误伤）
    const std::vector<double> C3 = {1.0, 0.9999, 0.9999, 1.0};
    P2Identifiability v3{};
    check(p2_identifiability_assess(C3.data(), 2, 100, 1e-10, &v3) == 0, "A8 rc==0");
    check(v3.identifiable == 1, "A9 rho=0.9999 (kappa~2e4) => still identifiable");
    std::printf("  INFO rho=0.3: kappa=%.6g rank_eff=%llu tau_eff=%.3e ; rho=0.9999: kappa=%.6g\n",
                v2.kappa, (unsigned long long)v2.rank_eff, v2.rank_rtol_effective, v3.kappa);
}

// ===========================================================================
// B. 判据本体：负例（秩亏/病态）必须判红——门不是恒真门
// ===========================================================================
void test_criterion_negative() {
    std::printf("[criterion_negative]\n");
    // B1 精确秩亏：两列完全共线（ρ=1）⇒ λ_min=0
    const std::vector<double> R1 = {1.0, 1.0, 1.0, 1.0};
    P2Identifiability v{};
    check(p2_identifiability_assess(R1.data(), 2, 100, 1e-10, &v) == 0, "B1 rc==0");
    check(v.identifiable == 0, "B2 exactly rank-deficient (rho=1) => RED");
    check(v.rank_eff == 1 && v.n_unidentified == 1, "B3 rank_eff==1, unidentified==1");
    check(!std::isfinite(v.kappa), "B4 rank-deficient kappa==+inf (no fake value)");
    // B5 零对角：该参数**完全没有信息** ⇒ 计入未约束方向（不是"无法评估"）
    const std::vector<double> Z = diag_mat({1.0, 1.0, 1.0, 0.0});
    P2Identifiability vz{};
    check(p2_identifiability_assess(Z.data(), 4, 100, 1e-10, &vz) == 0, "B5 rc==0");
    check(vz.identifiable == 0 && vz.rank_eff == 3 && vz.n_unidentified == 1,
          "B6 zero diagonal => 1 unidentified direction, rank_eff==3");
    check(!std::isfinite(vz.kappa), "B7 kappa==+inf when an unconstrained direction exists");
    // B8-B11 阈值可红可绿：ρ = 1 − 2e-10 ⇒ λ_min/λ_max ≈ 1e-10，正好跨在 τ 两侧
    const double rho = 1.0 - 2e-10;
    const std::vector<double> R2 = {1.0, rho, rho, 1.0};
    P2Identifiability vlo{}, vhi{}, vf{};
    check(p2_identifiability_assess(R2.data(), 2, 100, 1e-11, &vlo) == 0, "B8 rc==0");
    check(vlo.identifiable == 1, "B9 tau=1e-11 < gap => GREEN");
    check(p2_identifiability_assess(R2.data(), 2, 100, 1e-9, &vhi) == 0, "B10 rc==0");
    check(vhi.identifiable == 0, "B11 tau=1e-9 > gap => RED");
    check(p2_identifiability_assess(R2.data(), 2, 100, 1e-10, &vf) == 0, "B12 rc==0");
    check(vf.identifiable == 0, "B13 tau=1e-10 (frozen value) => RED on this gap");
    // B14-B15 非正定：对角为正但矩阵不定（[[1,2],[2,1]] ⇒ λ = 3, −1）
    const std::vector<double> N = {1.0, 2.0, 2.0, 1.0};
    P2Identifiability v3{};
    check(p2_identifiability_assess(N.data(), 2, 100, 1e-10, &v3) == 0, "B14 rc==0");
    check(v3.identifiable == 0 && !std::isfinite(v3.kappa),
          "B15 indefinite (lambda_min<0) => RED, kappa=+inf");
    // B16-B17 非法输入：负对角 ⇒ 不是法方程，明确拒绝（不猜、不用 |·| 蒙）
    check(p2_identifiability_assess(nullptr, 4, 100, 1e-10, &v3) == 1, "B16 null H => rc=1");
    const std::vector<double> bad = diag_mat({1.0, -1.0});
    check(p2_identifiability_assess(bad.data(), 2, 100, 1e-10, &v3) == 1,
          "B17 negative diagonal => rc=1 (not a normal matrix)");
}

// ===========================================================================
// C. 尺度不变：换参数单位、换权重绝对尺度，判决与 r_eff 都不变
// ===========================================================================
void test_scale_invariance() {
    std::printf("[scale_invariance]\n");
    // 一个 5×5 对称正定矩阵（固定 seed，确定性构造）
    const int n = 5;
    std::vector<double> H((std::size_t)n * n, 0.0);
    std::uint64_t s = 20260101ULL;
    auto rnd = [&]() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)((s >> 11) % 1000000) / 1000000.0 - 0.5;
    };
    for (int i = 0; i < n; ++i)
        for (int j = i; j < n; ++j) {
            const double v = (i == j) ? (1.0 + std::fabs(rnd())) : 0.3 * rnd();
            H[(std::size_t)i * n + j] = v;
            H[(std::size_t)j * n + i] = v;
        }
    P2Identifiability base{};
    check(p2_identifiability_assess(H.data(), n, 100, 1e-10, &base) == 0, "C1 rc==0");
    // C2 全局标度（换权重绝对尺度 / 换数据单位）：判决、r_eff、κ 全不变
    for (double c : {1e-16, 1e-6, 1.0, 1e6, 1e16}) {
        std::vector<double> Hc = H;
        for (auto& x : Hc) x *= c;
        P2Identifiability v{};
        check(p2_identifiability_assess(Hc.data(), n, 100, 1e-10, &v) == 0, "C2 rc==0");
        check(v.identifiable == base.identifiable && v.rank_eff == base.rank_eff,
              "C3 verdict & rank_eff invariant under global scale x" + std::to_string(c));
        check(near(v.kappa, base.kappa, 1e-9 * std::max(1.0, base.kappa)),
              "C4 kappa invariant under global scale x" + std::to_string(c));
    }
    // C5 **参数单位**重标定（逐列/逐对角缩放）：列均衡后必须不变
    //    （裸 κ 会变——这正是相对秩判据优于裸 κ 的地方）
    std::vector<double> Hd = H;
    const double scale[5] = {1e-3, 1e-1, 1.0, 1e1, 1e3};
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            Hd[(std::size_t)i * n + j] = H[(std::size_t)i * n + j] * scale[i] * scale[j];
    P2Identifiability vd{};
    check(p2_identifiability_assess(Hd.data(), n, 100, 1e-10, &vd) == 0, "C5 rc==0");
    check(vd.identifiable == base.identifiable && vd.rank_eff == base.rank_eff,
          "C6 verdict & rank_eff invariant under parameter-unit rescaling");
    check(near(vd.kappa, base.kappa, 1e-6 * std::max(1.0, base.kappa)),
          "C7 kappa invariant under parameter-unit rescaling (column equilibration)");
    // C8 反证：**未均衡**的裸 κ 在同样的列缩放下会变（说明均衡不是装饰）
    const std::vector<double> Eb = oracle_eig(H, n);
    const std::vector<double> Ed = oracle_eig(Hd, n);
    const double raw_b = Eb.front() / Eb.back();
    const double raw_d = Ed.front() / Ed.back();
    check(std::fabs(raw_b - raw_d) > 1e-3 * std::max(raw_b, raw_d),
          "C8 raw (unequilibrated) kappa DOES change under column scaling: " +
              std::to_string(raw_b) + " vs " + std::to_string(raw_d));
}

// ===========================================================================
// D. 独立 Oracle 对拍：判据的 κ / r_eff 必须与测试侧 Jacobi 特征值一致
// ===========================================================================
void test_oracle_cross_check() {
    std::printf("[oracle_cross_check]\n");
    const int n = 6;
    std::vector<double> H((std::size_t)n * n, 0.0);
    std::uint64_t s = 777001ULL;
    auto rnd = [&]() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)((s >> 11) % 1000000) / 1000000.0 - 0.5;
    };
    for (int i = 0; i < n; ++i)
        for (int j = i; j < n; ++j) {
            const double v = (i == j) ? (1.0 + std::fabs(rnd())) : 0.25 * rnd();
            H[(std::size_t)i * n + j] = v;
            H[(std::size_t)j * n + i] = v;
        }
    // 人为压出一对**几乎共线**的方向：ρ = 1 − 1e-8 ⇒ λ_min/λ_max ≈ 5e-9。
    // 该量级既能让独立 Jacobi Oracle 分辨（其绝对误差 ~eps·‖A‖ ≈ 1e-16），
    // 又能被 τ 两侧分别读成红/绿：τ=1e-6 ⇒ 红（r_eff 少 1），τ=1e-10 ⇒ 绿。
    const double r = 1.0 - 1e-8;
    const double c01 = r * std::sqrt(H[0] * H[(std::size_t)n + 1]);
    H[1] = c01;
    H[(std::size_t)n] = c01;
    // 把这对近共线方向**隔离**成 2×2 块：否则它们与其余方向的耦合会让矩阵
    // 真的变成不定（Schur 补为负），测的就不是"近奇异"而是"不定"了。
    for (int k = 2; k < n; ++k) {
        H[(std::size_t)k] = 0.0;                          // H[k][0]
        H[(std::size_t)k * n] = 0.0;                      // H[0][k]
        H[(std::size_t)k * n + 1] = 0.0;                  // H[k][1]
        H[(std::size_t)1 * n + k] = 0.0;                  // H[1][k]
    }
    P2Identifiability v{};
    check(p2_identifiability_assess(H.data(), n, 1000, 1e-6, &v) == 0, "D1 rc==0");
    const std::vector<double> ev = oracle_eig(equilibrate(H, n), n);
    const double lam_max = ev.front();
    const double lam_min = ev.back();
    const double tau = v.rank_rtol_effective;
    std::uint64_t want_rank = 0;
    for (double e : ev)
        if (e > tau * lam_max) ++want_rank;
    check(near(v.lambda_max, lam_max, 1e-9 * std::max(1.0, lam_max)),
          "D2 lambda_max matches Jacobi oracle: " + std::to_string(v.lambda_max) + " vs " +
              std::to_string(lam_max));
    check(near(v.lambda_min, lam_min, 1e-6 * std::max(1e-12, lam_min)),
          "D3 lambda_min matches Jacobi oracle: " + std::to_string(v.lambda_min) + " vs " +
              std::to_string(lam_min));
    check(v.rank_eff == want_rank,
          "D4 rank_eff matches oracle count: " + std::to_string(v.rank_eff) + " vs " +
              std::to_string(want_rank));
    check(near(v.kappa, lam_max / lam_min, 1e-6 * (lam_max / lam_min)),
          "D5 kappa matches oracle");
    check(v.identifiable == (want_rank == (std::uint64_t)n ? 1 : 0),
          "D6 verdict == (rank_eff == n)");
    check(want_rank == (std::uint64_t)(n - 1) && v.identifiable == 0,
          "D6b tau=1e-6 > gap => exactly one unidentified direction (RED)");
    P2Identifiability vgreen{};
    check(p2_identifiability_assess(H.data(), n, 1000, 1e-10, &vgreen) == 0, "D6c rc==0");
    check(vgreen.identifiable == 1 && vgreen.rank_eff == (std::uint64_t)n,
          "D6d same matrix at tau=1e-10 < gap => GREEN (verdict tracks tau, not the data)");
    // D7 精度地板：τ 请求值小于地板时按地板走（地板由算术给，不是标定常数）
    const double floor_tau = p2_identifiability_rank_rtol_floor(1000, n);
    check(near(floor_tau, 1000.0 * std::numeric_limits<double>::epsilon(), 1e-30),
          "D7 floor(tau) = max(m,n)*eps");
    P2Identifiability vfloor{};
    check(p2_identifiability_assess(H.data(), n, 1000, 0.0, &vfloor) == 0, "D8 rc==0");
    check(vfloor.rank_rtol_effective >= floor_tau,
          "D9 effective tau >= precision floor");
    // D10 有效自由度（Andrae 2010 式 (9)）：dof = n_obs − r_eff，不是 n_obs − n_params
    check(near(p2_identifiability_dof(1000, 30), 970.0, 1e-12), "D10 dof = n_obs - rank_eff");
}

// ===========================================================================
// E. 天光面侧接线：良性输入判绿；构造性病态输入判红（红/绿双向）
// ===========================================================================
std::vector<P2SkySample> benign_samples() {
    std::vector<P2SkySample> v;
    for (std::uint64_t f = 0; f < 3; ++f) {
        for (int iy = 0; iy < 10; ++iy) {
            for (int ix = 0; ix < 10; ++ix) {
                P2SkySample s{};
                s.frame_id = 100 + f;
                s.control_id = static_cast<std::uint64_t>(f * 100 + iy * 10 + ix);
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
        }
    }
    return v;
}

// 构造性病态：帧 1 只观测**参考帧完全没覆盖**的一个格子（2×2 节点）。
// 该格子的 B_ref（双线性：{1, ξ, η, ξη}）只有 ξη 方向不在 δ_1 的平面基
// {1, ξ, η} 里 ⇒ 经 Schur 消元后该格子留下 3 个未被数据约束的方向 ⇒ 判红。
std::vector<P2SkySample> rank_deficient_samples() {
    std::vector<P2SkySample> v;
    // 参考帧：左半区（ix=0..4）
    for (int iy = 0; iy < 8; ++iy) {
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
    }
    // 帧 1：右半区，但**只覆盖一个格子**（ix=8,9 × iy=4,5）
    for (int iy = 4; iy <= 5; ++iy) {
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
    }
    return v;
}

int build_sky(const std::vector<P2SkySample>& s, double h, P2SkyPlaneInfo* out,
              std::string* err_out) {
    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    cfg.node_spacing_deg = h;
    cfg.spline_degree = 1;
    cfg.frame_gradient_order = 1;
    cfg.max_nodes = 4096;
    char err[512] = {0};
    void* m = nullptr;
    const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
    if (err_out) *err_out = err;
    if (rc == P2_SKY_PLANE_OK && m && out) p2_sky_plane_info(m, &out[0]);
    if (m) p2_sky_plane_close(m);
    return rc;
}

void test_sky_plane_wiring() {
    std::printf("[sky_plane_wiring]\n");
    std::string err;
    P2SkyPlaneInfo info{};
    // E1 良性输入：判绿，判据读数齐全，rank_eff 是**计数**（= n_params）
    const std::vector<P2SkySample> good = benign_samples();
    const int rc = build_sky(good, 0.12, &info, &err);
    check(rc == P2_SKY_PLANE_OK, "E1 benign build ok rc=" + std::to_string(rc) + " " + err);
    check(info.identifiable == 1, "E2 benign => identifiable");
    check(info.rank == info.n_params && info.n_unidentified == 0,
          "E3 benign: rank_eff == n_params (count), unidentified == 0");
    check(std::isfinite(info.kappa) && info.kappa > 0.0,
          "E4 benign: kappa(H_red) finite>0 = " + std::to_string(info.kappa));
    check(info.rank_rtol_effective >= 1e-10, "E5 tau_eff reported");
    check(info.lambda_numerical > 0.0, "E6 derived numerical ridge reported");
    check(info.dof_eff > 0.0 &&
              near(info.dof_eff, (double)info.n_used - (double)info.rank_full, 1e-9),
          "E7 dof_eff == n_used - rank_full (Andrae 2010 eq.9: dof = N - rank(X))");
    check(info.rank_full == info.rank + (info.n_frames - 1) * 3,
          "E7b rank_full == r_eff + (n_frames-1)*m (delta block full rank per frame)");
    check(info.n_params == info.rank && info.n_params < info.n_params_full,
          "E7c n_params == criterion matrix order (delta profiled out)");
    // E8 **判决只看 H_red**：κ(H_solve) 是独立诊断量，且 λ_eff 不可能把红买成绿
    check(std::isfinite(info.kappa_solve) && info.kappa_solve > 0.0,
          "E8 kappa_solve (diagnostic only) finite>0 = " + std::to_string(info.kappa_solve));
    std::printf("  INFO benign: kappa=%.6g kappa_solve=%.6g lambda_eff=%.6g "
                "rank=%llu/%llu chi2_red=%.6g\n",
                info.kappa, info.kappa_solve, info.lambda_numerical,
                (unsigned long long)info.rank, (unsigned long long)info.n_params,
                info.chi2_red);

    // E9 病态输入：必须判红（rc = NOT_IDENTIFIABLE），且诊断指出未约束方向数
    const std::vector<P2SkySample> bad = rank_deficient_samples();
    P2SkyPlaneInfo bad_info{};
    const int rcb = build_sky(bad, 0.10, &bad_info, &err);
    check(rcb == P2_SKY_PLANE_NOT_IDENTIFIABLE,
          "E9 rank-deficient input => NOT_IDENTIFIABLE, got rc=" + std::to_string(rcb) +
              " " + err);
    check(err.find("rank_eff") != std::string::npos,
          "E10 error text names rank_eff (not a kappa constant): " + err);
    check(err.find("kappa(H_red)") != std::string::npos, "E11 error text names kappa(H_red)");

    // E12 尺度不变（端到端）：把全部方差（权重绝对尺度）乘 1e12 ⇒ 判决不变
    std::vector<P2SkySample> good2 = good;
    for (auto& s : good2) s.variance *= 1e12;
    P2SkyPlaneInfo info2{};
    const int rc2 = build_sky(good2, 0.12, &info2, &err);
    check(rc2 == P2_SKY_PLANE_OK, "E12 benign with variance x1e12 still ok");
    check(info2.identifiable == 1 && info2.rank == info2.n_params &&
              info2.n_unidentified == 0,
          "E13 verdict invariant under weight-scale x1e12");
    check(near(info2.kappa, info.kappa, 1e-6 * info.kappa),
          "E14 kappa invariant under weight-scale x1e12: " + std::to_string(info2.kappa) +
              " vs " + std::to_string(info.kappa));
    check(near(info2.chi2_red, info.chi2_red, 1e-6 * std::max(1.0, info.chi2_red)),
          "E15 chi2_red invariant under weight-scale x1e12");
}

}  // namespace

int main() {
    test_criterion_positive();
    test_criterion_negative();
    test_scale_invariance();
    test_oracle_cross_check();
    test_sky_plane_wiring();
    std::printf("P2-IDENTIFIABILITY: %d/%d checks passed, %d failed\n",
                g_total - g_fail, g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}

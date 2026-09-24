// lib/algorithms/coverage/src/identifiability.cpp — Phase2 唯一可辨识性判据实现
//
// 语义、依据与「为什么不需要标定常数」见头文件
// lib/algorithms/coverage/include/astro/phase2/identifiability.h。
//
// 数值实现（为什么这样做）：
//   · 列均衡 H_eq = D⁻¹ H D⁻¹（D=diag(sqrt(H_ii))）——消除**参数单位**自由度。
//     裸 κ 在参数列缩放下会变（实测：列缩放 1e-3..1e3 使 κ 由 1.01e17 变
//     5.03e17，rank 不变），均衡后的相对谱在列缩放下不变。
//   · λ_1：幂迭代（确定性起点 1/√n ⇒ 同输入逐位可复现）。
//   · λ_n：逆幂迭代（Cholesky 解）；H_eq 非正定 ⇒ λ_n = 0（不发布伪值）。
//   · r_eff：位移矩阵 (H_eq − τλ_1 I) 的**惯性计数**（Sylvester 惯性定律）：
//         #负主元(LDLᵀ) = #{λ_i < τλ_1}，
//     用 Gill–Murray–Wright 的**修正主元** LDLᵀ（主元 ≤ tol 时计数并置为 +tol）
//     使分解在不定矩阵上不中断。
//   · 判决位用**直接**判据：chol_spd(H_eq − τλ_1 I) 成功 ⟺ λ_n > τλ_1 ⟺ r_eff == n。
//     两个等价读法不一致时取**更保守者**（判红），并在注释中写明——不静默择一。
#include "astro/phase2/identifiability.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <limits>
#include <vector>

namespace {

constexpr double kEps = std::numeric_limits<double>::epsilon();   // 2.220446049250313e-16

// 无 jitter Cholesky：A = L Lᵀ（A 对称 SPD）。返回 false 表示非 SPD。
bool chol_spd(const std::vector<double>& A, int n, std::vector<double>& L) {
    L.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = A[static_cast<std::size_t>(i) * n + j];
            for (int k = 0; k < j; ++k)
                sum -= L[static_cast<std::size_t>(i) * n + k] *
                       L[static_cast<std::size_t>(j) * n + k];
            if (i == j) {
                if (!(sum > 0.0) || !std::isfinite(sum)) return false;
                L[static_cast<std::size_t>(i) * n + i] = std::sqrt(sum);
            } else {
                const double d = L[static_cast<std::size_t>(j) * n + j];
                if (!(d > 0.0)) return false;
                L[static_cast<std::size_t>(i) * n + j] = sum / d;
            }
        }
    }
    return true;
}

void chol_solve(const std::vector<double>& L, int n, std::vector<double>& b) {
    std::vector<double> y(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double sum = b[static_cast<std::size_t>(i)];
        for (int k = 0; k < i; ++k)
            sum -= L[static_cast<std::size_t>(i) * n + k] * y[static_cast<std::size_t>(k)];
        y[static_cast<std::size_t>(i)] = sum / L[static_cast<std::size_t>(i) * n + i];
    }
    for (int i = n - 1; i >= 0; --i) {
        double sum = y[static_cast<std::size_t>(i)];
        for (int k = i + 1; k < n; ++k)
            sum -= L[static_cast<std::size_t>(k) * n + i] * b[static_cast<std::size_t>(k)];
        b[static_cast<std::size_t>(i)] = sum / L[static_cast<std::size_t>(i) * n + i];
    }
}

double sym_rayleigh(const std::vector<double>& A, int n, const std::vector<double>& x) {
    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; ++i) {
        double ax = 0.0;
        const double* row = &A[static_cast<std::size_t>(i) * n];
        for (int j = 0; j < n; ++j) ax += row[j] * x[static_cast<std::size_t>(j)];
        num += x[static_cast<std::size_t>(i)] * ax;
        den += x[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)];
    }
    return (den > 0.0) ? num / den : 0.0;
}

// λ_1（幂迭代）。确定性起点 ⇒ 同输入逐位可复现。
double lambda_max_power(const std::vector<double>& A, int n) {
    std::vector<double> x(static_cast<std::size_t>(n), 1.0 / std::sqrt(static_cast<double>(n)));
    std::vector<double> y(static_cast<std::size_t>(n), 0.0);
    double prev = 0.0;
    for (int it = 0; it < 300; ++it) {
        for (int i = 0; i < n; ++i) {
            double s = 0.0;
            const double* row = &A[static_cast<std::size_t>(i) * n];
            for (int j = 0; j < n; ++j) s += row[j] * x[static_cast<std::size_t>(j)];
            y[static_cast<std::size_t>(i)] = s;
        }
        double norm = 0.0;
        for (int i = 0; i < n; ++i)
            norm += y[static_cast<std::size_t>(i)] * y[static_cast<std::size_t>(i)];
        norm = std::sqrt(norm);
        if (!(norm > 0.0) || !std::isfinite(norm)) break;
        for (int i = 0; i < n; ++i)
            x[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(i)] / norm;
        const double lam = sym_rayleigh(A, n, x);
        if (it > 3 && std::fabs(lam - prev) <= 1e-13 * std::max(1.0, std::fabs(lam))) {
            prev = lam;
            break;
        }
        prev = lam;
    }
    return sym_rayleigh(A, n, x);
}

// λ_n（逆幂迭代，用 Cholesky 解）。非正定 ⇒ 0。
double lambda_min_inverse(const std::vector<double>& A, const std::vector<double>& L, int n) {
    std::vector<double> x(static_cast<std::size_t>(n), 1.0 / std::sqrt(static_cast<double>(n)));
    for (int it = 0; it < 300; ++it) {
        std::vector<double> w = x;
        chol_solve(L, n, w);
        double norm = 0.0;
        for (int i = 0; i < n; ++i)
            norm += w[static_cast<std::size_t>(i)] * w[static_cast<std::size_t>(i)];
        norm = std::sqrt(norm);
        if (!(norm > 0.0) || !std::isfinite(norm)) return 0.0;
        for (int i = 0; i < n; ++i)
            x[static_cast<std::size_t>(i)] = w[static_cast<std::size_t>(i)] / norm;
    }
    return sym_rayleigh(A, n, x);
}

// 惯性计数：返回 #{ λ_i(A) ≤ 0 } 的估计（A 对称，已按 scale 归一）。
// 修正主元 LDLᵀ（Gill–Murray–Wright）：主元 ≤ tol 记一次并置为 +tol，
// 使分解在不定矩阵上继续；精确算术下 #负主元 = #负特征值（Sylvester）。
std::uint64_t count_nonpositive_pivots(std::vector<double> A, int n, double tol) {
    std::uint64_t neg = 0;
    std::vector<double> d(static_cast<std::size_t>(n), 0.0);
    for (int j = 0; j < n; ++j) {
        double dj = A[static_cast<std::size_t>(j) * n + j];
        for (int k = 0; k < j; ++k) {
            const double ljk = A[static_cast<std::size_t>(j) * n + k];
            dj -= ljk * ljk * d[static_cast<std::size_t>(k)];
        }
        if (!(dj > tol)) {
            ++neg;              // 该主元 ≤ tol：计一次「不高于阈值的方向」
            dj = tol;           // 修正主元，保证后续除法有界
        }
        d[static_cast<std::size_t>(j)] = dj;
        for (int i = j + 1; i < n; ++i) {
            double s = A[static_cast<std::size_t>(i) * n + j];
            for (int k = 0; k < j; ++k)
                s -= A[static_cast<std::size_t>(i) * n + k] *
                     A[static_cast<std::size_t>(j) * n + k] * d[static_cast<std::size_t>(k)];
            A[static_cast<std::size_t>(i) * n + j] = s / dj;
        }
    }
    return neg;
}

}  // namespace

extern "C" {

double p2_identifiability_rank_rtol_floor(std::uint64_t n_obs, std::uint64_t n_params) {
    const std::uint64_t mn = std::max<std::uint64_t>(n_obs, n_params);
    if (mn == 0) return kEps;
    return static_cast<double>(mn) * kEps;
}

double p2_identifiability_dof(std::uint64_t n_obs, std::uint64_t rank_eff) {
    return static_cast<double>(n_obs) - static_cast<double>(rank_eff);
}

int p2_identifiability_assess(const double* H, std::uint64_t n_params, std::uint64_t n_obs,
                              double rank_rtol, P2Identifiability* out) {
    if (H == nullptr || out == nullptr || n_params == 0) return 1;
    const int n = static_cast<int>(n_params);

    // ---- 阈值：请求值与环境精度地板取大者（地板由算术推出，不是标定常数）----
    const double floor_tau = p2_identifiability_rank_rtol_floor(n_obs, n_params);
    double tau = (std::isfinite(rank_rtol) && rank_rtol > 0.0) ? rank_rtol : floor_tau;
    if (tau < floor_tau) tau = floor_tau;

    // ---- 对角筛查：区分「无信息方向」与「非法输入」----
    // 法方程 H = JᵀWJ 的对角必为 Σ w j² ≥ 0：
    //   H_ii == 0 / 非有限  ⇒ 第 i 个参数**完全没有信息**（dead）：它本身就是
    //     一个未被约束的方向，直接计入 n_unidentified，不参与均衡；
    //   H_ii < 0            ⇒ 输入不是法方程（调用方给错了矩阵）⇒ rc=1，
    //     不猜、不用 |H_ii| 蒙过去。
    std::vector<int> live;
    live.reserve(static_cast<std::size_t>(n));
    std::uint64_t n_dead = 0;
    for (int i = 0; i < n; ++i) {
        const double hii = H[static_cast<std::size_t>(i) * n + i];
        if (std::isfinite(hii) && hii > 0.0) {
            live.push_back(i);
        } else if (std::isfinite(hii) && hii == 0.0) {
            ++n_dead;
        } else {
            return 1;   // 负对角 / NaN / inf：不是合法法方程
        }
    }

    out->n_params = n_params;
    out->n_obs = n_obs;
    out->rank_rtol = rank_rtol;
    out->rank_rtol_effective = tau;

    if (live.empty()) {   // 全部方向无信息
        out->lambda_max = 0.0;
        out->lambda_min = 0.0;
        out->kappa = std::numeric_limits<double>::infinity();
        out->rank_eff = 0;
        out->n_unidentified = n_params;
        out->identifiable = 0;
        out->equilibrate_min = 0.0;
        out->equilibrate_max = 0.0;
        return 0;
    }

    // ---- 列均衡 D⁻¹ H D⁻¹（消除参数单位自由度；只在 live 子块上做）----
    const int nl = static_cast<int>(live.size());
    const std::size_t nlnl = static_cast<std::size_t>(nl) * static_cast<std::size_t>(nl);
    std::vector<double> D(static_cast<std::size_t>(nl), 0.0);
    double dmin = std::numeric_limits<double>::infinity();
    double dmax = 0.0;
    for (int i = 0; i < nl; ++i) {
        const double hii = H[static_cast<std::size_t>(live[static_cast<std::size_t>(i)]) * n +
                             live[static_cast<std::size_t>(i)]];
        D[static_cast<std::size_t>(i)] = std::sqrt(hii);
        dmin = std::min(dmin, D[static_cast<std::size_t>(i)]);
        dmax = std::max(dmax, D[static_cast<std::size_t>(i)]);
    }
    std::vector<double> Heq(nlnl, 0.0);
    for (int i = 0; i < nl; ++i) {
        const double di = D[static_cast<std::size_t>(i)];
        for (int j = 0; j < nl; ++j) {
            const double dj = D[static_cast<std::size_t>(j)];
            // 对称化：输入可能只有一侧被填（数值噪声）⇒ 取平均保证严格对称
            const double a = H[static_cast<std::size_t>(live[static_cast<std::size_t>(i)]) * n +
                               live[static_cast<std::size_t>(j)]];
            const double b = H[static_cast<std::size_t>(live[static_cast<std::size_t>(j)]) * n +
                               live[static_cast<std::size_t>(i)]];
            Heq[static_cast<std::size_t>(i) * nl + j] = 0.5 * (a + b) / (di * dj);
        }
    }
    out->equilibrate_min = dmin;
    out->equilibrate_max = dmax;

    // ---- λ_1 / λ_n（live 子块）----
    const double lam_max = lambda_max_power(Heq, nl);
    out->lambda_max = lam_max;
    if (!(lam_max > 0.0) || !std::isfinite(lam_max)) {
        out->lambda_min = 0.0;
        out->kappa = std::numeric_limits<double>::infinity();
        out->rank_eff = 0;
        out->n_unidentified = n_params;
        out->identifiable = 0;
        return 0;
    }
    std::vector<double> L;
    double lam_min = 0.0;
    if (chol_spd(Heq, nl, L)) lam_min = lambda_min_inverse(Heq, L, nl);
    if (!(lam_min > 0.0) || !std::isfinite(lam_min)) lam_min = 0.0;
    out->lambda_min = lam_min;
    // 秩亏时 κ 是舍入噪声的比值（伪值），按 +inf 记账而不是发布一个数。
    // 存在 dead 方向时整个系统必然未约束 ⇒ κ 同样记 +inf（不发布局部好看的值）。
    out->kappa = (lam_min > 0.0 && n_dead == 0)
                     ? (lam_max / lam_min)
                     : std::numeric_limits<double>::infinity();

    // ---- 有效秩：位移矩阵 (H_eq − τλ_1 I) 的惯性 ----
    const double sigma = tau * lam_max;
    std::vector<double> shifted = Heq;
    for (int i = 0; i < nl; ++i) shifted[static_cast<std::size_t>(i) * nl + i] -= sigma;

    // 直接判据：chol_spd(H_eq − τλ_1 I) ⟺ λ_n > τλ_1 ⟺ r_eff == n_live。
    std::vector<double> Ls;
    const bool spd_shifted = chol_spd(shifted, nl, Ls);
    // 计数判据（同一件事的另一种读数，供发布 n_unidentified）。
    const double scale = std::max(1.0, std::fabs(1.0 - sigma));
    std::uint64_t below = count_nonpositive_pivots(shifted, nl, 64.0 * kEps * scale);
    if (below > static_cast<std::uint64_t>(nl)) below = static_cast<std::uint64_t>(nl);
    // 两个等价读法不一致时取更保守者（判红），不静默择一。
    if (spd_shifted && below != 0) below = 0;
    if (!spd_shifted && below == 0) below = 1;

    out->rank_eff = static_cast<std::uint64_t>(nl) - below;
    out->n_unidentified = n_dead + below;
    out->identifiable = (out->n_unidentified == 0) ? 1 : 0;
    return 0;
}

}  // extern "C"

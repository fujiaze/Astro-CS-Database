/**
 * @file nls_lm.cpp
 * @brief 自研信赖域 Levenberg–Marquardt 求解器实现（GSL gsl_multifit_nlinear 替代）
 *
 * ============================ 算法版本与逐条依据 ============================
 *
 * 本实现是**信赖域型 Levenberg–Marquardt**，按下列公开算法的原文实现（不复制任何
 * GPL 代码；GSL 只作为行为对齐的**实测对象**，见 run/GSL-REPLACE-01/REPORT.md §3）：
 *
 * 1) 子问题解法：**直接步（damped normal equations）**
 *      (JᵀJ + λ·D²) h = −Jᵀf,   D = diag(‖J 的第 i 列‖₂),  Cholesky 求解。
 *    依据 Moré 1978 §3（"The Levenberg-Marquardt algorithm"）与
 *    Madsen–Nielsen–Tingleff 2004（MM-04）§3.2 "The L-M method"。
 *    与 GSL gsl_multifit_nlinear_trs_lm + solver=qr 的公开语义同类：GSL 手册的 trs_lm
 *    就是阻尼正规方程；缩放默认 scale=more 即 D=diag(‖J_i‖)（由
 *    gsl_multifit_nlinear_default_parameters() 实测得到, 报告 §3.1）。
 *    **不采用** dogleg / 二维子空间：本仓问题 p=7、残差 n≈49–441，直接步与 GSL 行为
 *    最接近且无需额外 QR/Givens 存储（取舍见报告 §3.4）。J 的列可能恒为 0
 *    （各向同性星的朝向角列），故 λ=0 的 Cholesky 允许失败并按 λ 正则化回退。
 *
 * 2) 信赖域半径 Δ 更新（Nielsen 1999；GSL 的 factor_up=3 / factor_down=2 / avmax=0.75
 *    即该版常数，实测自 gsl_multifit_nlinear_default_parameters()）：
 *      ρ ≥ avmax(0.75)  ⇒ Δ ← max(Δ, factor_up·‖D·h‖)      （好步，放大）
 *      0.25 ≤ ρ < avmax ⇒ Δ 不变
 *      ρ < 0.25         ⇒ Δ ← Δ/factor_down                （差步，缩小）
 *      步被拒绝         ⇒ Δ ← Δ/factor_down, λ ← λ·ν, ν ← 2ν（内层重解）
 *    ρ = (χ²(x) − χ²(x+h)) / (χ²(x) − ‖f+Jh‖²)（实测下降 / 模型预测下降，MM-04 §3.2）。
 *
 * 3) λ 与 Δ 的换算：解 ‖D·h(λ)‖ = Δ 的一维求根。h(λ) 由 (JᵀJ+λD²)h = −Jᵀf 定义，
 *    ‖D·h(λ)‖ 关于 λ 单调下降；按 MINPACK lmpar 的技巧对
 *    ψ(λ) = 1/‖D·h(λ)‖ − 1/Δ 迭代（ψ 关于 λ 近似线性），**以二分法保护**。
 *    λ=0 ⇒ Gauss–Newton 步；首次迭代 Δ=+∞（λ=0，无约束），接受后令 Δ = ‖D·h‖
 *    （MINPACK lmdif 的 "on the first iteration, adjust the initial step bound"）。
 *    λ 的更新（步接受时，Nielsen 阻尼更新）: λ ← λ·max(1/3, 1−(2ρ−1)³)。
 *    上界搜索起点: λ = max(μ_hint, 1e-3·max_i(A_ii))（Marquardt 1963 / Nielsen 的经典
 *    初值 τ·max(diag(JᵀJ)), τ=1e-3）。
 *
 * 4) 停止判据（三判据，与 GSL driver 的 xtol/gtol/ftol **同序同编码**）：
 *    - **xtol**（info=1）: 逐分量 |Δx_i| ≤ xtol_abs + xtol·|x_i|。xtol_abs=1e-6 由 GSL
 *      行为标定：该式可复现 GSL 在 8 个探针问题上的**全部**终止点，而纯相对式
 *      (0 + 1e-3·|x_i|) 在 flat_ell 上、绝对式 (1e-3 + |x_i|) 在 snr30_ell 上都不能
 *      复现（报告 §3.1 表）。
 *    - **ftol**（info=2）: MINPACK 的代价相对变化判据
 *      |Δχ²| ≤ ftol·χ²_old ∧ 预测下降 ≤ ftol·χ²_old ∧ ρ ≤ 2。
 *    - **gtol**（info=3）: ‖Jᵀf‖_∞ ≤ gtol（绝对量纲；本仓残差为 ADU 量级 ⇒ 实际近乎
 *      不触发，与 GSL 实测一致）。
 *
 * 5) 失败语义（fail-closed，不返回未收敛结果）: 迭代上限用尽 ⇒ Status::MaxIterations
 *    （对应 GSL GSL_EMAXITER，生产侧 sdet_api.cpp 映射 SDET_FIT_NO_CONVERGENCE，
 *    与替换前一致）；λ 放大到 1e300 仍无法把步压进信赖域 / 试探点非有限 ⇒
 *    Status::NumericalFailure（同样映射为不收敛）。
 * ==========================================================================
 */
#include "nls_lm.h"

#include <cmath>
#include <cstring>

namespace astrocs {
namespace star_detection {
namespace nls {

namespace {

constexpr double kAcceptRho = 1e-4;         // 接受门（MINPACK p0001）
constexpr double kGoodRho = 0.75;           // 「好步」门（GSL avmax 默认值）
constexpr double kPoorRho = 0.25;           // 「差步」门（Nielsen）
constexpr std::size_t kMaxInnerRetry = 60;  // 单次迭代内 Δ/λ 重试上限
constexpr std::size_t kMaxLambdaIter = 50;  // ‖D·h(λ)‖=Δ 求根迭代上限
constexpr double kLambdaSeedTau = 1e-3;     // 上界搜索起点 τ·max(diag(JᵀJ))

inline double sumsq(const double* v, std::size_t n) {
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += v[i] * v[i];
    return s;
}

/** Cholesky 下三角原地分解（p×p 行主序）；非正定/非有限返回 false。*/
bool cholesky_factor(double* M, std::size_t p) {
    for (std::size_t i = 0; i < p; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double s = M[i * p + j];
            for (std::size_t k = 0; k < j; ++k) s -= M[i * p + k] * M[j * p + k];
            if (i == j) {
                if (!(s > 0.0) || !std::isfinite(s)) return false;
                M[i * p + i] = std::sqrt(s);
            } else {
                M[i * p + j] = s / M[j * p + j];
            }
        }
    }
    return true;
}

/** 解 L·Lᵀ·x = b（L 为下三角, p×p 行主序; wa 为长度 p 的临时）。*/
void cholesky_solve(const double* L, std::size_t p, const double* b, double* x, double* wa) {
    for (std::size_t i = 0; i < p; ++i) {
        double s = b[i];
        for (std::size_t k = 0; k < i; ++k) s -= L[i * p + k] * wa[k];
        wa[i] = s / L[i * p + i];
    }
    for (std::size_t ii = p; ii-- > 0;) {
        double s = wa[ii];
        for (std::size_t k = ii + 1; k < p; ++k) s -= L[k * p + ii] * x[k];
        x[ii] = s / L[ii * p + ii];
    }
}

/** ‖D·h‖₂。*/
double scaled_norm(const double* D, const double* h, std::size_t p) {
    double s = 0.0;
    for (std::size_t i = 0; i < p; ++i) {
        const double t = D[i] * h[i];
        s += t * t;
    }
    return std::sqrt(s);
}

/** 解 (A + λD²)·h = −g（M 为 p×p 工作矩阵, rhs/wa 为长度 p 临时）；成功返回 true。*/
bool solve_damped(const double* A, const double* D, const double* g, std::size_t p,
                  double lambda, double* h, double* M, double* rhs, double* wa) {
    for (std::size_t i = 0; i < p; ++i) {
        for (std::size_t j = 0; j < p; ++j) M[i * p + j] = A[i * p + j];
        M[i * p + i] += lambda * D[i] * D[i];
        rhs[i] = -g[i];
    }
    if (!cholesky_factor(M, p)) return false;
    cholesky_solve(M, p, rhs, h, wa);
    for (std::size_t i = 0; i < p; ++i) {
        if (!std::isfinite(h[i])) return false;
    }
    return true;
}

/**
 * 求信赖域子问题的直接步：在 ‖D·h‖ ≤ Δ 约束下解 (A+λD²)h = −g。
 * Δ ≤ 0 表示无约束（首次迭代）——此时若 λ=0 的 Gauss–Newton 步不可解
 * （J 的某列恒为 0 ⇒ A 奇异），改用 λ>0 的正则化步。
 * 返回 false 仅当 λ 放大到 1e300 仍不可解（调用方据此 fail-closed）。
 */
bool compute_step(const double* A, const double* D, const double* g, std::size_t p,
                  double delta, double mu_hint, double* h, double* M, double* rhs, double* wa,
                  double* out_lambda, double* out_dhnorm) {
    double max_diag = 0.0;
    for (std::size_t i = 0; i < p; ++i) max_diag = std::max(max_diag, A[i * p + i]);
    if (!(max_diag > 0.0) || !std::isfinite(max_diag)) max_diag = 1.0;

    // --- 阶段 1: 最小可行 λ。λ=0 即 Gauss–Newton 步；若 A 奇异（J 的某列恒为 0,
    //     例如各向同性星的朝向角列 ⇒ A 半正定）则取**最小可行 ridge**，
    //     相当于 QR 分解的秩亏处理（把不可辨识方向的分量置 0）---
    double lam0 = 0.0;
    bool ok0 = solve_damped(A, D, g, p, 0.0, h, M, rhs, wa);
    if (!ok0) {
        for (int k = -14; k <= 0 && !ok0; ++k) {
            lam0 = max_diag * std::pow(10.0, static_cast<double>(k));
            ok0 = solve_damped(A, D, g, p, lam0, h, M, rhs, wa);
        }
    }
    if (ok0) {
        const double dn = scaled_norm(D, h, p);
        if (delta <= 0.0 || dn <= delta) {
            *out_lambda = lam0;
            *out_dhnorm = dn;
            return true;
        }
    }

    // --- 阶段 2: 需要 λ > 0 且把步压进 Δ: 先搜上界 hi ---
    double lo = lam0, psi_lo = 0.0, hi = 0.0, psi_hi = 0.0, nh = 0.0;
    {
        double seed = mu_hint;
        const double seed0 = kLambdaSeedTau * max_diag;
        if (!(seed > seed0)) seed = seed0;
        if (!(seed > 0.0) || !std::isfinite(seed)) seed = 1e-12;
        hi = seed;
        bool ok_hi = false;
        for (std::size_t b = 0; b < 80; ++b) {
            if (solve_damped(A, D, g, p, hi, h, M, rhs, wa)) {
                nh = scaled_norm(D, h, p);
                if (delta <= 0.0 || nh <= delta) {
                    ok_hi = true;
                    break;
                }
                psi_hi = 1.0 / std::max(nh, 1e-300) - 1.0 / std::max(delta, 1e-300);
            }
            lo = hi;
            psi_lo = psi_hi;
            hi *= 10.0;
            if (!std::isfinite(hi) || hi > 1e300) break;
        }
        if (!ok_hi) return false;  // fail-closed
        if (delta <= 0.0) {        // 正则化步（无约束首次迭代的退化回退）
            *out_lambda = hi;
            *out_dhnorm = nh;
            return true;
        }
    }

    // --- ψ(λ)=1/‖D·h(λ)‖ − 1/Δ 的二分保护割线迭代（MINPACK lmpar 技巧）---
    double lam = hi;
    for (std::size_t it = 0; it < kMaxLambdaIter; ++it) {
        if (!solve_damped(A, D, g, p, lam, h, M, rhs, wa)) {
            lo = lam;
            lam = 0.5 * (lam + hi);
            continue;
        }
        nh = scaled_norm(D, h, p);
        if (std::fabs(nh - delta) <= 1e-3 * delta) break;
        const double psi = 1.0 / std::max(nh, 1e-300) - 1.0 / delta;
        if (nh > delta) {
            lo = lam;
            psi_lo = psi;
        } else {
            hi = lam;
            psi_hi = psi;
        }
        double next = 0.5 * (lo + hi);
        if (psi_hi != psi_lo) {
            const double cand = lam - psi * (hi - lo) / (psi_hi - psi_lo);
            if (cand > lo && cand < hi && std::isfinite(cand)) next = cand;
        }
        lam = next;
    }
    // 用最终 λ 再解一次，保证 h 与返回的 λ 一致
    if (!solve_damped(A, D, g, p, lam, h, M, rhs, wa)) return false;
    *out_lambda = lam;
    *out_dhnorm = scaled_norm(D, h, p);
    return true;
}

}  // namespace

void Workspace::ensure(std::size_t n_, std::size_t p_) {
    if (n == n_ && p == p_) return;
    n = n_;
    p = p_;
    f.assign(n, 0.0);
    ftrial.assign(n, 0.0);
    J.assign(n * p, 0.0);
    A.assign(p * p, 0.0);
    g.assign(p, 0.0);
    D.assign(p, 0.0);
    h.assign(p, 0.0);
    xtrial.assign(p, 0.0);
    M.assign(p * p, 0.0);
    wa1.assign(p, 0.0);
    wa2.assign(p, 0.0);
}

Report solve(ResidualFn residual, JacobianFn jacobian, void* ctx,
             std::size_t n, std::size_t p, double* x,
             const Options& opts, Workspace* ws) {
    Report rep;
    if (residual == nullptr || jacobian == nullptr || x == nullptr ||
        n == 0 || p == 0 || opts.max_iter == 0) {
        rep.status = Status::InvalidArgument;
        return rep;
    }
    for (std::size_t i = 0; i < p; ++i) {
        if (!std::isfinite(x[i])) {
            rep.status = Status::InvalidArgument;
            return rep;
        }
    }

    Workspace local;
    if (ws == nullptr) ws = &local;
    ws->ensure(n, p);

    double* f = ws->f.data();
    double* ftrial = ws->ftrial.data();
    double* J = ws->J.data();
    double* A = ws->A.data();
    double* g = ws->g.data();
    double* D = ws->D.data();
    double* h = ws->h.data();
    double* xtrial = ws->xtrial.data();
    double* M = ws->M.data();
    double* wa1 = ws->wa1.data();
    double* wa2 = ws->wa2.data();

    residual(x, ctx, f);
    rep.nfev++;
    jacobian(x, ctx, J);
    rep.njev++;

    double cost = sumsq(f, n);
    if (!std::isfinite(cost)) {
        rep.status = Status::NumericalFailure;
        return rep;
    }

    double delta = 0.0;  // ≤0 ⇒ 首次迭代不设信赖域上界（Gauss–Newton 试探步）
    double mu = 0.0;
    double nu = 2.0;
    Criterion criterion = Criterion::None;
    std::size_t accepted_iters = 0;
    bool converged = false;

    for (std::size_t iter = 1; iter <= opts.max_iter; ++iter) {
        // ---- 缩放矩阵 D_i = ‖J 的第 i 列‖₂（Moré 缩放; GSL scale=more）----
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k) {
                const double v = J[k * p + j];
                s += v * v;
            }
            D[j] = std::sqrt(s);
            if (!(D[j] > 0.0) || !std::isfinite(D[j])) D[j] = 1.0;  // 零列: 退化为绝对缩放
        }

        // ---- 正规方程 A = JᵀJ, g = Jᵀf ----
        for (std::size_t i = 0; i < p; ++i) {
            for (std::size_t j = 0; j <= i; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < n; ++k) s += J[k * p + i] * J[k * p + j];
                A[i * p + j] = s;
                A[j * p + i] = s;
            }
            double sg = 0.0;
            for (std::size_t k = 0; k < n; ++k) sg += J[k * p + i] * f[k];
            g[i] = sg;
        }

        // ---- 内层：求可接受步（Δ 缩小 / λ 放大重试）----
        double rho = -1.0, actred = 0.0, prered = 0.0, trial_cost = cost;
        double dhnorm = 0.0, lambda = 0.0;
        bool accepted = false, solved = false;

        for (std::size_t retry = 0; retry < kMaxInnerRetry; ++retry) {
            if (!compute_step(A, D, g, p, delta, mu, h, M, wa1, wa2, &lambda, &dhnorm)) {
                break;  // solved 仍为 false ⇒ 外层 fail-closed
            }
            solved = true;
            if (iter == 1 && retry == 0) delta = dhnorm;  // 首次步长即初始信赖域半径

            for (std::size_t i = 0; i < p; ++i) xtrial[i] = x[i] + h[i];
            bool finite_trial = true;
            for (std::size_t i = 0; i < p; ++i) {
                if (!std::isfinite(xtrial[i])) finite_trial = false;
            }
            if (!finite_trial) break;
            residual(xtrial, ctx, ftrial);
            rep.nfev++;
            trial_cost = sumsq(ftrial, n);

            // ---- 增益比 ρ = 实测下降 / 模型预测下降 ----
            actred = cost - trial_cost;
            double hgh = 0.0, hAh = 0.0;
            for (std::size_t i = 0; i < p; ++i) {
                hgh += h[i] * g[i];
                double s = 0.0;
                for (std::size_t j = 0; j < p; ++j) s += A[i * p + j] * h[j];
                hAh += h[i] * s;
            }
            prered = -2.0 * hgh - hAh;  // χ²(x) − ‖f + J·h‖²
            rho = (prered > 0.0 && std::isfinite(prered)) ? (actred / prered) : -1.0;

            if (std::isfinite(trial_cost) && rho > kAcceptRho) {
                accepted = true;
                break;
            }

            // ---- 拒绝：缩小信赖域 + 放大阻尼（ν 倍增）----
            if (delta > 0.0) {
                delta = std::max(delta / opts.factor_down, 1e-300);
            } else {
                delta = std::max(dhnorm / opts.factor_down, 1e-12);
            }
            mu = (mu > 0.0) ? mu * nu : std::max(lambda, 1e-12) * nu;
            nu = std::min(nu * 2.0, 1e12);
        }

        if (!solved || !accepted) {
            rep.status = Status::NumericalFailure;
            rep.iterations = accepted_iters;
            rep.cost = cost;
            rep.rms = std::sqrt(cost / static_cast<double>(n));
            return rep;
        }

        // ---- 接受步 ----
        std::memcpy(f, ftrial, n * sizeof(double));
        for (std::size_t i = 0; i < p; ++i) x[i] = xtrial[i];
        cost = trial_cost;
        accepted_iters++;
        jacobian(x, ctx, J);
        rep.njev++;

        // ---- Δ / λ 更新（Nielsen 1999; GSL trs_lm 同族常数）----
        if (rho >= kGoodRho) {
            delta = std::max(delta, opts.factor_up * dhnorm);
        } else if (rho < kPoorRho) {
            delta = std::max(delta / opts.factor_down, 1e-300);
        }
        {
            const double t = 2.0 * rho - 1.0;
            double factor = 1.0 - t * t * t;
            if (factor < 1.0 / 3.0) factor = 1.0 / 3.0;
            mu = lambda * factor;
            nu = 2.0;
        }

        if (opts.trace) opts.trace(accepted_iters, x, h, cost, opts.trace_ctx);

        // ---- 三停止判据（顺序与 GSL driver 一致: xtol → ftol → gtol）----
        {
            bool xtol_ok = true;
            for (std::size_t i = 0; i < p; ++i) {
                if (std::fabs(h[i]) > opts.xtol_abs + opts.xtol * std::fabs(x[i])) {
                    xtol_ok = false;
                    break;
                }
            }
            if (xtol_ok) {
                criterion = Criterion::Xtol;
                converged = true;
                break;
            }
        }
        {
            const double cost_old = cost - actred;  // 接受前的 χ²
            const double scale = (cost_old > 0.0) ? cost_old : 1.0;
            if (actred > 0.0 && std::fabs(actred) <= opts.ftol * scale &&
                prered <= opts.ftol * scale && 0.5 * rho <= 1.0) {
                criterion = Criterion::Ftol;
                converged = true;
                break;
            }
        }
        {
            double ginf = 0.0;
            for (std::size_t i = 0; i < p; ++i) ginf = std::max(ginf, std::fabs(g[i]));
            if (ginf <= opts.gtol) {
                criterion = Criterion::Gtol;
                converged = true;
                break;
            }
        }
    }

    rep.iterations = accepted_iters;
    rep.criterion = criterion;
    rep.cost = cost;
    rep.rms = std::sqrt(cost / static_cast<double>(n));
    rep.mu = mu;
    rep.delta = delta;
    rep.status = converged ? Status::Success : Status::MaxIterations;
    return rep;
}

}  // namespace nls
}  // namespace star_detection
}  // namespace astrocs

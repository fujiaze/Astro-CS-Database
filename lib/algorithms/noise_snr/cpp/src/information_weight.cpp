/* information_weight.cpp - W_info / Q / F_hat 实现 (IMPL-P1-PSFW-001)
 * 合同锚见 information_weight.h。纯 std + libm; 无 session 接线。
 * 数值算法: 对角闭式 / Cholesky 解 / Woodbury 低秩; 均为冻结 §3.4 列出的路径。 */
#include "astrocs/v6/information_weight.h"

#include <cmath>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p1psfw {

namespace {

bool all_finite(const double* x, std::size_t n) {
    if (x == nullptr) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isfinite(x[i])) return false;
    return true;
}

PointEstimate reject(const char* r) {
    PointEstimate e;
    e.ok = false;
    e.reject = r;
    return e;
}

/* Cholesky C = L L^T (行主序, 只写左下)。返回 false 若非正定。 */
bool cholesky(const double* c, std::size_t m, std::vector<double>& l) {
    l.assign(m * m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double s = c[i * m + j];
            for (std::size_t k = 0; k < j; ++k) s -= l[i * m + k] * l[j * m + k];
            if (i == j) {
                if (!(s > 0.0) || !std::isfinite(s)) return false;
                l[i * m + j] = std::sqrt(s);
            } else {
                l[i * m + j] = s / l[j * m + j];
            }
        }
    }
    return true;
}

/* 解 L L^T x = b。 */
void chol_solve(const std::vector<double>& l, std::size_t m, const double* b,
                std::vector<double>& x) {
    std::vector<double> y(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        double s = b[i];
        for (std::size_t k = 0; k < i; ++k) s -= l[i * m + k] * y[k];
        y[i] = s / l[i * m + i];
    }
    x.assign(m, 0.0);
    for (std::size_t ii = m; ii-- > 0;) {
        double s = y[ii];
        for (std::size_t k = ii + 1; k < m; ++k) s -= l[k * m + ii] * x[k];
        x[ii] = s / l[ii * m + ii];
    }
}

/* 解 M z = v (M r*r 行主序 SPD, 直接高斯消元带部分主元; r 小)。 */
bool solve_general(std::vector<double> a, std::vector<double> b, std::size_t r,
                   std::vector<double>& z) {
    z.assign(r, 0.0);
    for (std::size_t col = 0; col < r; ++col) {
        std::size_t piv = col;
        double best = std::fabs(a[col * r + col]);
        for (std::size_t i = col + 1; i < r; ++i) {
            const double v = std::fabs(a[i * r + col]);
            if (v > best) { best = v; piv = i; }
        }
        if (!(best > 0.0) || !std::isfinite(best)) return false;
        if (piv != col) {
            for (std::size_t j = 0; j < r; ++j) std::swap(a[col * r + j], a[piv * r + j]);
            std::swap(b[col], b[piv]);
        }
        for (std::size_t i = col + 1; i < r; ++i) {
            const double f = a[i * r + col] / a[col * r + col];
            if (f == 0.0) continue;
            for (std::size_t j = col; j < r; ++j) a[i * r + j] -= f * a[col * r + j];
            b[i] -= f * b[col];
        }
    }
    for (std::size_t ii = r; ii-- > 0;) {
        double s = b[ii];
        for (std::size_t j = ii + 1; j < r; ++j) s -= a[ii * r + j] * z[j];
        z[ii] = s / a[ii * r + ii];
    }
    return true;
}

PointEstimate finish_from_x(const double* p, const double* d, const double* x,
                            std::size_t m, double a) {
    double ptx = 0.0, dtx = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        ptx += p[i] * x[i];
        dtx += d[i] * x[i];
    }
    PointEstimate e;
    e.w_info = a * a * ptx;
    e.q = a * dtx;
    if (!std::isfinite(e.w_info) || !std::isfinite(e.q) || !(e.w_info > 0.0)) {
        e.ok = false;
        e.reject = "non_positive_or_non_finite_w";
        return e;
    }
    e.ok = true;
    return e;
}

}  /* namespace */

PointEstimate w_info_diagonal(const double* p, std::size_t m,
                              const double* sigma2, const double* d, double a) {
    if (p == nullptr || sigma2 == nullptr || d == nullptr || m == 0) return reject("null_input");
    if (!all_finite(p, m) || !all_finite(sigma2, m) || !all_finite(d, m))
        return reject("non_finite");
    for (std::size_t i = 0; i < m; ++i)
        if (!(sigma2[i] > 0.0)) return reject("non_positive_sigma2");
    std::vector<double> x(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) x[i] = p[i] / sigma2[i];
    return finish_from_x(p, d, x.data(), m, a);
}

PointEstimate w_info_dense(const double* p, std::size_t m,
                           const double* c, const double* d, double a) {
    if (p == nullptr || c == nullptr || d == nullptr || m == 0) return reject("null_input");
    if (!all_finite(p, m) || !all_finite(d, m) || !all_finite(c, m * m))
        return reject("non_finite");
    std::vector<double> l;
    if (!cholesky(c, m, l)) return reject("non_spd");
    std::vector<double> x;
    chol_solve(l, m, p, x);
    return finish_from_x(p, d, x.data(), m, a);
}

PointEstimate w_info_low_rank(const double* p, std::size_t m,
                              const double* d_diag, const double* l, std::size_t r,
                              const double* d, double a) {
    if (p == nullptr || d_diag == nullptr || d == nullptr || m == 0)
        return reject("null_input");
    if (r > 0 && l == nullptr) return reject("null_input");
    if (!all_finite(p, m) || !all_finite(d_diag, m) || !all_finite(d, m))
        return reject("non_finite");
    if (r > 0 && !all_finite(l, m * r)) return reject("non_finite");
    for (std::size_t i = 0; i < m; ++i)
        if (!(d_diag[i] > 0.0)) return reject("non_positive_diagonal");
    /* u = D^-1 p */
    std::vector<double> u(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) u[i] = p[i] / d_diag[i];
    if (r == 0) return finish_from_x(p, d, u.data(), m, a);
    /* M = I + L^T D^-1 L (r*r), v = L^T u */
    std::vector<double> M(r * r, 0.0), v(r, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        const double dinv = 1.0 / d_diag[i];
        for (std::size_t alpha = 0; alpha < r; ++alpha) {
            v[alpha] += l[i * r + alpha] * u[i];
            for (std::size_t beta = 0; beta < r; ++beta)
                M[alpha * r + beta] += l[i * r + alpha] * dinv * l[i * r + beta];
        }
    }
    for (std::size_t alpha = 0; alpha < r; ++alpha) M[alpha * r + alpha] += 1.0;
    std::vector<double> z;
    if (!solve_general(M, v, r, z)) return reject("singular");
    std::vector<double> x(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        double corr = 0.0;
        for (std::size_t alpha = 0; alpha < r; ++alpha) corr += l[i * r + alpha] * z[alpha];
        x[i] = u[i] - corr / d_diag[i];
    }
    return finish_from_x(p, d, x.data(), m, a);
}

PointEstimate w_info_solve(const CovarianceView& cov,
                           const double* p, const double* d, double a) {
    switch (cov.kind) {
        case CovarianceView::Kind::diagonal:
            return w_info_diagonal(p, cov.m, cov.sigma2, d, a);
        case CovarianceView::Kind::dense_spd:
            return w_info_dense(p, cov.m, cov.c, d, a);
        case CovarianceView::Kind::low_rank:
            return w_info_low_rank(p, cov.m, cov.d_diag, cov.l, cov.r, d, a);
    }
    return reject("unknown_covariance_kind");
}

WhiteNoiseGate white_noise_gate(const CovarianceView& cov) {
    WhiteNoiseGate g;
    g.is_diagonal = (cov.kind == CovarianceView::Kind::diagonal);
    g.sigma_declared = cov.sigma_declared;
    if (!g.is_diagonal) g.reason = "non_diagonal_covariance";
    else if (!g.sigma_declared) g.reason = "sigma_pix_undeclared";
    return g;
}

WhiteNoiseResult w_info_white_noise(const double* p, std::size_t m, double sigma_pix2,
                                    double a, const WhiteNoiseGate& gate) {
    WhiteNoiseResult out;
    if (!gate.allowed()) {
        out.reject = "white_noise_condition_not_met";
        return out;
    }
    if (p == nullptr || m == 0) {
        out.reject = "null_input";
        return out;
    }
    if (!(sigma_pix2 > 0.0) || !std::isfinite(sigma_pix2)) {
        out.reject = "non_positive_sigma2";
        return out;
    }
    if (!all_finite(p, m)) {
        out.reject = "non_finite";
        return out;
    }
    double sum = 0.0, sum2 = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        if (p[i] < 0.0) {
            out.reject = "negative_sample";
            return out;
        }
        sum += p[i];
        sum2 += p[i] * p[i];
    }
    if (sum2 <= 0.0 || std::fabs(sum - 1.0) > kQwRelTol) {
        out.reject = "psf_not_normalized";
        return out;
    }
    out.a_nea = 1.0 / sum2;
    out.w_info = a * a / (sigma_pix2 * out.a_nea);
    if (!(out.w_info > 0.0) || !std::isfinite(out.w_info)) {
        out.reject = "non_finite_w";
        return out;
    }
    out.ok = true;
    return out;
}

DiagApproxReport diag_approx_report(const CovarianceView& true_c,
                                    const double* c_tilde,
                                    const double* p, const double* d, double a) {
    DiagApproxReport rep;
    if (c_tilde == nullptr || p == nullptr || d == nullptr || true_c.m == 0) {
        rep.reject = "null_input";
        return rep;
    }
    const std::size_t m = true_c.m;
    if (!all_finite(c_tilde, m) || !all_finite(p, m) || !all_finite(d, m)) {
        rep.reject = "non_finite";
        return rep;
    }
    const PointEstimate e = w_info_solve(true_c, p, d, a);
    if (!e.ok || !(e.w_info > 0.0)) {
        rep.reject = e.reject != nullptr ? e.reject : "w_info_unavailable";
        return rep;
    }
    rep.ideal_inv_w = 1.0 / e.w_info;
    /* c~^T C c~: 对任意 C 视图逐元素展开 (真实 C, 不用近似)。 */
    double quad = 0.0;
    if (true_c.kind == CovarianceView::Kind::diagonal) {
        for (std::size_t i = 0; i < m; ++i) quad += c_tilde[i] * true_c.sigma2[i] * c_tilde[i];
    } else if (true_c.kind == CovarianceView::Kind::dense_spd) {
        for (std::size_t i = 0; i < m; ++i)
            for (std::size_t j = 0; j < m; ++j)
                quad += c_tilde[i] * true_c.c[i * m + j] * c_tilde[j];
    } else {
        /* D + L L^T */
        if (true_c.d_diag == nullptr) {
            rep.reject = "null_input";
            return rep;
        }
        for (std::size_t i = 0; i < m; ++i) quad += c_tilde[i] * true_c.d_diag[i] * c_tilde[i];
        if (true_c.r > 0 && true_c.l != nullptr) {
            for (std::size_t alpha = 0; alpha < true_c.r; ++alpha) {
                double s = 0.0;
                for (std::size_t i = 0; i < m; ++i) s += true_c.l[i * true_c.r + alpha] * c_tilde[i];
                quad += s * s;
            }
        }
    }
    if (!std::isfinite(quad) || quad <= 0.0) {
        rep.reject = "non_positive_quadratic_form";
        return rep;
    }
    rep.c_tilde_C_c_tilde = quad;
    rep.variance_ratio = quad / rep.ideal_inv_w;
    rep.reported = true;
    rep.ok = true;
    return rep;
}

FluxEstimate combine_point_estimates(const std::vector<PointEstimate>& points) {
    FluxEstimate out;
    if (points.empty()) {
        out.reject = "no_points";
        return out;
    }
    for (const auto& e : points) {
        if (!e.ok) {
            out.reject = "invalid_point_estimate";
            return out;
        }
        out.sum_q += e.q;
        out.sum_w += e.w_info;
    }
    if (!(out.sum_w > 0.0) || !std::isfinite(out.sum_w)) {
        out.reject = "zero_total_w";
        return out;
    }
    out.f_hat = out.sum_q / out.sum_w;
    out.var_f = 1.0 / out.sum_w;
    out.ok = true;
    return out;
}

}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */

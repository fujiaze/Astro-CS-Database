// gradient_fitter.cpp - 2D 多项式梯度曲面稳健拟合器实现
// 算法: IRLS + Tukey biweight + Ridge L2 + 帽子矩阵 LOOCV 自动选阶
// 对应 Python: lib/photometric_calib/archive/gradient_fitter.py

#include "gradient_fitter.h"
#include "log_macros.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace pc {

GradientFitter::GradientFitter() {}

// ----------------------------------------------------------------------------
// 公共静态工具
// ----------------------------------------------------------------------------

std::vector<std::pair<int,int>> GradientFitter::monomial_terms(int order) {
    std::vector<std::pair<int,int>> terms;
    for (int total = 0; total <= order; ++total) {
        for (int j = 0; j <= total; ++j) {
            int k = total - j;
            terms.push_back({j, k});
        }
    }
    return terms;
}

std::vector<double> GradientFitter::eval_basis(double x_norm, double y_norm, int order) {
    auto terms = monomial_terms(order);
    std::vector<double> basis(terms.size());
    // 预计算 x_norm 的幂
    std::vector<double> x_pow(order + 1), y_pow(order + 1);
    x_pow[0] = 1.0; y_pow[0] = 1.0;
    for (int e = 1; e <= order; ++e) {
        x_pow[e] = x_pow[e-1] * x_norm;
        y_pow[e] = y_pow[e-1] * y_norm;
    }
    for (size_t idx = 0; idx < terms.size(); ++idx) {
        int j = terms[idx].first;
        int k = terms[idx].second;
        basis[idx] = x_pow[j] * y_pow[k];
    }
    return basis;
}

double GradientFitter::eval_surface(const GradientSurface& surface,
                                    double x_norm, double y_norm) {
    auto basis = eval_basis(x_norm, y_norm, surface.order);
    double val = 0.0;
    for (size_t i = 0; i < basis.size() && i < surface.coeffs.size(); ++i) {
        val += surface.coeffs[i] * basis[i];
    }
    return val;
}

GradientSurface GradientFitter::identity_surface() {
    GradientSurface s;
    s.order = 1;
    s.coeffs.assign(3, 0.0); // 1阶 3项全 0: r=0 -> M=1
    s.loocv_error = std::numeric_limits<double>::infinity();
    s.residual_median = 0.0;
    s.residual_std = 0.0;
    s.n_used = 0;
    s.n_rejected = 0;
    s.ridge_alpha = 0.0;
    return s;
}

// ----------------------------------------------------------------------------
// 私有静态: 坐标归一化 / 设计矩阵
// ----------------------------------------------------------------------------

std::vector<double> GradientFitter::normalize_coords(
    const std::vector<double>& arr, double img_size) {
    std::vector<double> out(arr.size());
    double scale = (img_size > 0.0) ? (2.0 / img_size) : 0.0;
    for (size_t i = 0; i < arr.size(); ++i) {
        out[i] = arr[i] * scale - 1.0;
    }
    return out;
}

std::vector<std::vector<double>> GradientFitter::build_design_matrix(
    const std::vector<double>& x_norm,
    const std::vector<double>& y_norm, int order) {
    auto terms = monomial_terms(order);
    size_t n = x_norm.size();
    size_t p = terms.size();
    std::vector<std::vector<double>> X(n, std::vector<double>(p, 0.0));

    // 预计算每点的 x/y 幂
    std::vector<std::vector<double>> x_pow(n, std::vector<double>(order + 1));
    std::vector<std::vector<double>> y_pow(n, std::vector<double>(order + 1));
    for (size_t i = 0; i < n; ++i) {
        x_pow[i][0] = 1.0; y_pow[i][0] = 1.0;
        for (int e = 1; e <= order; ++e) {
            x_pow[i][e] = x_pow[i][e-1] * x_norm[i];
            y_pow[i][e] = y_pow[i][e-1] * y_norm[i];
        }
    }
    for (size_t i = 0; i < n; ++i) {
        for (size_t idx = 0; idx < p; ++idx) {
            int j = terms[idx].first;
            int k = terms[idx].second;
            X[i][idx] = x_pow[i][j] * y_pow[i][k];
        }
    }
    return X;
}

// ----------------------------------------------------------------------------
// 私有: IRLS + Tukey biweight + Ridge 拟合
// ----------------------------------------------------------------------------

void GradientFitter::irls_fit(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& r,
    double ridge_alpha,
    std::vector<double>& coeffs,
    std::vector<double>& weights,
    std::vector<double>& residuals) const {

    int n = (int)X.size();
    int p = (n > 0) ? (int)X[0].size() : 0;
    if (n == 0 || p == 0) {
        coeffs.assign(p, 0.0);
        weights.assign(n, 0.0);
        residuals.assign(n, 0.0);
        return;
    }

    weights.assign(n, 1.0);
    coeffs.assign(p, 0.0);
    residuals.assign(n, 0.0);

    // Ridge 正则化对角矩阵: reg_diag[0]=0 (常数项不正则), 其余=1
    std::vector<double> reg_diag(p, 1.0);
    reg_diag[0] = 0.0;

    // 初始 OLS (Ridge with current weights=1): (X^T X + alpha*D) beta = X^T r
    auto compute_coeffs = [&](const std::vector<double>& w,
                              std::vector<double>& out_coeffs) {
        // A = X^T W X + alpha * diag(reg_diag), b = X^T W r
        std::vector<std::vector<double>> A(p, std::vector<double>(p, 0.0));
        std::vector<double> b(p, 0.0);
        for (int i = 0; i < n; ++i) {
            for (int a = 0; a < p; ++a) {
                b[a] += X[i][a] * w[i] * r[i];
                for (int c = 0; c < p; ++c) {
                    A[a][c] += X[i][a] * X[i][c] * w[i];
                }
            }
        }
        for (int a = 0; a < p; ++a) {
            A[a][a] += ridge_alpha * reg_diag[a];
        }
        std::vector<double> x_sol;
        if (!solve_linear(A, b, x_sol)) {
            // 奇异, 退化到最小二乘 (伪逆 fallback)
            // 简化: 用 lstsq via A^T A 的伪逆 - 此处用 pinv via inverse fallback
            std::vector<std::vector<double>> A_inv;
            if (inverse_matrix(A, A_inv)) {
                x_sol.assign(p, 0.0);
                for (int a = 0; a < p; ++a) {
                    for (int c = 0; c < p; ++c) {
                        x_sol[a] += A_inv[a][c] * b[c];
                    }
                }
            } else {
                x_sol.assign(p, 0.0);
            }
        }
        out_coeffs = x_sol;
    };

    compute_coeffs(weights, coeffs);

    for (int iteration = 0; iteration < IRLS_MAX_ITER; ++iteration) {
        // 残差
        for (int i = 0; i < n; ++i) {
            double pred = 0.0;
            for (int a = 0; a < p; ++a) pred += X[i][a] * coeffs[a];
            residuals[i] = r[i] - pred;
        }

        // MAD
        double med = median(residuals);
        std::vector<double> abs_dev(n);
        for (int i = 0; i < n; ++i) abs_dev[i] = std::abs(residuals[i] - med);
        double mad = median(abs_dev);
        double sigma = mad / MAD_SCALE;
        if (sigma < 1e-12) sigma = 1e-12;

        // Tukey biweight 权重
        double c = TUKEY_C * sigma;
        std::vector<double> new_weights(n);
        for (int i = 0; i < n; ++i) {
            double u = residuals[i] / c;
            if (std::abs(u) < 1.0) {
                double t = 1.0 - u * u;
                new_weights[i] = t * t;
            } else {
                new_weights[i] = 0.0;
            }
        }

        // 加权求解
        std::vector<double> new_coeffs;
        compute_coeffs(new_weights, new_coeffs);

        // 收敛判定
        double max_change = 0.0;
        for (int a = 0; a < p; ++a) {
            double d = std::abs(new_coeffs[a] - coeffs[a]);
            if (d > max_change) max_change = d;
        }
        coeffs = new_coeffs;
        weights = new_weights;

        int n_used = 0;
        for (int i = 0; i < n; ++i) if (weights[i] > 0.0) ++n_used;
        (void)n_used; // 抑制 -Wunused-but-set-variable (LOG_DEBUG 在 release 模式下为空)
        LOG_DEBUG("IRLS iter %d: change=%.2e, used=%d/%d, sigma=%.4e, alpha=%.2e",
                  iteration, max_change, n_used, n, sigma, ridge_alpha);

        if (max_change < IRLS_TOL) {
            LOG_DEBUG("IRLS converged at iter %d", iteration);
            break;
        }
    }

    // 最终残差
    for (int i = 0; i < n; ++i) {
        double pred = 0.0;
        for (int a = 0; a < p; ++a) pred += X[i][a] * coeffs[a];
        residuals[i] = r[i] - pred;
    }
}

// ----------------------------------------------------------------------------
// 私有: 帽子矩阵 LOOCV
// ----------------------------------------------------------------------------

double GradientFitter::loocv_score(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& r,
    const std::vector<double>& weights,
    double ridge_alpha) const {

    int n = (int)X.size();
    int p = (n > 0) ? (int)X[0].size() : 0;
    if (n == 0 || p == 0) return std::numeric_limits<double>::infinity();

    std::vector<double> reg_diag(p, 1.0);
    reg_diag[0] = 0.0;

    // A = X^T W X + alpha * D, b = X^T W r
    std::vector<std::vector<double>> A(p, std::vector<double>(p, 0.0));
    std::vector<double> b(p, 0.0);
    for (int i = 0; i < n; ++i) {
        for (int a = 0; a < p; ++a) {
            b[a] += X[i][a] * weights[i] * r[i];
            for (int c = 0; c < p; ++c) {
                A[a][c] += X[i][a] * X[i][c] * weights[i];
            }
        }
    }
    for (int a = 0; a < p; ++a) A[a][a] += ridge_alpha * reg_diag[a];

    // beta = A^{-1} b
    std::vector<double> beta;
    if (!solve_linear(A, b, beta)) {
        std::vector<std::vector<double>> A_inv;
        if (!inverse_matrix(A, A_inv)) {
            return std::numeric_limits<double>::infinity();
        }
        beta.assign(p, 0.0);
        for (int a = 0; a < p; ++a) {
            for (int c = 0; c < p; ++c) beta[a] += A_inv[a][c] * b[c];
        }
    }

    // 残差
    std::vector<double> residuals(n);
    for (int i = 0; i < n; ++i) {
        double pred = 0.0;
        for (int a = 0; a < p; ++a) pred += X[i][a] * beta[a];
        residuals[i] = r[i] - pred;
    }

    // A^{-1}
    std::vector<std::vector<double>> A_inv;
    if (!inverse_matrix(A, A_inv)) {
        return std::numeric_limits<double>::infinity();
    }

    // h_diag[i] = W[i] * X[i] · (A_inv · X[i])
    // 计算每个样本的杠杆值
    double cv_sum = 0.0;
    int cv_count = 0;
    for (int i = 0; i < n; ++i) {
        if (weights[i] <= 0.0 || !std::isfinite(residuals[i])) continue;
        // t = A_inv · X[i]
        std::vector<double> t(p, 0.0);
        for (int a = 0; a < p; ++a) {
            for (int c = 0; c < p; ++c) t[a] += A_inv[a][c] * X[i][c];
        }
        // X[i] · t
        double xt = 0.0;
        for (int a = 0; a < p; ++a) xt += X[i][a] * t[a];
        double h_diag = weights[i] * xt;
        double denom = 1.0 - h_diag;
        if (std::abs(denom) < 1e-10) continue;
        double loo_resid = residuals[i] / denom;
        cv_sum += weights[i] * loo_resid * loo_resid;
        ++cv_count;
    }
    if (cv_count == 0) return std::numeric_limits<double>::infinity();
    return cv_sum / cv_count;
}

// ----------------------------------------------------------------------------
// 私有: LOOCV 自动选阶 + Ridge alpha 网格
// ----------------------------------------------------------------------------

void GradientFitter::select_order(
    const std::vector<double>& x_arr,
    const std::vector<double>& y_arr,
    const std::vector<double>& r_arr,
    double img_w, double img_h, int max_order,
    int& best_order, double& best_cv, double& best_alpha) const {

    int n = (int)r_arr.size();
    auto x_norm = normalize_coords(x_arr, img_w);
    auto y_norm = normalize_coords(y_arr, img_h);

    int upper = std::min(max_order, MAX_ORDER);
    if (upper < MIN_ORDER) upper = MIN_ORDER;

    // Ridge alpha 网格 (与 Python 一致: {0, 0.1, 1, 10, 100, 1000})
    static const double alphas[] = {0.0, 0.1, 1.0, 10.0, 100.0, 1000.0};
    static const int n_alphas = 6;

    // results: (order, alpha) -> cv
    struct Key { int order; double alpha; };
    struct Entry { Key key; double cv; };
    std::vector<Entry> results;

    double cv_min = std::numeric_limits<double>::infinity();

    for (int order = MIN_ORDER; order <= upper; ++order) {
        int n_coeffs = (order + 1) * (order + 2) / 2;
        int min_samples = n_coeffs * MIN_SAMPLE_FACTOR;
        if (n < min_samples) {
            LOG_INFO("order %d: n=%d < min_samples=%d, skip", order, n, min_samples);
            continue;
        }
        auto X = build_design_matrix(x_norm, y_norm, order);
        for (int ai = 0; ai < n_alphas; ++ai) {
            double alpha = alphas[ai];
            std::vector<double> coeffs, weights, residuals;
            irls_fit(X, r_arr, alpha, coeffs, weights, residuals);
            double cv = loocv_score(X, r_arr, weights, alpha);
            int n_used = 0;
            for (int i = 0; i < n; ++i) if (weights[i] > 0.0) ++n_used;
            LOG_INFO("order %d, alpha=%.2f: LOOCV=%.6e, used=%d/%d",
                     order, alpha, cv, n_used, n);
            results.push_back({{order, alpha}, cv});
            if (cv < cv_min) cv_min = cv;
        }
    }

    if (results.empty()) {
        LOG_INFO("no valid order, fallback to MIN_ORDER=%d", MIN_ORDER);
        best_order = MIN_ORDER;
        best_cv = std::numeric_limits<double>::infinity();
        best_alpha = 0.0;
        return;
    }

    // 选最简模型: 最低阶 + 最小 alpha, 在 cv_min * (1 + 10%) 容差内
    best_order = upper;
    best_alpha = 0.0;
    best_cv = std::numeric_limits<double>::infinity();
    double tol = cv_min * (1.0 + CV_REL_TOL);
    bool found = false;
    for (int order = MIN_ORDER; order <= upper && !found; ++order) {
        for (int ai = 0; ai < n_alphas; ++ai) {
            double alpha = alphas[ai];
            // 查找结果
            for (const auto& e : results) {
                if (e.key.order == order && e.key.alpha == alpha) {
                    if (e.cv <= tol) {
                        best_order = order;
                        best_alpha = alpha;
                        best_cv = e.cv;
                        found = true;
                    }
                    break;
                }
            }
            if (found) break;
        }
    }
    if (!found) {
        // 退化: 取 cv 最小的
        auto it = std::min_element(results.begin(), results.end(),
            [](const Entry& a, const Entry& b) { return a.cv < b.cv; });
        best_order = it->key.order;
        best_alpha = it->key.alpha;
        best_cv = it->cv;
    }
    LOG_INFO("select_order: best_order=%d, alpha=%.2f, cv=%.6e, cv_min=%.6e, tol=%.0f%%",
             best_order, best_alpha, best_cv, cv_min, CV_REL_TOL * 100);
}

// ----------------------------------------------------------------------------
// 私有: 拟合核心
// ----------------------------------------------------------------------------

GradientSurface GradientFitter::fit_surface(
    const std::vector<double>& x_arr,
    const std::vector<double>& y_arr,
    const std::vector<double>& val_arr,
    double img_w, double img_h, int max_order,
    const std::string& channel_name) {

    int n = (int)val_arr.size();
    LOG_INFO("%s fit: n=%d samples, img=%gx%g", channel_name.c_str(), n, img_w, img_h);

    if (n < MIN_MATCHED_FOR_FIT) {
        LOG_INFO("%s: n=%d < %d, return identity surface", channel_name.c_str(), n, MIN_MATCHED_FOR_FIT);
        return identity_surface();
    }

    int best_order;
    double best_cv, best_alpha;
    select_order(x_arr, y_arr, val_arr, img_w, img_h, max_order,
                 best_order, best_cv, best_alpha);

    auto x_norm = normalize_coords(x_arr, img_w);
    auto y_norm = normalize_coords(y_arr, img_h);
    auto X = build_design_matrix(x_norm, y_norm, best_order);

    std::vector<double> coeffs, weights, residuals;
    irls_fit(X, val_arr, best_alpha, coeffs, weights, residuals);

    int n_used = 0;
    for (int i = 0; i < n; ++i) if (weights[i] > 0.0) ++n_used;
    int n_rejected = n - n_used;

    // 内点残差统计
    std::vector<double> resid_used;
    resid_used.reserve(n_used);
    for (int i = 0; i < n; ++i) {
        if (weights[i] > 0.0) resid_used.push_back(residuals[i]);
    }
    double resid_median = 0.0, resid_std = 0.0;
    if (!resid_used.empty()) {
        resid_median = median(resid_used);
        double mean = std::accumulate(resid_used.begin(), resid_used.end(), 0.0) / resid_used.size();
        double var = 0.0;
        for (double v : resid_used) var += (v - mean) * (v - mean);
        resid_std = std::sqrt(var / resid_used.size());
    }

    GradientSurface s;
    s.order = best_order;
    s.coeffs = coeffs;
    s.loocv_error = best_cv;
    s.residual_median = resid_median;
    s.residual_std = resid_std;
    s.n_used = n_used;
    s.n_rejected = n_rejected;
    s.ridge_alpha = best_alpha;
    LOG_INFO("%s fit done: order=%d, alpha=%.2f, cv=%.6e, used=%d, rejected=%d, resid_med=%.4e, resid_std=%.4e",
             channel_name.c_str(), best_order, best_alpha, best_cv,
             n_used, n_rejected, resid_median, resid_std);
    return s;
}

GradientSurface GradientFitter::fit_multiplicative(
    const std::vector<double>& x_arr,
    const std::vector<double>& y_arr,
    const std::vector<double>& r_arr,
    double img_w, double img_h, int max_order) {
    return fit_surface(x_arr, y_arr, r_arr, img_w, img_h, max_order, "mult");
}

// ----------------------------------------------------------------------------
// R² 计算
// ----------------------------------------------------------------------------

double GradientFitter::compute_r_squared(
    const GradientSurface& surface,
    const std::vector<double>& x_arr,
    const std::vector<double>& y_arr,
    const std::vector<double>& val_arr,
    double img_w, double img_h) const {

    int n = (int)x_arr.size();
    if (n < 3) return 0.0;

    auto x_norm = normalize_coords(x_arr, img_w);
    auto y_norm = normalize_coords(y_arr, img_h);
    auto X = build_design_matrix(x_norm, y_norm, surface.order);

    // IRLS 获取权重 (与拟合阶段一致)
    std::vector<double> coeffs, weights, residuals;
    irls_fit(X, val_arr, surface.ridge_alpha, coeffs, weights, residuals);

    // 内点 R²
    double ss_res = 0.0, ss_tot = 0.0;
    double mean_val = 0.0;
    int n_in = 0;
    for (int i = 0; i < n; ++i) {
        if (weights[i] > 0.0) {
            mean_val += val_arr[i];
            ++n_in;
        }
    }
    if (n_in < 3) return 0.0;
    mean_val /= n_in;

    for (int i = 0; i < n; ++i) {
        if (weights[i] > 0.0) {
            double pred = 0.0;
            for (int a = 0; a < (int)X[i].size(); ++a) pred += X[i][a] * surface.coeffs[a];
            double res = val_arr[i] - pred;
            ss_res += res * res;
            double dev = val_arr[i] - mean_val;
            ss_tot += dev * dev;
        }
    }
    if (ss_tot < 1e-15) return 0.0;
    return 1.0 - ss_res / ss_tot;
}

// ----------------------------------------------------------------------------
// 曲面评估
// ----------------------------------------------------------------------------

std::vector<double> GradientFitter::evaluate_surface_fullimage(
    const GradientSurface& surface, int width, int height) const {

    std::vector<double> out(width * height);
    double sx = (width > 0) ? (2.0 / width) : 0.0;
    double sy = (height > 0) ? (2.0 / height) : 0.0;
    int order = surface.order;
    auto terms = monomial_terms(order);

    for (int y = 0; y < height; ++y) {
        double y_norm = y * sy - 1.0;
        for (int x = 0; x < width; ++x) {
            double x_norm = x * sx - 1.0;
            out[y * width + x] = eval_surface(surface, x_norm, y_norm);
        }
    }
    return out;
}

std::vector<double> GradientFitter::evaluate_surface_points(
    const GradientSurface& surface,
    const std::vector<double>& x_arr,
    const std::vector<double>& y_arr,
    double img_w, double img_h) const {

    std::vector<double> out(x_arr.size());
    double sx = (img_w > 0) ? (2.0 / img_w) : 0.0;
    double sy = (img_h > 0) ? (2.0 / img_h) : 0.0;
    for (size_t i = 0; i < x_arr.size(); ++i) {
        double x_norm = x_arr[i] * sx - 1.0;
        double y_norm = y_arr[i] * sy - 1.0;
        out[i] = eval_surface(surface, x_norm, y_norm);
    }
    return out;
}

// ----------------------------------------------------------------------------
// 线性代数工具
// ----------------------------------------------------------------------------

bool GradientFitter::solve_linear(
    const std::vector<std::vector<double>>& A,
    const std::vector<double>& b,
    std::vector<double>& x) {
    int n = (int)A.size();
    if (n == 0) { x.clear(); return true; }
    int p = (int)A[0].size();
    if (n != p || (int)b.size() != n) { x.assign(p, 0.0); return false; }

    // 增广矩阵 [A | b]
    std::vector<std::vector<double>> M(n, std::vector<double>(p + 1));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < p; ++j) M[i][j] = A[i][j];
        M[i][p] = b[i];
    }

    // 高斯消元 + 部分主元
    for (int k = 0; k < n; ++k) {
        // 主元
        int piv = k;
        double maxv = std::abs(M[k][k]);
        for (int i = k + 1; i < n; ++i) {
            double v = std::abs(M[i][k]);
            if (v > maxv) { maxv = v; piv = i; }
        }
        if (maxv < 1e-14) return false; // 奇异
        if (piv != k) std::swap(M[piv], M[k]);

        // 消元
        for (int i = k + 1; i < n; ++i) {
            double f = M[i][k] / M[k][k];
            for (int j = k; j <= p; ++j) M[i][j] -= f * M[k][j];
        }
    }

    // 回代
    x.assign(n, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double s = M[i][p];
        for (int j = i + 1; j < n; ++j) s -= M[i][j] * x[j];
        if (std::abs(M[i][i]) < 1e-14) return false;
        x[i] = s / M[i][i];
    }
    return true;
}

bool GradientFitter::inverse_matrix(
    const std::vector<std::vector<double>>& A,
    std::vector<std::vector<double>>& A_inv) {
    int n = (int)A.size();
    if (n == 0) { A_inv.clear(); return true; }
    int p = (int)A[0].size();
    if (n != p) { A_inv.assign(n, std::vector<double>(p, 0.0)); return false; }

    // 增广 [A | I]
    std::vector<std::vector<double>> M(n, std::vector<double>(2 * n));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) M[i][j] = A[i][j];
        for (int j = 0; j < n; ++j) M[i][n + j] = (i == j) ? 1.0 : 0.0;
    }

    // 高斯-若尔当
    for (int k = 0; k < n; ++k) {
        int piv = k;
        double maxv = std::abs(M[k][k]);
        for (int i = k + 1; i < n; ++i) {
            double v = std::abs(M[i][k]);
            if (v > maxv) { maxv = v; piv = i; }
        }
        if (maxv < 1e-14) return false; // 奇异
        if (piv != k) std::swap(M[piv], M[k]);

        double d = M[k][k];
        for (int j = 0; j < 2 * n; ++j) M[k][j] /= d;

        for (int i = 0; i < n; ++i) {
            if (i == k) continue;
            double f = M[i][k];
            if (f == 0.0) continue;
            for (int j = 0; j < 2 * n; ++j) M[i][j] -= f * M[k][j];
        }
    }

    A_inv.assign(n, std::vector<double>(n));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) A_inv[i][j] = M[i][n + j];
    }
    return true;
}

// ----------------------------------------------------------------------------
// 统计工具
// ----------------------------------------------------------------------------

double GradientFitter::median(std::vector<double> arr) {
    if (arr.empty()) return 0.0;
    std::sort(arr.begin(), arr.end());
    size_t n = arr.size();
    if (n % 2 == 0) return (arr[n/2 - 1] + arr[n/2]) * 0.5;
    return arr[n/2];
}

double GradientFitter::median_abs_dev(const std::vector<double>& arr, double med) {
    if (arr.empty()) return 0.0;
    std::vector<double> abs_dev(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) abs_dev[i] = std::abs(arr[i] - med);
    return median(abs_dev);
}

} // namespace pc

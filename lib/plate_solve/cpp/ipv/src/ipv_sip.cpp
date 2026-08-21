// ============================================================================
// ipv_sip.cpp - IPV SIP 多项式畸变拟合实现
//
// 算法:
// 1. 降阶判定 (依据内点数)
// 2. 坐标归一化 (避免数值不稳定)
// 3. 构造 SIP 多项式基 (3 阶 10 项, 2 阶 6 项)
// 4. IRLS + Huber 权重拟合 (分别对 A 和 B)
// 5. 失败兜底 (奇异 / 系数过大 / RMS 过大)
// 6. 拟合 RMS 评估
//
// 残差模型 ( 像素空间):
// linear_pred = s·R(θ)·U + t
// r = W - linear_pred
// r_x ≈ A_poly(xn, yn), r_y ≈ B_poly(xn, yn)
//
// 系数索引: SIPCoeffs.A[i*6+j] 对应 dx^i * dy^j (i+j <= order)
//
// 日期: 2026-07-04
// ============================================================================

#include "ipv_sip.h"

#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cstdarg>

namespace ipv {

// ---------------------------------------------------------------------------
// 内部辅助: 日志输出 (logger 为空时静默)
// 直接复用 Logger 自带的 infof/warnf 等格式化方法, 避免重复 va_list 处理
// ---------------------------------------------------------------------------
static inline void sip_log(Logger* logger, Logger::Level lvl, const std::string& msg) {
    if (logger) logger->log(lvl, msg);
}

// 格式化日志: 用 snprintf 后调用 log
static inline void sip_logf(Logger* logger, Logger::Level lvl, const char* fmt, ...) {
    if (!logger) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    logger->log(lvl, std::string(buf));
}

// ---------------------------------------------------------------------------
// SIP 多项式基索引表 (按总阶数升序, 同阶内 i 降序)
// k=0:(0,0) k=1:(1,0) k=2:(0,1)
// k=3:(2,0) k=4:(1,1) k=5:(0,2)
// k=6:(3,0) k=7:(2,1) k=8:(1,2) k=9:(0,3)
// ---------------------------------------------------------------------------
struct SipBasisIndex {
    int i;
    int j;
};

// 根据阶数获取基索引表
static int get_basis_table(int order, SipBasisIndex* table) {
    int k = 0;
    for (int deg = 0; deg <= order; ++deg) {
        for (int i = deg; i >= 0; --i) {
            int j = deg - i;
            table[k].i = i;
            table[k].j = j;
            ++k;
        }
    }
    return k;   // 项数: order=2 → 6, order=3 → 10
}

// 计算单项式基值: basis_k(x, y) = x^i * y^j
static inline double basis_value(const SipBasisIndex& idx, double x, double y) {
    // 用 std::pow 处理 0 次幂, 但 0/1/2/3 次幂直接乘更快
    double xi = 1.0, yj = 1.0;
    for (int p = 0; p < idx.i; ++p) xi *= x;
    for (int p = 0; p < idx.j; ++p) yj *= y;
    return xi * yj;
}

// ---------------------------------------------------------------------------
// 高斯消元解 K×K 线性方程组 (带列主元选取)
// 输入: A 是 K×(K+1) 增广矩阵 (行优先), K 维
// 输出: x 长度 K
// 返回: true 成功, false 矩阵奇异 (主元接近 0)
// ---------------------------------------------------------------------------
static bool gaussian_solve(std::vector<double>& A, int K, std::vector<double>& x) {
    // A 是 K 行, 每行 K+1 列 (增广)
    const double PIVOT_EPS = 1e-12; // 奇异判定阈值 (PLATESOLVE.md 数值风险)

    for (int col = 0; col < K; ++col) {
        // 列主元: 找当前列绝对值最大的行
        int pivot_row = col;
        double max_val = std::fabs(A[col * (K + 1) + col]);
        for (int r = col + 1; r < K; ++r) {
            double v = std::fabs(A[r * (K + 1) + col]);
            if (v > max_val) {
                max_val = v;
                pivot_row = r;
            }
        }

        // 主元接近 0 → 奇异
        if (max_val < PIVOT_EPS) {
            return false;
        }

        // 交换行
        if (pivot_row != col) {
            for (int c = 0; c <= K; ++c) {
                std::swap(A[col * (K + 1) + c], A[pivot_row * (K + 1) + c]);
            }
        }

        // 消元: 当前行以下所有行
        double pivot = A[col * (K + 1) + col];
        for (int r = col + 1; r < K; ++r) {
            double factor = A[r * (K + 1) + col] / pivot;
            if (factor == 0.0) continue;
            for (int c = col; c <= K; ++c) {
                A[r * (K + 1) + c] -= factor * A[col * (K + 1) + c];
            }
        }
    }

    // 回代
    x.assign(K, 0.0);
    for (int r = K - 1; r >= 0; --r) {
        double s = A[r * (K + 1) + K];
        for (int c = r + 1; c < K; ++c) {
            s -= A[r * (K + 1) + c] * x[c];
        }
        double diag = A[r * (K + 1) + r];
        if (std::fabs(diag) < PIVOT_EPS) return false;
        x[r] = s / diag;
    }
    return true;
}

// ---------------------------------------------------------------------------
// IRLS + Huber 权重拟合单组多项式 (A 或 B)
// 输入:
// X - N×K 设计矩阵 (行优先, N 个样本, K 个基)
// b - N 维目标残差 (r_x 或 r_y)
// K - 基项数
// n - 样本数
// max_iter - 最大 IRLS 迭代次数
// converge_eps - 收敛阈值 (max|Δc|)
// 输出:
// coeff - K 维系数
// final_w - 最终权重 (用于 RMS 计算)
// final_r - 最终残差 (b - X·coeff)
// 返回:
// true 成功, false 正规方程奇异
// ---------------------------------------------------------------------------
static bool irls_huber_fit(
    const std::vector<double>& X,
    const std::vector<double>& b,
    int K, int n,
    int max_iter, double converge_eps,
    std::vector<double>& coeff,
    std::vector<double>& final_w,
    std::vector<double>& final_r
) {
    std::vector<double> w(n, 1.0);          // 初始权重 = 1
    coeff.assign(K, 0.0);
    std::vector<double> prev_coeff(K, 0.0);

    // 工作矩阵: K×(K+1) 增广矩阵（CodeQL #5：先提升 size_t 防溢出）
    std::vector<double> Amat((std::size_t)K * (std::size_t)(K + 1), 0.0);

    for (int iter = 0; iter < max_iter; ++iter) {
        // 构造正规方程 (A^T W A) c = A^T W b
        // Amat[i*(K+1)+j] = sum_n w_n * X[n*K+i] * X[n*K+j] (i,j < K)
        // Amat[i*(K+1)+K] = sum_n w_n * X[n*K+i] * b[n] (右端项)
        std::fill(Amat.begin(), Amat.end(), 0.0);

        for (int smp = 0; smp < n; ++smp) {
            double ws = w[smp];
            if (ws == 0.0) continue;
            const double* Xrow = &X[(size_t)smp * K];
            for (int i = 0; i < K; ++i) {
                double Xi = Xrow[i];
                if (Xi == 0.0) continue;
                double wXi = ws * Xi;
                // 累加到 A[i][j] (j >= i, 利用对称只算上三角然后镜像)
                for (int j = i; j < K; ++j) {
                    Amat[(size_t)i * (K + 1) + j] += wXi * Xrow[j];
                }
                // 右端项
                Amat[(size_t)i * (K + 1) + K] += wXi * b[smp];
            }
        }
        // 镜像下三角
        for (int i = 0; i < K; ++i) {
            for (int j = 0; j < i; ++j) {
                Amat[(size_t)i * (K + 1) + j] = Amat[(size_t)j * (K + 1) + i];
            }
        }

        // 解线性方程组
        std::vector<double> x_sol;
        if (!gaussian_solve(Amat, K, x_sol)) {
            return false;   // 奇异
        }

        // 计算新残差 r_i = b_i - X_i · c
        std::vector<double> r(n, 0.0);
        for (int smp = 0; smp < n; ++smp) {
            const double* Xrow = &X[(size_t)smp * K];
            double pred = 0.0;
            for (int i = 0; i < K; ++i) pred += Xrow[i] * x_sol[i];
            r[smp] = b[smp] - pred;
        }

        // 收敛判定: max|Δc|
        double max_delta = 0.0;
        for (int i = 0; i < K; ++i) {
            double d = std::fabs(x_sol[i] - prev_coeff[i]);
            if (d > max_delta) max_delta = d;
        }
        prev_coeff = x_sol;
        coeff = x_sol;

        // Huber 权重更新
        // MAD = median(|r_i|), δ = 1.345 × MAD
        std::vector<double> abs_r(n);
        for (int smp = 0; smp < n; ++smp) abs_r[smp] = std::fabs(r[smp]);
        std::sort(abs_r.begin(), abs_r.end());
        double median_abs_r;
        if (n % 2 == 1) {
            median_abs_r = abs_r[n / 2];
        } else {
            median_abs_r = 0.5 * (abs_r[n / 2 - 1] + abs_r[n / 2]);
        }
        double delta = 1.345 * median_abs_r;
        // 避免 δ=0 (所有残差为 0 时): 给个保护值
        if (delta < 1e-9) delta = 1e-9;

        for (int smp = 0; smp < n; ++smp) {
            double ar = std::fabs(r[smp]);
            if (ar <= delta) {
                w[smp] = 1.0;
            } else {
                w[smp] = delta / ar;
            }
        }

        final_w = w;
        final_r = r;

        if (max_delta < converge_eps) {
            break;   // 收敛
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// fit_sip 主函数
// ---------------------------------------------------------------------------
SIPCoeffs fit_sip(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& inliers,
    const SimTransform& tf,
    double s0,
    int img_width,
    int img_height,
    int order,
    Logger* logger
) {
    SIPCoeffs result;
    result.order = 0;
    for (int i = 0; i < 36; ++i) {
        result.A[i] = 0.0;
        result.B[i] = 0.0;
    }

    // -----------------------------------------------------------------
    // 0. 基本检查
    // -----------------------------------------------------------------
    const int n_pairs = (int)inliers.size();
    // 2 阶 SIP 有 6 个系数 per A/B, 至少需要 7 个点留余量
    if (n_pairs < 7) {
        sip_logf(logger, Logger::INFO,
                 "[sip] n_pairs=%d < 7, 返回 order=0", n_pairs);
        return result;
    }
    if (img_width <= 0 || img_height <= 0) {
        sip_logf(logger, Logger::WARN,
                 "[sip] 无效图像尺寸 %dx%d, 返回 order=0", img_width, img_height);
        return result;
    }
    if (order < 2) {
        sip_logf(logger, Logger::INFO,
                 "[sip] 请求 order=%d < 2, 返回 order=0", order);
        return result;
    }

    // -----------------------------------------------------------------
    // 1. 降阶判定
    // -----------------------------------------------------------------
    int eff_order = order;
    if (n_pairs < 12 && eff_order > 2) {
        eff_order = 2;
        sip_logf(logger, Logger::INFO,
                 "[sip] n_pairs=%d < 12, 降阶到 order=2", n_pairs);
    }
    else if (n_pairs < 30 && eff_order > 2) {
        eff_order = 2;
        sip_logf(logger, Logger::INFO,
                 "[sip] n_pairs=%d < 30, 降阶到 order=2", n_pairs);
    }
    if (n_pairs < 60 && eff_order > 3) {
        eff_order = 3;   // 保守
        sip_logf(logger, Logger::INFO,
                 "[sip] n_pairs=%d < 60, 降阶到 order=3", n_pairs);
    }
    // 上限保护
    if (eff_order > 4) eff_order = 4;

    // -----------------------------------------------------------------
    // 2. 坐标归一化 + 残差计算
    // -----------------------------------------------------------------
    const double cx = img_width / 2.0;
    const double cy = img_height / 2.0;
    const double scale = std::max((double)img_width, (double)img_height) / 2.0;
    if (scale <= 0.0) {
        sip_logf(logger, Logger::WARN, "[sip] scale=0, 返回 order=0");
        return result;
    }

    const double cos_t = std::cos(tf.theta);
    const double sin_t = std::sin(tf.theta);

    // 收集有效样本 (跳过索引越界)
    std::vector<double> xn_arr, yn_arr;     // 归一化坐标
    std::vector<double> rx_arr, ry_arr;     // 残差 (W - linear_pred)
    xn_arr.reserve(n_pairs);
    yn_arr.reserve(n_pairs);
    rx_arr.reserve(n_pairs);
    ry_arr.reserve(n_pairs);

    for (const MatchPair& mp : inliers) {
        if (mp.u < 0 || mp.u >= (int)U.size()) continue;
        if (mp.w < 0 || mp.w >= (int)W.size()) continue;

        const StarPoint& u = U[mp.u];
        const StarPoint& w = W[mp.w];

        // 归一化坐标: xn = (U[u].x + cx) / scale
        // 注: U[u].x 是相对图像中心的像素坐标 , + cx 转回绝对像素
        xn_arr.push_back((u.x + cx) / scale);
        yn_arr.push_back((u.y + cy) / scale);

        // 线性预测 ( 像素空间): linear_pred = s·R(θ)·U + t
        const double x_pred = tf.s * (cos_t * u.x - sin_t * u.y) + tf.tx;
        const double y_pred = tf.s * (sin_t * u.x + cos_t * u.y) + tf.ty;

        // 残差 r = W - linear_pred
        rx_arr.push_back(w.x - x_pred);
        ry_arr.push_back(w.y - y_pred);
    }

    const int n = (int)xn_arr.size();
    if (n < 7) {
        sip_logf(logger, Logger::INFO,
                 "[sip] 有效样本 n=%d < 7, 返回 order=0", n);
        return result;
    }

    sip_logf(logger, Logger::INFO,
             "[sip] 开始拟合: eff_order=%d, n=%d, s0=%.4f, img=%dx%d",
             eff_order, n, s0, img_width, img_height);

    // -----------------------------------------------------------------
    // 3. 构造 SIP 多项式基表
    // -----------------------------------------------------------------
    SipBasisIndex basis_table[15];   // 最多 4 阶 = 15 项 ( 修复: 原 10 太小)
    const int K = get_basis_table(eff_order, basis_table);
    sip_logf(logger, Logger::DEBUG, "[sip] 基项数 K=%d (order=%d)", K, eff_order);

    // 构造设计矩阵 X (n×K), 行优先
    std::vector<double> X((size_t)n * K, 0.0);
    for (int smp = 0; smp < n; ++smp) {
        double x = xn_arr[smp];
        double y = yn_arr[smp];
        for (int k = 0; k < K; ++k) {
            X[(size_t)smp * K + k] = basis_value(basis_table[k], x, y);
        }
    }

    // -----------------------------------------------------------------
    // 4. IRLS + Huber 拟合 (分别对 A 和 B)
    // -----------------------------------------------------------------
    std::vector<double> coeff_a, coeff_b;
    std::vector<double> w_a, w_b;
    std::vector<double> r_a, r_b;

    const int    IRLS_MAX_ITER  = 15;   // IRLS 上限 (PLATESOLVE.md 数值风险)
    const double IRLS_CONV_EPS  = 1e-6; // IRLS 收敛 ε (PLATESOLVE.md 数值风险)

    bool ok_a = irls_huber_fit(X, rx_arr, K, n,
                                IRLS_MAX_ITER, IRLS_CONV_EPS,
                                coeff_a, w_a, r_a);
    if (!ok_a) {
        sip_logf(logger, Logger::WARN,
                 "[sip] A 多项式正规方程奇异, 返回 order=0");
        return result;
    }

    bool ok_b = irls_huber_fit(X, ry_arr, K, n,
                                IRLS_MAX_ITER, IRLS_CONV_EPS,
                                coeff_b, w_b, r_b);
    if (!ok_b) {
        sip_logf(logger, Logger::WARN,
                 "[sip] B 多项式正规方程奇异, 返回 order=0");
        return result;
    }

    // -----------------------------------------------------------------
    // 5. 失败兜底: 系数过大检查
    // -----------------------------------------------------------------
    double max_coeff = 0.0;
    for (int k = 0; k < K; ++k) {
        if (std::fabs(coeff_a[k]) > max_coeff) max_coeff = std::fabs(coeff_a[k]);
        if (std::fabs(coeff_b[k]) > max_coeff) max_coeff = std::fabs(coeff_b[k]);
    }
    if (max_coeff > 100.0) {
        sip_logf(logger, Logger::WARN,
                 "[sip] 系数过大 max_coeff=%.3f > 100, 返回 order=0", max_coeff);
        return result;
    }

    // -----------------------------------------------------------------
    // 6. 拟合 RMS 评估 (加权)
    // rms = sqrt(sum(wi * r_i²) / sum(wi))
    // -----------------------------------------------------------------
    double sum_wa = 0.0, sum_wa_r2 = 0.0;
    double sum_wb = 0.0, sum_wb_r2 = 0.0;
    for (int smp = 0; smp < n; ++smp) {
        sum_wa     += w_a[smp];
        sum_wa_r2  += w_a[smp] * r_a[smp] * r_a[smp];
        sum_wb     += w_b[smp];
        sum_wb_r2  += w_b[smp] * r_b[smp] * r_b[smp];
    }
    double rms_x = (sum_wa > 0.0) ? std::sqrt(sum_wa_r2 / sum_wa) : 0.0;
    double rms_y = (sum_wb > 0.0) ? std::sqrt(sum_wb_r2 / sum_wb) : 0.0;
    double total_rms = std::sqrt(rms_x * rms_x + rms_y * rms_y);

    if (total_rms > 10.0) {
        sip_logf(logger, Logger::WARN,
                 "[sip] 拟合 RMS 过大 total_rms=%.3f > 10px, 返回 order=0",
                 total_rms);
        return result;
    }

    // -----------------------------------------------------------------
    // 7. 系数填充到 SIPCoeffs
    // 索引: A[i*6+j] 对应 dx^i * dy^j
    // basis_table[k] = (i, j) → A[i*6+j]
    //
    // 坐标系转换 ( 修复):
    // fit_sip 内部用归一化坐标 xn=(U.x+cx)/scale 拟合得到 coeff_norm
    // 但标准 WCS SIP 期望原始像素坐标 (x-CRPIX) 的系数
    // 转换: A_orig[i,j] = coeff_norm[k] / scale^(i+j)
    // 因为 dx_orig^i * dy_orig^j = (xn*scale)^i * (yn*scale)^j
    // = xn^i * yn^j * scale^(i+j)
    // 所以 coeff_norm * xn^i * yn^j = coeff_norm/scale^(i+j) * dx_orig^i * dy_orig^j
    // -----------------------------------------------------------------
    for (int k = 0; k < K; ++k) {
        int i = basis_table[k].i;
        int j = basis_table[k].j;
        int idx = i * 6 + j;
        if (idx >= 0 && idx < 36) {
            int deg = i + j;
            double scale_pow = std::pow(scale, deg);  // scale^deg
            result.A[idx] = coeff_a[k] / scale_pow;
            result.B[idx] = coeff_b[k] / scale_pow;
        }
    }
    result.order = eff_order;

    // 重新计算 max_coeff (转换后)
    double max_coeff_orig = 0.0;
    for (int i = 0; i < 36; ++i) {
        if (std::fabs(result.A[i]) > max_coeff_orig) max_coeff_orig = std::fabs(result.A[i]);
        if (std::fabs(result.B[i]) > max_coeff_orig) max_coeff_orig = std::fabs(result.B[i]);
    }

    sip_logf(logger, Logger::INFO,
             "[sip] order=%d n=%d rms_x=%.3f rms_y=%.3f total_rms=%.3f max_coeff=%.3f",
             eff_order, n, rms_x, rms_y, total_rms, max_coeff);

    // 详细系数日志 (DEBUG)
    for (int k = 0; k < K; ++k) {
        int i = basis_table[k].i;
        int j = basis_table[k].j;
        sip_logf(logger, Logger::DEBUG,
                 "[sip]   A[%d][%d]=%.6f  B[%d][%d]=%.6f",
                 i, j, result.A[i * 6 + j],
                 i, j, result.B[i * 6 + j]);
    }

    return result;
}

} // namespace ipv

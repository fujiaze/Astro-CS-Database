// spherical_spline.cpp - 3D 嵌入球面样条拟合与评估实现
//
// 实现: 球面双调和 Green 函数核 + 3D 笛卡尔零空间 [1,x,y,z] + Eigen 稠密 LU 求解
//
// 数学推导 (spec §3.4.4):
//   最小化: Σ w_i (z_i - g(p_i))² + λ ||g||²_H
//   KKT 条件 → 线性系统:
//     [K + λ W^{-1}    P ] [w]   [z]
//     [P'              0 ] [v] = [0]
//   K_ij = K(γ(p_i,p_j)), P = 3D 笛卡尔设计矩阵 [1, x, y, z]
//   W = diag(weights_i), 高 SNR² → 权重大 → 正则项小 → 精确拟合
//
//   评估: g(p) = Σ_k w_k·K(γ(p,p_k)) + c0 + c1·x + c2·y + c3·z
//
// 3D 嵌入优势 (vs 球谐零空间):
//   - 球谐 [Y00,Y10,Y11,Y1-1] 在小 FOV 近似线性相关 → 矩阵病态
//   - 3D 笛卡尔 [1,x,y,z] 在任意 FOV 始终线性无关 → 良态
//   - 支持全天空 (局部马赛克到全天球马赛克, 不论单帧 FOV)
//
// 数值稳定性:
//   - 核函数 K(0) = 0 (对角线置零, 避免奇点)
//   - λI 正则项保证 (K + λW^{-1}) 非奇异
//   - gamma_eps 阈值防止 sin(γ/2) 过小
//   - PartialPivLU 适合稠密矩阵 (n+4 典型 100~500)

#include "spherical_spline.h"

#include <cmath>
#include <stdexcept>

#include <Eigen/Dense>
#include <Eigen/LU>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gradient {

namespace {
constexpr double DEG2RAD = M_PI / 180.0;
} // namespace

// ============================================================================
// 构造/析构
// ============================================================================
SphericalSpline::SphericalSpline() {}
SphericalSpline::~SphericalSpline() {}

// ============================================================================
// 球面点 (ra,dec) → 3D 单位向量 v=(x,y,z)
//   x = cos(dec)·cos(ra)
//   y = cos(dec)·sin(ra)
//   z = sin(dec)
// ============================================================================
void SphericalSpline::toUnitVector(double ra_deg, double dec_deg,
                                     double out_v[3]) {
    const double dec_r = dec_deg * DEG2RAD;
    const double ra_r  = ra_deg  * DEG2RAD;
    const double cos_d = std::cos(dec_r);
    out_v[0] = cos_d * std::cos(ra_r);  // x
    out_v[1] = cos_d * std::sin(ra_r);  // y
    out_v[2] = std::sin(dec_r);          // z
}

// ============================================================================
// 球面大圆弧角 (弧度)
// γ = arccos(v1 · v2)  (3D 向量点积)
// 数值稳定: clamp dot 到 [-1, 1]
// ============================================================================
double SphericalSpline::greatCircleDistanceRad(double ra1_deg, double dec1_deg,
                                                double ra2_deg, double dec2_deg) {
    double v1[3], v2[3];
    toUnitVector(ra1_deg, dec1_deg, v1);
    toUnitVector(ra2_deg, dec2_deg, v2);

    double dot = v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
    if (dot >  1.0) dot =  1.0;
    if (dot < -1.0) dot = -1.0;
    return std::acos(dot);
}

// ============================================================================
// 球面双调和 Green 函数 (样条核)
//   K(γ) = (1/(4π)) · [1 - 2·log(sin(γ/2))]   for γ > gamma_eps
//   K(0) = 0  (对角线置零)
// ============================================================================
double SphericalSpline::kernelValue(double gamma_rad, double gamma_eps) {
    if (gamma_rad < gamma_eps) return 0.0;
    const double sin_half = std::sin(gamma_rad * 0.5);
    if (sin_half < 1e-15) return 0.0;  // 数值保护
    return (1.0 / (4.0 * M_PI)) * (1.0 - 2.0 * std::log(sin_half));
}

// ============================================================================
// 3D 笛卡尔零空间 [1, x, y, z]
//   P[0] = 1
//   P[1] = x = cos(dec)·cos(ra)
//   P[2] = y = cos(dec)·sin(ra)
//   P[3] = z = sin(dec)
// ============================================================================
void SphericalSpline::cartesianBasis(double ra_deg, double dec_deg,
                                       double out_P[4]) {
    double v[3];
    toUnitVector(ra_deg, dec_deg, v);
    out_P[0] = 1.0;
    out_P[1] = v[0];  // x
    out_P[2] = v[1];  // y
    out_P[3] = v[2];  // z
}

// ============================================================================
// fit: 拟合 3D 嵌入球面样条模型
// ============================================================================
int SphericalSpline::fit(const double* ra_deg,
                          const double* dec_deg,
                          const double* z,
                          const double* weights,
                          int n,
                          const SplineParams& params,
                          SplineModel& out_model) {
    // 重置输出
    out_model.valid = false;
    out_model.weights.clear();
    out_model.ctrl_ra_deg.clear();
    out_model.ctrl_dec_deg.clear();
    out_model.v[0] = out_model.v[1] = out_model.v[2] = out_model.v[3] = 0.0;
    out_model.fit_rms = 0.0;
    out_model.lambda_used = 0.0;

    // 输入校验
    if (n <= 0 || !ra_deg || !dec_deg || !z) {
        error_msg_ = "fit: empty input (n<=0 or null pointer)";
        return 1;
    }
    if (n < 5) {
        error_msg_ = "fit: n<5 (need at least 5 points for 4 null-space + 1 kernel)";
        return 2;
    }

    const int N = n + 4;  // 系统 (n+4)×(n+4)

    try {
        // 构建稠密矩阵 A 和右端 b
        // A = [K + λW^{-1}   P]
        //     [P'             0]
        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(N, N);
        Eigen::VectorXd b = Eigen::VectorXd::Zero(N);

        for (int i = 0; i < n; ++i) {
            const double ra_i  = ra_deg[i];
            const double dec_i = dec_deg[i];

            // 核矩阵 K 第 i 行
            for (int j = 0; j < n; ++j) {
                const double g = greatCircleDistanceRad(ra_i, dec_i,
                                                        ra_deg[j], dec_deg[j]);
                A(i, j) = kernelValue(g, params.gamma_eps);
            }

            // λ W^{-1} 加到对角线 (加权样条)
            // 高 SNR² (w_i 大) → 正则项小 → 精确拟合
            // 低 SNR² (w_i 小) → 正则项大 → 强制平滑
            double w_i = 1.0;
            if (weights && weights[i] > 0.0) {
                w_i = weights[i];
            }
            A(i, i) += params.lambda / w_i;

            // 设计矩阵 P (n×4), 同时填 P' (4×n)
            double P[4];
            cartesianBasis(ra_i, dec_i, P);
            for (int k = 0; k < 4; ++k) {
                A(i, n + k) = P[k];
                A(n + k, i) = P[k];
            }

            // 右端 z
            b(i) = z[i];
        }
        // 下右块 (4×4) = 0, 右端下 4 行 = 0 (已置零)

        // 求解 A x = b (稠密 PartialPivLU, 适合 N~500)
        Eigen::PartialPivLU<Eigen::MatrixXd> lu(A);
        Eigen::VectorXd x = lu.solve(b);

        // 求解质量检查
        const Eigen::VectorXd residual = A * x - b;
        const double res_norm = residual.norm();
        const double b_norm   = b.norm();
        if (!std::isfinite(res_norm) ||
            (b_norm > 0 && res_norm > 1e-6 * b_norm)) {
            error_msg_ = "fit: Eigen solve failed (singular or ill-conditioned)";
            return 3;
        }

        // 填充输出模型
        out_model.ctrl_ra_deg.assign(ra_deg, ra_deg + n);
        out_model.ctrl_dec_deg.assign(dec_deg, dec_deg + n);
        out_model.weights.resize(n);
        for (int i = 0; i < n; ++i) {
            out_model.weights[i] = x(i);
        }
        for (int k = 0; k < 4; ++k) {
            out_model.v[k] = x(n + k);
        }
        out_model.lambda_used = params.lambda;
        out_model.valid = true;

        // 计算拟合残差 RMS (诊断)
        out_model.fit_rms = residualRms(out_model, ra_deg, dec_deg, z, n);
        return 0;

    } catch (const std::bad_alloc&) {
        error_msg_ = "fit: memory allocation failed";
        return 4;
    } catch (const std::exception& e) {
        error_msg_ = std::string("fit: exception: ") + e.what();
        return 3;
    }
}

// ============================================================================
// evaluate: 评估单点 g(p)
// g(p) = Σ_k w_k · K(γ(p, p_k)) + c0 + c1·x + c2·y + c3·z
// ============================================================================
double SphericalSpline::evaluate(const SplineModel& model,
                                   double ra_deg, double dec_deg) const {
    if (!model.valid || model.weights.empty()) return 0.0;

    const int n = (int)model.weights.size();
    double result = 0.0;

    // 核函数项
    for (int k = 0; k < n; ++k) {
        const double g = greatCircleDistanceRad(ra_deg, dec_deg,
                                                 model.ctrl_ra_deg[k],
                                                 model.ctrl_dec_deg[k]);
        result += model.weights[k] * kernelValue(g, 1e-10);
    }

    // 3D 笛卡尔线性项
    double P[4];
    cartesianBasis(ra_deg, dec_deg, P);
    for (int k = 0; k < 4; ++k) {
        result += model.v[k] * P[k];
    }
    return result;
}

// ============================================================================
// evaluateBatch: 批量评估 (OpenMP 并行)
// ============================================================================
void SphericalSpline::evaluateBatch(const SplineModel& model,
                                      const double* ra_deg,
                                      const double* dec_deg,
                                      int m,
                                      double* out_values) const {
    if (!out_values) return;
    if (!model.valid || model.weights.empty()) {
        for (int i = 0; i < m; ++i) out_values[i] = 0.0;
        return;
    }

    #pragma omp parallel for
    for (int i = 0; i < m; ++i) {
        out_values[i] = evaluate(model, ra_deg[i], dec_deg[i]);
    }
}

// ============================================================================
// residualRms: 拟合点残差 RMS (诊断)
// ============================================================================
double SphericalSpline::residualRms(const SplineModel& model,
                                      const double* ra_deg,
                                      const double* dec_deg,
                                      const double* z,
                                      int n) const {
    if (!model.valid || n <= 0) return 0.0;
    double sum_sq = 0.0;
    for (int i = 0; i < n; ++i) {
        const double g = evaluate(model, ra_deg[i], dec_deg[i]);
        const double r = z[i] - g;
        sum_sq += r * r;
    }
    return std::sqrt(sum_sq / static_cast<double>(n));
}

} // namespace gradient

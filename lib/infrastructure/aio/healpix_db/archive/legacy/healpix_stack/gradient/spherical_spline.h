// spherical_spline.h - 3D 嵌入球面样条拟合与评估
//
// 功能:
//   球面双调和 Green 函数作为核函数, 配合 3D 笛卡尔零空间 [1,x,y,z],
//   通过求解线性系统 [K+λI, P; P', 0][w; v] = [z; 0] 得到稀疏控制点权重,
//   在球面任意点 (ra,dec) 评估 g(p) = Σ w_k·K(γ(p,p_k)) + c0 + c1·x + c2·y + c3·z。
//
// 用途:
//   gradient_fitter (阶段2 差异拟合) 中, 每帧拟合一个 3D 嵌入球面样条模型,
//   表示该帧的梯度校正场 g_i(p)。
//
// 数学模型 (spec §3.4.4):
//   球面点 (ra,dec) → 3D 单位向量:
//     v = (cos(dec)cos(ra), cos(dec)sin(ra), sin(dec))
//     即 x = cos(dec)cos(ra), y = cos(dec)sin(ra), z = sin(dec)
//
//   g(p) = Σ_k w_k · K(γ(p, p_k)) + c0 + c1·x + c2·y + c3·z
//
//   核函数 (球面双调和 Green 函数, Wahba 1981):
//     K(γ) = (1/(4π)) · [1 - 2·log(sin(γ/2))]   (γ > 0)
//     K(0) = 0  (对角线置零, 由 λI 正则项保证稳定)
//   γ(p1,p2) = arccos(v1 · v2)  (大圆弧角, 3D 向量点积)
//
//   3D 笛卡尔零空间 [1, x, y, z] (4 项):
//     始终线性无关, 解决球谐 [Y00,Y10,Y11,Y1-1] 在小 FOV 近似线性相关的病态问题
//     支持全天空 (局部马赛克到全天球马赛克, 不论单帧 FOV)
//
//   线性系统:
//     [K + λI    P ] [w]   [z]
//     [P'        0 ] [v] = [0]
//   K: n×n 核矩阵, P: n×4 笛卡尔设计矩阵, z: diff 样本值, λ: 平滑正则项
//   用 Eigen PartialPivLU 求解 (稠密矩阵, 控制点数 n 典型 100~500)
//
// 依赖: Eigen3 (Dense/LU), C++17
//
// 设计文档: .trae/specs/snr-compact-storage-and-gradient-correction/spec.md §3.4.4

#ifndef SPHERICAL_SPLINE_H
#define SPHERICAL_SPLINE_H

#include <cstdint>
#include <string>
#include <vector>

namespace gradient {

// ============================================================================
// 拟合参数
// ============================================================================
struct SplineParams {
    // λ 正则化参数 (平滑度), 0 = 纯插值, 越大越平滑
    // 用户参数, 默认 0.01 (对应 PMM logSmoothing ≈ -2)
    double lambda = 0.01;

    // K(γ) 的最小距离阈值 (弧度), 防止 sin(γ/2) 过小导致数值发散
    // 默认 1e-10 弧度 ≈ 2e-5 角秒, 远小于实际控制点间距
    double gamma_eps = 1e-10;
};

// ============================================================================
// 拟合结果 (评估时使用)
// ============================================================================
struct SplineModel {
    // 控制点 (ra, dec) 度, 拟合时保存以便评估时计算 γ(p, p_k)
    std::vector<double> ctrl_ra_deg;
    std::vector<double> ctrl_dec_deg;

    // 核权重 w_k (n_points 个)
    std::vector<double> weights;

    // 3D 笛卡尔零空间系数 v = [c0, c1, c2, c3] 对应 [1, x, y, z]
    double v[4] = {0, 0, 0, 0};

    // 拟合时的 λ (记录用于诊断)
    double lambda_used = 0.0;

    // 拟合残差 (RMS, 诊断用)
    double fit_rms = 0.0;

    bool valid = false;  // 拟合是否成功
};

// ============================================================================
// SphericalSpline 类 (3D 嵌入球面样条)
// ============================================================================
class SphericalSpline {
public:
    SphericalSpline();
    ~SphericalSpline();

    // ------------------------------------------------------------------------
    // fit: 拟合 3D 嵌入球面样条模型
    //
    // 输入:
    //   ra_deg, dec_deg: 控制点球面坐标 (度), 长度 n
    //   z:               控制点的目标值 (diff 样本), 长度 n
    //   weights:         控制点权重 (SNR² 加权), 长度 n; nullptr = 等权
    //   n:               控制点数
    //   params:          拟合参数 (λ, gamma_eps)
    // 输出:
    //   out_model: 拟合结果 (含控制点、权重、笛卡尔系数)
    // 返回:
    //   0 = 成功
    //   1 = 输入为空 (n=0 或指针为空)
    //   2 = n < 5 (至少需要 5 点才能拟合 4 个零空间 + 至少 1 个核项)
    //   3 = Eigen 求解失败 (矩阵奇异)
    //   4 = 内存分配失败
    // ------------------------------------------------------------------------
    int fit(const double* ra_deg,
            const double* dec_deg,
            const double* z,
            const double* weights,
            int n,
            const SplineParams& params,
            SplineModel& out_model);

    // ------------------------------------------------------------------------
    // evaluate: 评估单点 g(p)
    //
    // g(p) = Σ_k w_k · K(γ(p, p_k)) + c0 + c1·x + c2·y + c3·z
    // ------------------------------------------------------------------------
    double evaluate(const SplineModel& model,
                    double ra_deg, double dec_deg) const;

    // ------------------------------------------------------------------------
    // evaluateBatch: 批量评估 (OpenMP 并行)
    //
    // 输入:
    //   model:        拟合好的模型
    //   ra_deg, dec_deg: 查询点球面坐标 (度), 长度 m
    //   m:            查询点数
    // 输出:
    //   out_values: 评估值数组 (调用方分配, 长度 m)
    // ------------------------------------------------------------------------
    void evaluateBatch(const SplineModel& model,
                       const double* ra_deg,
                       const double* dec_deg,
                       int m,
                       double* out_values) const;

    // ------------------------------------------------------------------------
    // residualRms: 计算模型在拟合点上的残差 RMS (诊断用)
    // ------------------------------------------------------------------------
    double residualRms(const SplineModel& model,
                       const double* ra_deg,
                       const double* dec_deg,
                       const double* z,
                       int n) const;

    const std::string& lastError() const { return error_msg_; }

private:
    std::string error_msg_;

    // 内部: 球面点 (ra,dec) → 3D 单位向量 v=(x,y,z)
    static void toUnitVector(double ra_deg, double dec_deg,
                              double out_v[3]);

    // 内部: 球面大圆弧角 (弧度), γ = arccos(v1 · v2)
    static double greatCircleDistanceRad(double ra1_deg, double dec1_deg,
                                          double ra2_deg, double dec2_deg);

    // 内部: 球面双调和 Green 函数 K(γ)
    static double kernelValue(double gamma_rad, double gamma_eps);

    // 内部: 3D 笛卡尔零空间 [1, x, y, z]
    static void cartesianBasis(double ra_deg, double dec_deg,
                                double out_P[4]);
};

} // namespace gradient

#endif // SPHERICAL_SPLINE_H

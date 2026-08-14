// gradient_fitter.h - 阶段2: 差异拟合 3D 嵌入球面样条梯度校正场
//
// 功能:
//   输入 gradient_sampler 的样本表 (每帧控制点 bg_median + snr),
//   通过 PMM 风格差异拟合 (diff_i = bg_i - ref) 为每帧拟合一个 3D 嵌入球面样条模型 g_i(p),
//   表示该帧的梯度校正场。
//
// 算法 (spec §3.4 差异拟合, 无迭代):
//   1. 参考场计算 (SNR²加权):
//      ref(p) = Σ_{j covers p} w_j(p) · bg_j(p) / Σ_{j covers p} w_j(p)
//      w_j(p) = SNR_j(p)²
//
//   2. 差异计算 + sigma-clip 过滤:
//      diff_i(p) = bg_i(p) - ref(p)
//      med = median(diff_i), mad = median(|diff_i - med|)
//      threshold = max(5 × 1.4826 × mad, floor_threshold)
//      保留 |diff_i(p) - med| < threshold 的控制点
//
//   3. 3D 嵌入球面样条拟合 (一次性, 无 Gauss-Seidel 迭代):
//      g_i = spherical_spline.fit(ctrl_points_i, diff_i, weights=snr²_i, lambda)
//
//   4. gauge fixing: 加权平均归零
//      g_i.v[0] -= Σ_i Σ_p w_i·g_i(p) / Σ_i Σ_p w_i
//
// 暗环避免四重机制 (PMM 风格, spec §3.4.1):
//   1. 差异拟合 (核心): 亮天体光度在 diff 中抵消
//   2. 星掩蔽: 采样阶段已处理 (Gaia 点源拒绝)
//   3. 平滑参数 λ: 3D 嵌入球面样条低通滤波
//   4. sigma-clip 异常值过滤: 剔除未完全抵消的亮天体边缘
//
// 非重叠帧处理:
//   帧无任何 overlap → ref(p) 无定义 → g_i = 0 (不校正)
//   部分重叠: 重叠区正常拟合, 非重叠区样条外推
//
// 依赖:
//   - spherical_spline (3D 嵌入球面样条拟合)
//   - gradient_sampler (SampleRow 结构体)
//
// 设计文档: .trae/specs/snr-compact-storage-and-gradient-correction/spec.md §3.4

#ifndef GRADIENT_FITTER_H
#define GRADIENT_FITTER_H

#include <cstdint>
#include <string>
#include <vector>

#include "spherical_spline.h"
#include "gradient_sampler.h"  // SampleRow

namespace gradient {

// ============================================================================
// 拟合参数
// ============================================================================
struct FitterParams {
    // 3D 嵌入球面样条正则化参数 (用户参数, 传给 spherical_spline)
    // 默认 0.01 (对应 PMM logSmoothing ≈ -2)
    double lambda = 0.01;

    // sigma-clip 异常值过滤阈值 (spec §3.4.3)
    // threshold = max(sigma_clip_factor × 1.4826 × MAD, floor_threshold)
    double sigma_clip_factor = 5.0;

    // sigma-clip 地板阈值 (避免 MAD 过小时全部保留)
    // 典型 0.001 × 典型背景值
    double sigma_clip_floor = 0.0;

    // 帧间控制点匹配阈值 (角秒)
    // 两帧的控制点 p_i, p_j 距离 < 此值时, 认为 j 覆盖 p_i
    // 典型 300" (5 角分), 覆盖 nside=64~8192 的情况
    double match_threshold_arcsec = 300.0;

    // 是否启用 gauge fixing (加权平均归零)
    bool   enable_gauge_fixing = true;
};

// ============================================================================
// 拟合结果
// ============================================================================
struct FitterResult {
    // 每帧一个 SplineModel, 索引对应 frame_ids
    std::vector<SplineModel> models;

    // 帧索引列表 (与 models 对应)
    std::vector<int32_t> frame_ids;

    // 每帧 sigma-clip 剔除的控制点数 (诊断)
    std::vector<int> n_clipped_per_frame;

    // 每帧拟合残差 RMS (诊断)
    std::vector<double> fit_rms_per_frame;

    // 是否成功
    bool   success = false;
};

// ============================================================================
// GradientFitter 类
// ============================================================================
class GradientFitter {
public:
    GradientFitter();
    ~GradientFitter();

    // ------------------------------------------------------------------------
    // fit: 差异拟合 (一次性, 无迭代)
    //
    // 输入:
    //   rows:     样本表 (来自 gradient_sampler)
    //   n_rows:   样本数
    //   frame_ids: 帧索引列表 (去重, 排序)
    //   n_frames: 帧数
    //   params:   拟合参数
    // 输出:
    //   out: 拟合结果 (含每帧 SplineModel)
    // 返回:
    //   0 = 成功
    //   1 = 输入为空 (n_rows=0 或 n_frames=0)
    //   2 = 无有效帧 (frame_ids 不匹配 rows)
    //   3 = 样条拟合失败 (所有帧都失败)
    // ------------------------------------------------------------------------
    int fit(const SampleRow* rows, int n_rows,
            const int32_t* frame_ids, int n_frames,
            const FitterParams& params,
            FitterResult& out);

    const std::string& lastError() const { return error_msg_; }

private:
    std::string error_msg_;
    SphericalSpline spline_;  // 3D 嵌入球面样条求解器实例

    // 内部: 球面大圆弧角 (弧度)
    static double greatCircleDistanceRad(double ra1_deg, double dec1_deg,
                                          double ra2_deg, double dec2_deg);

    // 内部: 计算中位数
    static double median(std::vector<double>& vals);

    // 内部: 计算 MAD (median absolute deviation)
    static double mad(const std::vector<double>& vals, double med);
};

} // namespace gradient

#endif // GRADIENT_FITTER_H

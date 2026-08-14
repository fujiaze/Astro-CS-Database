// corrected_stacker.h - 阶段3: 梯度校正叠加
//
// 功能:
//   输入已读取的帧数据 (ipix + pixel + snr) 和 FitterResult 的 SplineModel 数组,
//   对每帧每个像素做梯度校正 (pixel - g_i(ra,dec)),
//   再用 SNR² 加权 sigma-clip 累加, 输出叠加结果。
//
// 算法 (spec §3.5):
//   for each frame i:
//     for each ipix:
//       (ra, dec) = pix2ang(ipix)
//       corrected_pixel = pixel_value - g_i(ra, dec)
//       → SNR² 加权 sigma-clip 累加
//   输出 mean = Σ(SNR²·corrected) / Σ(SNR²)
//
// 职责:
//   - 纯算法模块, 不涉及 .hiss/.hcsd I/O
//   - 输入: FrameData (已读取) + SplineModel (已拟合)
//   - 输出: StackResult (ipix + mean + count + weight)
//   - .hiss 读取和 .hcsd 写入由上层 hp_stack_api 处理
//
// 依赖:
//   - healpix_core (pix2ang)
//   - spherical_spline (evaluate g_i, 3D 嵌入球面样条)
//
// 设计文档: .trae/specs/snr-compact-storage-and-gradient-correction/spec.md §3.5

#ifndef CORRECTED_STACKER_H
#define CORRECTED_STACKER_H

#include <cstdint>
#include <string>
#include <vector>

#include "spherical_spline.h"

namespace gradient {

// ============================================================================
// 输入: 已读取的帧数据
// ============================================================================
struct FrameData {
    std::vector<int64_t> ipix;     // HEALPix 像素索引 (NESTED, nside 与输出一致)
    std::vector<float>   pixel;    // 像素值
    std::vector<float>   snr;      // SNR-B (空数组 = 等权, 权重=1.0)
    int32_t              frame_id; // 帧索引 (对应 SplineModel 数组索引)
};

// ============================================================================
// 输出: 叠加结果
// ============================================================================
struct StackResult {
    std::vector<int64_t> ipix;     // 全局唯一 ipix (排序)
    std::vector<double>  mean;     // 加权平均 (corrected)
    std::vector<double>  count;    // 每像素帧数
    std::vector<double>  weight;   // 总权重 Σ(SNR²)
    int                  nside;    // 输出 nside
    bool                 nested;   // 输出排列
};

// ============================================================================
// 叠加参数
// ============================================================================
struct CorrectedStackParams {
    // sigma-clip 阈值 (通常 3.0)
    double sigma = 3.0;

    // sigma-clip 最大迭代次数 (通常 5)
    int    max_iter = 5;

    // Winsorized sigma clip 参数 (GAP-017 新增)
    // use_winsorized=false (默认) 保持向后兼容, 执行普通 sigma-clip
    // use_winsorized=true  执行 Winsorized sigma clip (更稳健, 抗异常值)
    bool   use_winsorized = false;
    double winsorize_low_pct = 0.05;   // 缩尾下分位数 (默认 5%)
    double winsorize_high_pct = 0.95;  // 缩尾上分位数 (默认 95%)

    // 输出 nside 和 nested (应与输入帧一致)
    int    nside = 0;       // 0 = 自动从第一帧推断
    bool   nested = true;
};

// ============================================================================
// CorrectedStacker 类
// ============================================================================
class CorrectedStacker {
public:
    CorrectedStacker();
    ~CorrectedStacker();

    // ------------------------------------------------------------------------
    // stack: 梯度校正 + SNR² 加权 sigma-clip 叠加
    //
    // 输入:
    //   frames:     帧数据数组 (ipix + pixel + snr)
    //   n_frames:   帧数
    //   models:     SplineModel 数组 (每帧一个 g_i), 索引对应 frames[i].frame_id
    //   n_models:   模型数 (应 = 最大 frame_id + 1, 或 = n_frames)
    //   nside:      HEALPix nside (用于 pix2ang)
    //   nested:     HEALPix 排列 (true=NESTED)
    //   params:     叠加参数
    // 输出:
    //   out: 叠加结果
    // 返回:
    //   0 = 成功
    //   1 = 输入为空
    //   2 = nside 无效
    //   3 = 帧数据为空
    // ------------------------------------------------------------------------
    int stack(const FrameData* frames, int n_frames,
              const SplineModel* models, int n_models,
              int nside, bool nested,
              const CorrectedStackParams& params,
              StackResult& out);

    const std::string& lastError() const { return error_msg_; }

private:
    std::string error_msg_;
    SphericalSpline spline_;  // 3D 嵌入球面样条评估器

    // pix2ang: ipix → (ra_deg, dec_deg)
    static void pix2radec(int64_t ipix, int nside, bool nested,
                          double* ra_deg, double* dec_deg);
};

} // namespace gradient

#endif // CORRECTED_STACKER_H

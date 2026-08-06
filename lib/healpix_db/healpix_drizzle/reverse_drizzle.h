// ============================================================================
// reverse_drizzle.h - Sphere -> Plane 球面面积 Drizzle (签字修正 REV-101..107)
//
// 冻结语义 (wiki/Reverse_Drizzle.md):
//   - 每个 HEALPix source leaf 构造球面 footprint (HEALPix 边界自适应细分);
//   - pixfrac 收缩沿球面 slerp 向像素中心收缩;
//   - 目标平面像素通过 WCS/SIP pixelToSky 映射得到球面 footprint
//     (自适应边细分, 消除投影曲率误差);
//   - 重叠面积 = drop ∩ 目标像素球面 footprint 的球面面积 (Girard/S-H),
//     禁止用投影平面 2D 面积作为权重;
//   - signal 按球面面积比例分配; support 以"覆盖在 leaf 内均匀分布"假设
//     参与 coverage 输出 (文档化近似);
//   - FP32 输入→FP32 输出使用真实 float 累计器 (非 double 伪装);
//   - 输入契约严格验证, 非法参数硬失败, 不得静默裁剪。
// ============================================================================
#ifndef REVERSE_DRIZZLE_H
#define REVERSE_DRIZZLE_H

#include "fits_reader.h"
#include <cstdint>
#include <vector>
#include <string>

namespace drizzle {

// 反向 Drizzle 输入
struct ReverseDrizzleInput {
    uint32_t nside = 0;              // HEALPix NSIDE (2 的幂, 1..2^22)
    bool nested = true;              // 仅支持 NESTED
    // source leaf 稀疏数据: leaf_signal_f32 与 leaf_signal 二选一 (严格)
    std::vector<uint64_t> leaf_ipix;       // HEALPix NESTED ipix
    std::vector<double> leaf_signal;       // signal (FP64, 与 f32 二选一)
    std::vector<float>  leaf_signal_f32;   // signal (FP32, 与 f64 二选一)
    std::vector<double> leaf_support;      // 覆盖比例 [0,1] (可选, 空=1.0)
    // target 平面
    WcsParams wcs;                   // target WCS/SIP (has_wcs 必须 true)
    int target_width = 0;
    int target_height = 0;
    double pixfrac = 1.0;            // source leaf 球面收缩 (0, 1]
    bool output_fp64 = false;        // true=FP64 输出, false=FP32 输出
    bool no_data_as_zero = true;     // 无覆盖像素输出 0 (true) 或 NaN (false)
};

// 反向 Drizzle 输出 (统计字段为签字门必填)
struct ReverseDrizzleOutput {
    std::vector<double> signal;      // width*height (FP64 主视图, 恒填充)
    std::vector<double> coverage;    // width*height, 覆盖比例 [0,1]
    std::vector<float>  signal_f32;  // FP32 输出 (恒填充)
    std::vector<float>  coverage_f32;
    // 统计
    int64_t n_source_leaf = 0;           // 有效 source leaf 数
    int64_t n_target_pixel_touched = 0;  // 有 overlap 的目标像素数
    int64_t n_candidates = 0;            // leaf-像素候选对数
    int64_t n_overlaps = 0;              // overlap>0 的对数
    double  total_signal_in = 0.0;       // Σ leaf signal
    double  total_signal_out = 0.0;      // Σ 输出 signal (图像内)
    double  total_covered_area_in = 0.0; // Σ leaf 收缩面积 × support (球面度)
    double  total_covered_area_out = 0.0;// Σ overlap × support (球面度)
    int64_t n_invalid_ipix = 0;          // 越界/重复 ipix 计数
    int64_t n_nonfinite = 0;             // 非有限 signal 计数
    int64_t n_skipped_outside = 0;       // 完全在图像外被跳过
};

class ReverseDrizzle {
public:
    // 执行 Sphere -> Plane 球面面积 Drizzle
    // 失败 (非法输入) 返回 false 并填充 error_msg; 成功返回 true。
    bool run(const ReverseDrizzleInput& in, ReverseDrizzleOutput& out,
             std::string& error_msg);
};

} // namespace drizzle

#endif // REVERSE_DRIZZLE_H

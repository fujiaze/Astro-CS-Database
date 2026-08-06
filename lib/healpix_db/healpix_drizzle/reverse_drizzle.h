// ============================================================================
// reverse_drizzle.h - Sphere -> Plane 真面积 Drizzle (R13 REV-001)
//
// 与 Plane -> Sphere 对称: 对每个 HEALPix source leaf, 构造球面 footprint,
// 经 WCS skyToPixel 投影到目标平面, 用 2D Sutherland-Hodgman 计算与平面
// 像素的面积重叠, 按面积比例分配 signal, 输出平面 signal 与 coverage。
//
// 禁止: 最近邻 / 双线性 / Lanczos / 只采样 leaf 中心 (REVERSE_DRIZZLE_CONTRACT)
// ============================================================================
#ifndef REVERSE_DRIZZLE_H
#define REVERSE_DRIZZLE_H

#include "fits_reader.h"
#include <cstdint>
#include <vector>
#include <string>

namespace drizzle {

// 反向 Drizzle 输入 (REVERSE_DRIZZLE_CONTRACT 底层输入)
struct ReverseDrizzleInput {
    uint32_t nside = 0;              // HEALPix NSIDE
    bool nested = true;              // NESTED ordering
    // source leaf 稀疏数据 (三种 dtype 支持: 提供其一)
    std::vector<uint64_t> leaf_ipix;       // HEALPix NESTED ipix
    std::vector<double> leaf_signal;       // signal (FP64)
    std::vector<float>  leaf_signal_f32;   // signal (FP32, 若提供)
    std::vector<double> leaf_support;      // covered_area 或 support (可选)
    // target 平面
    WcsParams wcs;                   // target WCS/SIP
    int target_width = 0;
    int target_height = 0;
    double pixfrac = 0.8;            // source leaf 收缩 (默认 0.8)
    bool output_fp64 = false;        // 输出 dtype: true=FP64, false=FP32
    bool no_data_as_zero = true;     // mask/no-data: 无覆盖像素输出 0 (true) 或 NaN (false)
};

// 反向 Drizzle 输出
struct ReverseDrizzleOutput {
    std::vector<double> signal;      // width*height (FP64 主)
    std::vector<double> coverage;    // width*height, 覆盖面积 (像素单位², 0..1)
    std::vector<float>  signal_f32;  // FP32 输出 (output_fp64=false 时有效)
    std::vector<float>  coverage_f32;
    // 统计
    int64_t n_source_leaf = 0;
    int64_t n_target_pixel = 0;
    int64_t n_candidates = 0;       // leaf-像素候选对
    int64_t n_overlaps = 0;         // overlap>0 的对
    double total_signal_in = 0.0;
    double total_signal_out = 0.0;
    double total_coverage = 0.0;
};

class ReverseDrizzle {
public:
    // 执行 Sphere -> Plane 面积 Drizzle
    bool run(const ReverseDrizzleInput& in, ReverseDrizzleOutput& out,
             std::string& error_msg);
};

} // namespace drizzle

#endif // REVERSE_DRIZZLE_H

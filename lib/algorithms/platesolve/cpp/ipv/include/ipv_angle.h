#ifndef IPV_ANGLE_H
#define IPV_ANGLE_H

// ============================================================================
// ipv_angle.h - IPV 角度循环验证模块 ( CDA §5.3)
//
// 设计参考: ipv_cda_distortion_design.md §5.3 角度循环顺序验证
//
// 动机:
// 即使距离特征在去畸变后恢复准确, 在密集星场 (N_W=150-300) 中,
// 距离匹配仍可能产生歧义。角度提供与径向畸变完全正交的验证信号
// (径向畸变只改变径向距离, 不改变方位角)。
//
// 算法:
// 1. 对每个已匹配邻星 k, 计算图像侧方位角 φₖ 与星表侧方位角 Φₖ
// 2. 旋转角差 Δθₖ = Φₖ - φₖ (归一化到 [-π, π))
// 3. 180° 周期循环统计 (旋转 180° 在镜像模式下等价):
// R = |Σ exp(i×2Δθₖ)| / m, circular_std = sqrt(-2×ln(R))
// 4. 离群点剔除 (3σ 准则)
// 5. consistency = max(0, 1 - circular_std_deg / angle_tol_deg)
//
// 日期: 2026-07-04
// ============================================================================

#include <vector>
#include <utility>
#include "ipv_types.h"

namespace ipv {

// 角度循环验证: 返回 [0,1] 一致性分数
// 1.0 = 完全一致 (circular_std=0)
// 0.0 = 完全不一致 (circular_std >= angle_tol_deg)
// 0.5 = 中性 (邻星数 < 3 无法判断)
//
// 输入:
// pivot_img, pivot_cat: pivot 星的图像侧/星表侧坐标
// U, W: 图像侧/星表侧星点数组 ( 像素坐标)
// hex: pivot 的六边形描述符 (含 neighbor_idx[5])
// matched_neighbors: 已匹配的邻星对 (u_neighbor_idx, w_neighbor_idx)
// angle_tol_deg: 角度容差 (默认 5.0 度)
double angle_cyclic_verify(
    const StarPoint& pivot_img,
    const StarPoint& pivot_cat,
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const HexDescriptor& hex,
    const std::vector<std::pair<int,int>>& matched_neighbors,
    double angle_tol_deg = 5.0
);

} // namespace ipv

#endif // IPV_ANGLE_H

#ifndef IPV_KVECTOR_H
#define IPV_KVECTOR_H

// ============================================================================
// ipv_kvector.h - IPV k-vector 索引模块
//
// 在 Gaia 侧星点集 W 上构建 k-vector 索引 (排序的星对距离表),
// 支持 O(log M + k) 时间查询角距在 [d_lo, d_hi] 范围内的所有星对。
//
// 提取自 V4.5 vm45_relvec.cpp 的 k-vector 构建/查询逻辑,
// 解耦为独立通用模块, 供 IPV 各阶段复用。
//
// 坐标系: 角秒坐标 (TAN 投影小区域, 欧氏距离近似足够)
// 复杂度: 构建 O(M log M), 查询 O(log M + k)
//   M = N_W*(N_W-1)/2, k = 匹配星对数
//
// 日期: 2026-07-02
// ============================================================================

#include <vector>
#include <utility>
#include <cstdint>
#include "ipv_types.h"

namespace ipv {

// k-vector 索引结构
// 在星点集 W 上构建, 支持 O(log M + k) 查询角距在 [d_lo, d_hi] 内的星对
struct KVectorIndex {
    std::vector<double> distances;          // 排序后的星对距离 (角秒)
    std::vector<std::pair<int,int>> pairs;  // 对应的 (a_idx, b_idx) 星对索引
    int    n_stars = 0;                     // 星点数 N_W
    size_t n_pairs = 0;                     // 星对数 M = N_W*(N_W-1)/2
    double d_min = 0.0;                     // 最小距离
    double d_max = 0.0;                     // 最大距离
    bool   built = false;                   // 是否已构建
};

// 构建 k-vector 索引
// 输入: W (StarPoint 数组, 角秒坐标)
// 输出: KVectorIndex
// 复杂度: O(M log M), M = N_W*(N_W-1)/2
KVectorIndex kvector_build(const std::vector<StarPoint>& W);

// 查询角距在 [d_lo, d_hi] 内的星对
// 输入: KVectorIndex, d_lo, d_hi (角秒)
// 输出: 星对索引列表 (a_idx, b_idx)
// 复杂度: O(log M + k), k 为匹配星对数
std::vector<std::pair<int,int>> kvector_query(
    const KVectorIndex& kv,
    double d_lo,
    double d_hi
);

} // namespace ipv

#endif // IPV_KVECTOR_H

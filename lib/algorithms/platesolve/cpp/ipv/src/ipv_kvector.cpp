// ============================================================================
// ipv_kvector.cpp - IPV k-vector 索引模块实现
//
// 提取自 vm45_relvec.cpp 的 k-vector 构建/查询逻辑:
// - 构建: 计算所有 a<b 星对的欧氏距离, 按距离升序排序
// - 查询: 二分查找 [d_lo, d_hi] 区间, 返回所有匹配星对
//
// 与 原实现的差异:
// 1. 解耦为独立函数 ( 中是 RelativeVectorMatcher 类的成员方法)
// 2. 数据结构: distances 和 pairs 分两个并行数组
// ( 用 GaiaPair{dist, a, b} 单数组, 这里按头文件设计要求拆分)
// 3. 不再构建 N_w×N_w 距离矩阵 ( 中预计算 D_W_ 供第三星验证复用,
// 本模块仅做 k-vector, 直接计算星对距离)
// 4. 查询接口返回 pair 列表 ( 内部返回索引区间 [idx_lo, idx_hi))
//
// 日期: 2026-07-02
// ============================================================================

#include "ipv_kvector.h"

#include <cmath>
#include <algorithm>

namespace ipv {

// ----------------------------------------------------------------------------
// kvector_build: 构建 k-vector 索引
// ----------------------------------------------------------------------------
KVectorIndex kvector_build(const std::vector<StarPoint>& W)
{
    KVectorIndex kv;
    kv.n_stars = (int)W.size();

    // 边界: 星点少于 2 颗时无法构成星对
    if (kv.n_stars < 2) {
        kv.built = true;   // 标记为已构建 (但 n_pairs=0)
        return kv;
    }

    // 星对数 M = N_W*(N_W-1)/2
    kv.n_pairs = (size_t)kv.n_stars * (kv.n_stars - 1) / 2;

    // 构建三元组数组 (distance, a_idx, b_idx), a < b
    struct Triple { double dist; int a; int b; };
    std::vector<Triple> triples;
    triples.reserve(kv.n_pairs);

    for (int a = 0; a < kv.n_stars; ++a) {
        for (int b = a + 1; b < kv.n_stars; ++b) {
            // 欧氏距离 (角秒坐标, TAN 投影小区域内足够精确)
            double dx = W[a].x - W[b].x;
            double dy = W[a].y - W[b].y;
            double d  = std::sqrt(dx * dx + dy * dy);
            triples.push_back({d, a, b});
        }
    }

    // 按距离升序排序
    std::sort(triples.begin(), triples.end(),
              [](const Triple& p, const Triple& q) {
                  return p.dist < q.dist;
              });

    // 拆分到 distances 和 pairs 两个并行数组
    kv.distances.reserve(kv.n_pairs);
    kv.pairs.reserve(kv.n_pairs);
    for (const auto& t : triples) {
        kv.distances.push_back(t.dist);
        kv.pairs.push_back({t.a, t.b});
    }

    // 记录 d_min, d_max
    kv.d_min = kv.distances.front();
    kv.d_max = kv.distances.back();
    kv.built = true;

    return kv;
}

// ----------------------------------------------------------------------------
// kvector_query: 查询角距在 [d_lo, d_hi] 内的星对
// ----------------------------------------------------------------------------
std::vector<std::pair<int,int>> kvector_query(
    const KVectorIndex& kv,
    double d_lo,
    double d_hi)
{
    std::vector<std::pair<int,int>> result;

    // 边界: 索引未构建或为空
    if (!kv.built || kv.n_pairs == 0) {
        return result;
    }

    // 边界: 区间无效 (d_lo > d_hi) → 返回空
    if (d_lo > d_hi) {
        return result;
    }

    // 边界: d_hi < d_min 或 d_lo > d_max → 无匹配
    if (d_hi < kv.d_min || d_lo > kv.d_max) {
        return result;
    }

    // 二分查找:
    // lo_it = lower_bound(distances, d_lo) → 第一个 >= d_lo 的位置
    // hi_it = upper_bound(distances, d_hi) → 第一个 > d_hi 的位置
    // 返回 [lo_it, hi_it) 范围内的所有 pairs
    auto lo_it = std::lower_bound(kv.distances.begin(), kv.distances.end(), d_lo);
    auto hi_it = std::upper_bound(kv.distances.begin(), kv.distances.end(), d_hi);

    size_t idx_lo = (size_t)(lo_it - kv.distances.begin());
    size_t idx_hi = (size_t)(hi_it - kv.distances.begin());

    if (idx_hi <= idx_lo) {
        return result;
    }

    // 复制 [idx_lo, idx_hi) 范围内的 pairs
    result.assign(kv.pairs.begin() + idx_lo, kv.pairs.begin() + idx_hi);
    return result;
}

} // namespace ipv

#ifndef IPV_RANSAC_H
#define IPV_RANSAC_H

// ============================================================================
// ipv_ransac.h - IPV PROSAC 验证模块
//
// 实现从候选匹配中求解最优相似变换:
//   - solve_similarity_transform: 2 对匹配解析求解 (s, θ, tx, ty)
//   - umeyama_estimate: 所有内点 Umeyama SVD 闭合解
//   - prosac_verify: PROSAC 按 vote 降序优先采样, 内点验证, Umeyama 精化
//
// 模型: W = s·R(θ)·U + t  (U=图像侧角秒, W=星表侧角秒)
//   R = [[cos θ, -sin θ], [sin θ, cos θ]]
//
// 日期: 2026-07-02
// ============================================================================

#include <vector>
#include "ipv_types.h"

namespace ipv {

// 从 2 对匹配求解相似变换 (s, θ, tx, ty)
// 使用相对向量法消去平移:
//   s = |ΔW| / |ΔU|  (ΔW = W[w2]-W[w1], ΔU = U[u2]-U[u1])
//   θ = atan2(ΔW.y, ΔW.x) - atan2(ΔU.y, ΔU.x)
//   tx = W[w1].x - s*(cos(θ)*U[u1].x - sin(θ)*U[u1].y)
//   ty = W[w1].y - s*(sin(θ)*U[u1].x + cos(θ)*U[u1].y)
SimTransform solve_similarity_transform(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    int u1, int w1, int u2, int w2
);

// Umeyama SVD 完整估计
// 用所有内点匹配做闭合解最优相似变换
// 参考 V4.4 vm44_fit.cpp 的 Umeyama 实现 (手写 2x2 SVD, 不依赖 Eigen)
SimTransform umeyama_estimate(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& pairs
);

// PROSAC 验证主函数
// 输入: U, W, candidates (按 vote 降序), params
// 输出: PROSACResult (最优变换, 内点列表, RMS, score)
PROSACResult prosac_verify(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<CandidateMatch>& candidates,
    const IPVSolverParams& params
);

// 全量验证: 用最优变换对所有 (u, w) 对做最近邻匹配
// 动机: PROSAC 只验证 candidates 中的 (u,w) 对, 但真实匹配可能不在 candidates 中
//       (vote < threshold)。此函数用 PROSAC 最优变换对所有 U 中星点预测 W 中位置,
//       找最近邻 w, 若距离 < tau 则加入内点。
// 算法: O(N_U × N_W) 暴力最近邻 (N_U, N_W < 500, < 250000 操作, < 1ms)
// 参数:
//   U, W: 图像侧 / 星表侧星点
//   tf: 待验证的相似变换
//   tau: 内点阈值 (角秒)
// 返回: 扩展内点列表 (可能比 PROSAC 内点多)
std::vector<MatchPair> full_verify_transform(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const SimTransform& tf,
    double tau
);

// V4.16 (Task 12): iter_trans 鲁棒拟合 — 作为 PROSAC 失败时的备选路径
//
// 算法:
//   1. 取票数最高的 6 对 (AT_MATCH_STARTN_LINEAR=6) 起始拟合
//   2. 拟合 6 参数线性 TRANS (过约束最小二乘):
//        W.x = a00*U.x + a01*U.y + tx
//        W.y = a10*U.x + a11*U.y + ty
//      用 6 对 12 个方程最小二乘求解 6 个参数 (拆分为两个独立 3x3 正规方程)
//   3. 计算所有候选对的残差
//   4. 取残差 35% 百分位作为有效 sigma
//   5. 剔除残差 > 10*sigma 的星对
//   6. 用剩余星对重新拟合 TRANS
//   7. 重复 5 次或直到收敛 (残差变化 < 10%)
//   8. 用最终 TRANS 做全量匹配 (tau 半径内最近邻)
//   9. 输出 PROSACResult 格式 (将线性 TRANS 转回 SimTransform:
//      s = sqrt(|a00*a11 - a01*a10|), θ = atan2(a10, a00))
//
// 设计决策: 不直接替换 PROSAC, 而是作为 PROSAC 失败时的备选路径,
//          保持现有成功率不退化。
//
// 输入: U, W, candidates (按 vote 降序), params
// 输出: PROSACResult (最优变换, 内点列表, RMS, score)
PROSACResult iter_trans_verify(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<CandidateMatch>& candidates,
    const IPVSolverParams& params
);

} // namespace ipv

#endif // IPV_RANSAC_H

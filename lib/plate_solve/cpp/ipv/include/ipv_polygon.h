#ifndef IPV_POLYGON_H
#define IPV_POLYGON_H

// ============================================================================
// ipv_polygon.h - IPV 多边形匹配模块 (Task 6/7/8)
//
// 实现核心的多边形匹配 pipeline:
//   Task 6: 六边形描述符构建 + 候选收集 + 多边形完整性验证 + 主匹配循环
//   Task 7: 全星对 pairwise 距离对称投票 (geometric voting)
//   Task 8: 共识候选提取 (按票数 + 置信度过滤)
//
// 输出 CandidateMatch 列表供 PROSAC 验证。
//
// 日期: 2026-07-02
// ============================================================================

#include <vector>
#include <set>
#include <utility>
#include "ipv_types.h"
#include "ipv_kvector.h"

namespace ipv {

// ===========================================================================
// Task 6: 六边形描述符匹配
// ===========================================================================

// 构建 pivot 星的六边形描述符
// 取 pivot 在 U 中像素距离最近的 5 颗邻星, 返回 5 距离特征 + 邻星索引
// r_local > 0 时只收集距离 pivot <= r_local 的邻星 (宽 FOV 抗畸变局部化)
// fov_diag > 0 时启用贪心形状剪枝 (Task 3):
//   1) 最小邻星距离 >= 0.02 * fov_diag
//   2) 邻星距离比 <= 3.0 (剔除等边配置)
//   3) 邻星间夹角 >= 15° (剔除共线/过扁)
// fov_diag = 0 时跳过形状剪枝, 保持原最近 5 颗行为 (兼容窄 FOV 旧调用)
// V4.12: use_ratio=true 时输出距离比值 [d_k/d_ref], d_ref=d_1 (抗宽 FOV 畸变)
HexDescriptor build_hex_descriptor(
    const std::vector<StarPoint>& U,   // 图像侧星点 (角秒坐标)
    int pivot_idx,                    // pivot 星索引
    double r_local = 0.0,             // 邻星距离上限 (角秒, 0=不限)
    double fov_diag = 0.0,            // FOV 对角线 (角秒, 0=禁用形状剪枝)
    bool use_ratio = false            // V4.12: true=输出距离比值, false=输出绝对距离
);

// 收集星表候选星
// 对 HexDescriptor 的前 K 个距离 d[k], k-vector 查询 [d[k]-3σ_d, d[k]+3σ_d]
// 收集至少出现在 >=min_occur 个查询中的星表星
// V4.12: use_ratio=true 时需传入 W, 对每个候选 w 用 d_ref_w × ratio 恢复绝对距离查询
std::set<int> collect_candidates(
    const KVectorIndex& kv,
    const HexDescriptor& hex,
    double sigma_d,                   // 距离容差 (角秒)
    int K = 5,                        // 使用前 K 个距离 (降阶: 5/4/3/2)
    int min_occur = 2,                // 最少出现次数 (降阶时放松到 1)
    const std::vector<StarPoint>* W = nullptr,  // V4.12: 距离比模式需要星表侧
    bool use_ratio = false            // V4.12: true=距离比模式
);

// 验证候选星的多边形完整性
// 检查 candidate 的前 K 个邻星是否与 hex 的前 K 个邻星距离匹配
// 返回匹配的 (u_neighbor_idx, w_neighbor_idx) 对列表
// V4.12: use_ratio=true 时用 d_ref_w × ratio 恢复绝对距离查询
std::vector<std::pair<int,int>> verify_polygon(
    const std::vector<StarPoint>& W,  // 星表侧星点 (角秒坐标)
    int candidate_w,                  // 候选星表星索引
    const HexDescriptor& hex,         // 图像侧六边形描述符
    const KVectorIndex& kv,
    double sigma_d,
    int K = 5,                        // 使用前 K 个邻星 (降阶: 5/4/3/2)
    int min_match = 3,                // 最少匹配邻星数 (降阶时放松)
    bool use_ratio = false            // V4.12: true=距离比模式
);

// 多边形匹配主函数
// 遍历最亮 n_pivot 颗 pivot 星, 构建描述符、查询候选、验证、投票
PolygonMatchResult polygon_match(
    const std::vector<StarPoint>& U,  // 图像侧星点 (角秒坐标, 已按 flux 降序)
    const std::vector<StarPoint>& W,  // 星表侧星点 (角秒坐标)
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double sigma_d                    // 距离容差 (角秒)
);

// 自适应降阶多边形匹配 (宽 FOV 专用)
// 六边形(K=5)失败时自动降阶: 五边形(K=4) → 四边形(K=3) → 三角形(K=2)
// 同时启用邻星局部化 (r_local = 0.15 * fov_diag), 限制邻星在 pivot 附近,
// 减小宽 FOV 畸变对距离特征的污染。
// 各阶匹配的 votes 合并累加, 让 PROSAC 有更大采样空间。
// 降阶触发条件: 当前阶 max_vote < 3 且 n_polygon_passed < n_pivot/2
// V4.12: use_ratio=true 时启用距离比描述符 (抗宽 FOV 畸变 fallback)
PolygonMatchResult polygon_match_adaptive(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double sigma_d,
    double fov_diag,                  // FOV 对角线 (角秒)
    bool use_ratio = false            // V4.12: true=距离比模式 fallback
);

// ===========================================================================
// Task 7: 几何投票
// ===========================================================================

// 全星对 pairwise 距离对称投票
// 对图像侧每对星 (i,j), k-vector 查询 W 中角距匹配的星对 (a,b), 对称投票
// tol = 3 * sigma_d (与 polygon_match 一致, 避免引入过多噪声)
void geometric_vote(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double fov_diag,                  // FOV 对角线 (角秒)
    double sigma_d,                   // 距离容差 (角秒, 与 polygon_match 一致)
    VoteMap& votes                    // 输出/累加投票矩阵
);

// ===========================================================================
// Task 8: 共识提取
// ===========================================================================

// 从投票矩阵提取候选匹配列表
// 对每个图像星 i 取最大票数 w_best, 若 max_vote >= vote_threshold 则加入候选
std::vector<CandidateMatch> extract_consensus(
    const VoteMap& votes,
    int N_U,
    const IPVSolverParams& params
);

} // namespace ipv

#endif // IPV_POLYGON_H

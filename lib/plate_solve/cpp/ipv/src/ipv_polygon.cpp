// ============================================================================
// ipv_polygon.cpp - IPV 多边形匹配模块实现 (Task 6/7/8)
//
// 实现:
//   Task 6: build_hex_descriptor / collect_candidates / verify_polygon / polygon_match
//   Task 7: geometric_vote (全星对 pairwise 距离对称投票)
//   Task 8: extract_consensus (按票数 + 置信度过滤候选)
//
// 设计要点:
//   - 距离计算用欧氏距离 (角秒坐标, TAN 投影小区域内足够精确)
//   - 投票矩阵用 VoteMap (unordered_map<VoteKey, int, VoteKeyHash>)
//   - sigma_d 容差统一采用 3σ 区间
//   - 日志: 模块级静态 Logger, 默认输出到 stderr, 可通过 init_polygon_logger 写文件
//
// 日期: 2026-07-02
// ============================================================================

#include "ipv_polygon.h"
#include "ipv_log.h"
#include "ipv_angle.h"  // V4.11: 角度循环验证 (Phase C §5.3)

#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <cstdio>    // std::fprintf (angle bonus 日志)

namespace ipv {

// ---------------------------------------------------------------------------
// 模块级日志器
// ---------------------------------------------------------------------------
// 默认仅输出到 stderr; 外部可调用 init_polygon_logger() 写入文件
static Logger g_polygon_logger;

Logger& polygon_logger() {
    return g_polygon_logger;
}

void init_polygon_logger(const std::string& path) {
    g_polygon_logger.init(path);
}

// ===========================================================================
// Task 6: 六边形描述符匹配
// ===========================================================================

// ----------------------------------------------------------------------------
// build_hex_descriptor: 构建 pivot 星的六边形描述符
//   pivot = U[pivot_idx]
//   计算所有其他星到 pivot 的欧氏距离, 按距离升序排序
//   r_local > 0 时只收集距离 <= r_local 的邻星 (宽 FOV 抗畸变局部化)
//
// Task 3: 几何描述子形状剪枝 (fov_diag > 0 时启用)
//   不再简单取前 5 颗, 改为贪心选星: 按距离升序遍历候选, 依次检查三项约束,
//   满足全部才加入已选列表。剔除等边/共线/过扁/过近噪声等退化几何配置,
//   提升描述子区分度。fov_diag = 0 时跳过剪枝, 保持原最近 5 颗行为。
// ----------------------------------------------------------------------------
HexDescriptor build_hex_descriptor(
    const std::vector<StarPoint>& U,
    int pivot_idx,
    double r_local,
    double fov_diag,
    bool use_ratio)
{
    HexDescriptor hex;
    hex.pivot_idx = pivot_idx;
    // 初始化为非法值, 便于后续检查
    for (int k = 0; k < 5; ++k) {
        hex.distances[k]    = -1.0;
        hex.neighbor_idx[k] = -1;
    }

    const int N = (int)U.size();
    if (pivot_idx < 0 || pivot_idx >= N) {
        return hex;
    }

    const StarPoint& pivot = U[pivot_idx];

    // 收集 (distance, idx) 对, 排除 pivot 自身 (距离>0)
    // r_local > 0 时排除距离过远的星 (宽 FOV 抗畸变)
    std::vector<std::pair<double,int>> dist_list;
    dist_list.reserve(N - 1);
    for (int i = 0; i < N; ++i) {
        if (i == pivot_idx) continue;
        double dx = U[i].x - pivot.x;
        double dy = U[i].y - pivot.y;
        double d  = std::sqrt(dx * dx + dy * dy);
        if (d > 0.0) {
            if (r_local > 0.0 && d > r_local) continue;  // 超出局部范围, 跳过
            dist_list.emplace_back(d, i);
        }
    }

    // 按距离升序排序
    std::sort(dist_list.begin(), dist_list.end(),
              [](const std::pair<double,int>& a, const std::pair<double,int>& b) {
                  return a.first < b.first;
              });

    if (fov_diag > 0.0) {
        // Task 3: 几何描述子形状剪枝 (贪心选星)
        //   1) 最小邻星距离 >= 0.02 * fov_diag (剔除过近噪声)
        //   2) 邻星距离比 <= 3.0 (剔除等边配置, 要求邻星距离有层次)
        //   3) 邻星间夹角 >= 15° (剔除共线/过扁)
        const double min_dist_threshold = 0.02 * fov_diag;
        const double max_dist_ratio     = 3.0;
        const double min_angle_deg      = 15.0;
        const double RAD2DEG            = 180.0 / 3.14159265358979323846;

        std::vector<std::pair<double,int>>    selected;      // (distance, idx)
        std::vector<std::pair<double,double>> selected_vec;  // 方向向量 (ax, ay)
        double min_selected_dist = 0.0;

        for (const auto& cand : dist_list) {
            double dist = cand.first;
            int    idx  = cand.second;

            // 约束 1: 最小邻星距离
            if (dist < min_dist_threshold) continue;

            // 约束 2: 邻星距离比 (当前距离 / 已选最小距离)
            if (!selected.empty()) {
                if (dist / min_selected_dist > max_dist_ratio) continue;
            }

            // 约束 3: 邻星间夹角 (pivot 为顶点, 方向向量夹角)
            double ax = U[idx].x - pivot.x;
            double ay = U[idx].y - pivot.y;
            bool angle_ok = true;
            for (const auto& sv : selected_vec) {
                double bx    = sv.first;
                double by    = sv.second;
                double cross = ax * by - ay * bx;
                double dot   = ax * bx + ay * by;
                double ang   = std::abs(std::atan2(cross, dot)) * RAD2DEG;
                if (ang < min_angle_deg) {
                    angle_ok = false;
                    break;
                }
            }
            if (!angle_ok) continue;

            // 全部约束通过, 加入已选
            selected.push_back(cand);
            selected_vec.emplace_back(ax, ay);
            if (selected.size() == 1) {
                min_selected_dist = dist;
            }
            if ((int)selected.size() >= 5) break;
        }

        // 填充 hex (不足 5 颗时 distances/neighbor_idx 保持 -1)
        int n_take = std::min(5, (int)selected.size());
        // V4.12: use_ratio=true 时存储距离比值 d[k]/d_ref, d_ref=d_1 (抗宽 FOV 畸变)
        double d_ref = 1e-6;
        if (n_take > 0) d_ref = std::max(selected[0].first, 1e-6);
        for (int k = 0; k < n_take; ++k) {
            hex.distances[k]    = use_ratio ? (selected[k].first / d_ref) : selected[k].first;
            hex.neighbor_idx[k] = selected[k].second;
        }
    } else {
        // fov_diag = 0: 原逻辑, 取前 5 颗 (兼容窄 FOV 旧调用)
        int n_take = std::min(5, (int)dist_list.size());
        // V4.12: use_ratio=true 时存储距离比值 d[k]/d_ref, d_ref=d_1
        double d_ref = 1e-6;
        if (n_take > 0) d_ref = std::max(dist_list[0].first, 1e-6);
        for (int k = 0; k < n_take; ++k) {
            hex.distances[k]    = use_ratio ? (dist_list[k].first / d_ref) : dist_list[k].first;
            hex.neighbor_idx[k] = dist_list[k].second;
        }
    }

    return hex;
}

// ----------------------------------------------------------------------------
// collect_candidates: 收集星表候选星
//   对 hex.distances[k] (k=0..K-1) k-vector 查询 [d-3σ, d+3σ]
//   收集出现在 >=min_occur 个查询中的星表星索引
//   K=5 (六边形, 默认), K=4 (五边形), K=3 (四边形), K=2 (三角形)
//
// V4.12: use_ratio=true 时启用距离比模式
//   hex.distances[k] 是比值, 不能直接查询
//   遍历所有 W 中的星 w 作为候选 pivot, 用 d_ref_w × ratio 恢复绝对距离
//   调用 verify_polygon(use_ratio=true) 验证, 通过 min_occur 邻星匹配的 w 加入候选
// ----------------------------------------------------------------------------
std::set<int> collect_candidates(
    const KVectorIndex& kv,
    const HexDescriptor& hex,
    double sigma_d,
    int K,
    int min_occur,
    const std::vector<StarPoint>* W,
    bool use_ratio)
{
    std::set<int> result;

    // V4.12: 距离比模式 - 遍历 W, 用 d_ref_w × ratio 恢复绝对距离验证
    if (use_ratio && W != nullptr) {
        int N_W = (int)W->size();
        for (int w = 0; w < N_W; ++w) {
            auto matched = verify_polygon(*W, w, hex, kv, sigma_d, K, min_occur, true);
            if ((int)matched.size() >= min_occur) {
                result.insert(w);
            }
        }
        return result;
    }

    std::map<int,int> occur;  // 星表星索引 -> 出现次数

    double tol = 3.0 * sigma_d;
    if (K < 1) K = 1;
    if (K > 5) K = 5;

    for (int k = 0; k < K; ++k) {
        if (hex.distances[k] < 0.0) continue;  // 邻星不足时跳过

        double d_lo = hex.distances[k] - tol;
        double d_hi = hex.distances[k] + tol;
        if (d_lo < 0.0) d_lo = 0.0;

        auto pairs = kvector_query(kv, d_lo, d_hi);
        for (const auto& pr : pairs) {
            occur[pr.first]  += 1;
            occur[pr.second] += 1;
        }
    }

    // 出现次数 >= min_occur 的加入候选集
    for (const auto& kv_pair : occur) {
        if (kv_pair.second >= min_occur) {
            result.insert(kv_pair.first);
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
// verify_polygon: 验证候选星的多边形完整性
//   对 hex 的前 K 个邻星, 在星表侧寻找距离候选星 candidate_w 在
//   [hex.distances[k] - 3σ, hex.distances[k] + 3σ] 内的星
//   若 >=min_match 个邻星找到匹配, 返回 (u_neighbor_idx, w_neighbor_idx) 对列表
//   否则返回空列表
//
// 注: 原 V4.1 要求全部 5 邻星匹配, 在密集星场 (如 M20) 容错性不足,
//     单个邻星偏差即导致整个 pivot 失败。放宽到 >=3 邻星匹配,
//     依赖 PROSAC 阶段的尺度约束和 Umeyama 精化过滤错误匹配。
//     V4.7: 支持 K 降阶 (K=4/3/2), min_match 相应放松。
//
// V4.12: use_ratio=true 时启用距离比模式
//   hex.distances[k] 是比值, 用 d_ref_w × ratio 恢复绝对距离查询
//   d_ref_w = candidate_w 在 W 中的最近邻星距离
// ----------------------------------------------------------------------------
std::vector<std::pair<int,int>> verify_polygon(
    const std::vector<StarPoint>& W,
    int candidate_w,
    const HexDescriptor& hex,
    const KVectorIndex& kv,
    double sigma_d,
    int K,
    int min_match,
    bool use_ratio)
{
    std::vector<std::pair<int,int>> matched_pairs;
    if (candidate_w < 0 || candidate_w >= (int)W.size()) {
        return matched_pairs;
    }

    if (K < 1) K = 1;
    if (K > 5) K = 5;

    double tol = 3.0 * sigma_d;
    std::set<int> used_w;  // 已匹配的星表邻星索引 (避免重复分配)

    // 统计可用邻星数 (hex.distances[k] >= 0, k < K)
    int n_valid_neighbors = 0;
    for (int k = 0; k < K; ++k) {
        if (hex.distances[k] >= 0.0) n_valid_neighbors++;
    }
    if (n_valid_neighbors < min_match) {
        // 可用邻星不足, 无法验证
        return matched_pairs;
    }

    // V4.12: 距离比模式 - 计算 candidate_w 的最近邻星距离 d_ref_w
    double d_ref_w = 0.0;
    if (use_ratio) {
        d_ref_w = 1e18;
        const StarPoint& cw = W[candidate_w];
        for (int j = 0; j < (int)W.size(); ++j) {
            if (j == candidate_w) continue;
            double dx = W[j].x - cw.x;
            double dy = W[j].y - cw.y;
            double d = std::sqrt(dx * dx + dy * dy);
            if (d < d_ref_w) d_ref_w = d;
        }
        if (d_ref_w < 1e-6) d_ref_w = 1e-6;  // 保护: 邻星重合
    }

    // 对 hex 的前 K 个邻星, 通过 k-vector 查询距离匹配的星对,
    // 筛选其中包含 candidate_w 的星对, 另一端即为候选邻星
    int n_matched = 0;
    for (int k = 0; k < K; ++k) {
        if (hex.distances[k] < 0.0) continue;  // 该邻星无效, 跳过

        // V4.12: 距离比模式用 d_ref_w × ratio 恢复绝对距离
        double d_query = use_ratio ? (d_ref_w * hex.distances[k]) : hex.distances[k];
        double d_lo = d_query - tol;
        double d_hi = d_query + tol;
        if (d_lo < 0.0) d_lo = 0.0;

        auto pairs = kvector_query(kv, d_lo, d_hi);

        for (const auto& pr : pairs) {
            int w_neighbor = -1;
            if (pr.first == candidate_w)       w_neighbor = pr.second;
            else if (pr.second == candidate_w) w_neighbor = pr.first;
            if (w_neighbor < 0) continue;
            if (used_w.find(w_neighbor) != used_w.end()) continue;  // 已分配
            // 找到匹配
            matched_pairs.emplace_back(hex.neighbor_idx[k], w_neighbor);
            used_w.insert(w_neighbor);
            ++n_matched;
            break;
        }
        // 该邻星无匹配, 继续尝试下一个 (允许部分缺失)
    }

    // 至少 min_match 个邻星匹配才算通过
    if (n_matched < min_match) {
        matched_pairs.clear();
    }
    return matched_pairs;
}

// ----------------------------------------------------------------------------
// polygon_match: 多边形匹配主函数
//   遍历最亮 n_pivot 颗 pivot 星, 构建描述符、查询候选、验证、投票
// ----------------------------------------------------------------------------
PolygonMatchResult polygon_match(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double sigma_d)
{
    PolygonMatchResult result;
    result.n_pivots         = 0;
    result.n_polygon_passed = 0;
    result.max_vote         = 0.0;
    result.success          = false;

    int N_U = (int)U.size();
    if (N_U < 6 || (int)W.size() < 6) {
        g_polygon_logger.warn("polygon_match: 星点数不足 (U or W < 6)");
        return result;
    }

    // pivot 列表: U 已按 flux 降序, 取前 min(n_pivot, N_U) 颗
    int n_pivot = std::min(params.n_pivot, N_U);
    result.n_pivots = n_pivot;

    g_polygon_logger.infof("polygon_match: N_U=%d, N_W=%d, n_pivot=%d, sigma_d=%.3f",
                           N_U, (int)W.size(), n_pivot, sigma_d);

    int n_pivot_with_candidates = 0;
    int n_pivot_with_pass       = 0;

    for (int p = 0; p < n_pivot; ++p) {
        // 1. 构建六边形描述符 (窄 FOV: fov_diag=0.0 禁用形状剪枝, 保持原行为)
        HexDescriptor hex = build_hex_descriptor(U, p, 0.0, 0.0);
        if (hex.neighbor_idx[4] < 0) {
            // 邻星不足 5 颗, 跳过
            continue;
        }

        // 2. 收集候选星表星
        std::set<int> candidates = collect_candidates(kv, hex, sigma_d);
        if (candidates.empty()) continue;
        n_pivot_with_candidates++;

        // 3. 对每个候选验证多边形完整性
        bool pivot_passed = false;
        for (int w : candidates) {
            auto matched = verify_polygon(W, w, hex, kv, sigma_d);
            if (matched.size() >= 3) {
                // 通过验证: 投票 (pivot + 每个匹配邻星各 1 票)
                result.votes[{p, w}] += 1;
                pivot_passed = true;
                // 对每个匹配的邻星对投票
                for (const auto& mp : matched) {
                    result.votes[{mp.first, mp.second}] += 1;
                }
                result.n_polygon_passed++;

                // V4.11 §5.3: 角度循环验证 bonus (Phase C 辅助过滤)
                // 对通过距离验证的候选, 用方位角一致性额外加权投票
                // - consistency ∈ [0,1], 1.0 = 完全一致, 0.0 = 完全不一致
                // - bonus = alpha * consistency, alpha=0.5 (设计文档默认)
                // - 邻星数 < 3 时 consistency=0.5 (中性, 不加不减)
                const double alpha = 0.5;
                double consistency = angle_cyclic_verify(
                    U[p], W[w], U, W, hex, matched, 5.0);
                double bonus = alpha * consistency;
                result.votes[{p, w}] += bonus;
            }
        }
        if (pivot_passed) n_pivot_with_pass++;
    }

    // 4. 统计最大票数
    // V4.11: VoteMap value 改为 double (支持 angle bonus), max_v 同步改为 double
    double max_v = 0.0;
    for (const auto& kv_pair : result.votes) {
        if (kv_pair.second > max_v) max_v = kv_pair.second;
    }
    result.max_vote = max_v;
    result.success  = (max_v > 0.0);

    g_polygon_logger.infof("polygon_match: pivots=%d, with_cand=%d, with_pass=%d, "
                           "n_passed=%d, max_vote=%.2f, vote_entries=%zu",
                           n_pivot, n_pivot_with_candidates, n_pivot_with_pass,
                           result.n_polygon_passed, max_v, result.votes.size());

    return result;
}

// ===========================================================================
// V4.7: 自适应降阶多边形匹配 (宽 FOV 专用)
// ===========================================================================

namespace {

// 单阶匹配: 用指定的 K / min_occur / min_match / r_local 执行一轮 polygon_match
// votes 累加到传入的 VoteMap (跨阶合并)
// 返回该阶的 (max_vote, n_polygon_passed, n_pivot_with_pass)
// V4.11: max_vote 改为 double (VoteMap value 已改为 double)
struct DegradedStageResult {
    double max_vote;
    int n_polygon_passed;
    int n_pivot_with_pass;
};

DegradedStageResult polygon_match_single_stage(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double sigma_d,
    double r_local,
    double fov_diag,
    int K,
    int min_occur,
    int min_match,
    VoteMap& votes)
{
    DegradedStageResult sr{0.0, 0, 0};

    int N_U = (int)U.size();
    int n_pivot = std::min(params.n_pivot, N_U);

    int n_pivot_with_candidates = 0;
    int n_pivot_with_pass       = 0;

    for (int p = 0; p < n_pivot; ++p) {
        // 1. 构建描述符 (带 r_local 局部化 + fov_diag 形状剪枝)
        HexDescriptor hex = build_hex_descriptor(U, p, r_local, fov_diag);

        // 检查可用邻星数
        int n_valid = 0;
        for (int k = 0; k < K; ++k) {
            if (hex.distances[k] >= 0.0) n_valid++;
        }
        if (n_valid < min_match) continue;  // 邻星不足, 跳过

        // 2. 收集候选
        std::set<int> candidates = collect_candidates(kv, hex, sigma_d, K, min_occur);
        if (candidates.empty()) continue;
        n_pivot_with_candidates++;

        // 3. 验证多边形完整性
        bool pivot_passed = false;
        for (int w : candidates) {
            auto matched = verify_polygon(W, w, hex, kv, sigma_d, K, min_match);
            if ((int)matched.size() >= min_match) {
                votes[{p, w}] += 1;
                pivot_passed = true;
                for (const auto& mp : matched) {
                    votes[{mp.first, mp.second}] += 1;
                }
                sr.n_polygon_passed++;
            }
        }
        if (pivot_passed) n_pivot_with_pass++;
    }

    // 统计该阶最大票数 (只看本次新增的, 但 votes 是合并的)
    // 注: 这里无法区分哪些是本阶新增的, 所以 max_vote 反映的是合并后的
    //     调用方在每阶结束后会重新统计
    sr.n_pivot_with_pass = n_pivot_with_pass;

    return sr;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// polygon_match_adaptive: 单层构造 + 两档降阶多边形匹配
//   每个 pivot 只调用一次 build_hex_descriptor, 复用至两档:
//     第一档 K=5 六边形 (min_occur=2, min_match=3) — 强约束
//     第二档 K=2 三角形 (min_occur=2, min_match=2) — 仅当第一档 max_vote < 5
//   邻星局部化: r_local = 0.15 * fov_diag (宽 FOV 抗畸变)
//   单 pivot 内 max_vote >= 5 即停 (跳过第二档, 继续下一 pivot)
//   跨 pivot votes 合并累加, 让 PROSAC 有更大采样空间
// ----------------------------------------------------------------------------
PolygonMatchResult polygon_match_adaptive(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double sigma_d,
    double fov_diag,
    bool use_ratio)
{
    PolygonMatchResult result;
    result.n_pivots         = 0;
    result.n_polygon_passed = 0;
    result.max_vote         = 0.0;
    result.success          = false;

    int N_U = (int)U.size();
    if (N_U < 3 || (int)W.size() < 3) {
        g_polygon_logger.warn("polygon_match_adaptive: 星点数不足 (U or W < 3)");
        return result;
    }

    int n_pivot = std::min(params.n_pivot, N_U);
    result.n_pivots = n_pivot;

    // 邻星局部化半径: 15% FOV 对角线
    // 宽 FOV (7.7°) 时 r_local ≈ 1.16°, 邻星在局部小范围内畸变近似线性
    double r_local = 0.15 * fov_diag;

    g_polygon_logger.infof("polygon_match_adaptive: N_U=%d, N_W=%d, n_pivot=%d, "
                           "sigma_d=%.3f, r_local=%.1f\" (%.2f deg)",
                           N_U, (int)W.size(), n_pivot, sigma_d,
                           r_local, r_local / 3600.0);

    // 两档降阶参数
    // 第一档: K=5 六边形 (强约束, 与 polygon_match 一致)
    // 第二档: K=2 三角形 (弱约束, 仅当第一档 max_vote < 5 时启用)
    const int    K1 = 5, min_occur1 = 2, min_match1 = 3;
    const int    K2 = 2, min_occur2 = 2, min_match2 = 2;
    const char*  stage1_name = "六边形";
    const char*  stage2_name = "三角形";

    int total_passed         = 0;
    int n_pivot_stage1_pass  = 0;  // 第一档 max_vote >= 5 直接通过的 pivot 数
    int n_pivot_stage2_used  = 0;  // 启用第二档的 pivot 数

    // 单层构造 + 两档降阶: 每个 pivot 只构建一次描述子, 复用至两档
    for (int p = 0; p < n_pivot; ++p) {
        // 1. 构建描述子 (单次, 带 r_local 局部化 + fov_diag 形状剪枝)
        // V4.12: use_ratio=true 时输出距离比值 (抗宽 FOV 畸变 fallback)
        HexDescriptor hex = build_hex_descriptor(U, p, r_local, fov_diag, use_ratio);

        // ---------- 第一档: K=5 六边形 ----------
        // 检查 K=5 可用邻星数
        int n_valid_k5 = 0;
        for (int k = 0; k < K1; ++k) {
            if (hex.distances[k] >= 0.0) n_valid_k5++;
        }

        int stage1_passed = 0;
        if (n_valid_k5 >= min_match1) {
            int votes_before = (int)result.votes.size();

            std::set<int> candidates = collect_candidates(kv, hex, sigma_d, K1, min_occur1, &W, use_ratio);
            for (int w : candidates) {
                auto matched = verify_polygon(W, w, hex, kv, sigma_d, K1, min_match1, use_ratio);
                if ((int)matched.size() >= min_match1) {
                    result.votes[{p, w}] += 1;
                    for (const auto& mp : matched) {
                        result.votes[{mp.first, mp.second}] += 1;
                    }
                    stage1_passed++;
                }
            }
            total_passed += stage1_passed;

            int votes_after = (int)result.votes.size();

            g_polygon_logger.infof("  [pivot %d/%d %s] K=%d, occur>=%d, match>=%d: "
                                   "cand=%d, passed=%d, votes_added=%d",
                                   p + 1, n_pivot, stage1_name, K1, min_occur1, min_match1,
                                   (int)candidates.size(), stage1_passed,
                                   votes_after - votes_before);

            // 检查合并后 max_vote, 决定是否进入第二档
            // V4.11: VoteMap value 改为 double, max_v 同步改为 double
            double max_v = 0.0;
            for (const auto& kv_pair : result.votes) {
                if (kv_pair.second > max_v) max_v = kv_pair.second;
            }
            if (max_v >= 5.0) {
                // 第一档已找到强匹配, 不降阶, 继续下一个 pivot
                n_pivot_stage1_pass++;
                continue;
            }
        } else {
            g_polygon_logger.infof("  [pivot %d/%d %s] K=%d 邻星不足 (%d<%d), 跳过第一档",
                                   p + 1, n_pivot, stage1_name, K1, n_valid_k5, min_match1);
        }

        // ---------- 第二档: K=2 三角形 (复用同一描述子的前 2 颗邻星) ----------
        // 检查 K=2 可用邻星数
        int n_valid_k2 = 0;
        for (int k = 0; k < K2; ++k) {
            if (hex.distances[k] >= 0.0) n_valid_k2++;
        }
        if (n_valid_k2 < min_match2) {
            continue;
        }

        n_pivot_stage2_used++;

        int votes_before = (int)result.votes.size();
        std::set<int> candidates = collect_candidates(kv, hex, sigma_d, K2, min_occur2, &W, use_ratio);

        int stage2_passed = 0;
        for (int w : candidates) {
            auto matched = verify_polygon(W, w, hex, kv, sigma_d, K2, min_match2, use_ratio);
            if ((int)matched.size() >= min_match2) {
                result.votes[{p, w}] += 1;
                for (const auto& mp : matched) {
                    result.votes[{mp.first, mp.second}] += 1;
                }
                stage2_passed++;
            }
        }
        total_passed += stage2_passed;

        int votes_after = (int)result.votes.size();

        g_polygon_logger.infof("  [pivot %d/%d %s] K=%d, occur>=%d, match>=%d: "
                               "cand=%d, passed=%d, votes_added=%d",
                               p + 1, n_pivot, stage2_name, K2, min_occur2, min_match2,
                               (int)candidates.size(), stage2_passed,
                               votes_after - votes_before);
    }

    result.n_polygon_passed = total_passed;

    // V4.11: VoteMap value 改为 double, max_v 同步改为 double
    double max_v = 0.0;
    for (const auto& kv_pair : result.votes) {
        if (kv_pair.second > max_v) max_v = kv_pair.second;
    }
    result.max_vote = max_v;
    result.success  = (max_v > 0.0);

    g_polygon_logger.infof("polygon_match_adaptive 完成: total_passed=%d, "
                           "stage1_pass=%d, stage2_used=%d, "
                           "max_vote=%.2f, vote_entries=%zu",
                           total_passed, n_pivot_stage1_pass, n_pivot_stage2_used,
                           max_v, result.votes.size());

    return result;
}

// ===========================================================================
// Task 7: 几何投票
// ===========================================================================

// ----------------------------------------------------------------------------
// geometric_vote: 全星对 pairwise 距离对称投票
//   对图像侧每对星 (i,j), k-vector 查询 W 中角距匹配的星对 (a,b), 对称投票
//   若 N_U > geo_n_max, 仅取前 geo_n_max 颗 (U 已按 flux 降序, 取最亮)
//   始终启用距离窗口过滤 [0.05*fov_diag, 0.95*fov_diag] (排除近距噪声对)
//   若候选星对数 > 100, 跳过该 (i,j) 对 (低区分度)
//   tol = 3 * sigma_d (与 polygon_match 一致, 原先用 ransac_inlier_threshold=9" 过大)
// ----------------------------------------------------------------------------
void geometric_vote(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const KVectorIndex& kv,
    const IPVSolverParams& params,
    double fov_diag,
    double sigma_d,
    VoteMap& votes)
{
    int N_U = (int)U.size();
    if (N_U < 2) return;

    // 限制 geometric_vote 用的星点数: U 已按 flux 降序, 取前 geo_n_max 颗
    // (亮天区饱和星 200+ 颗时, O(N²) 噪声爆炸淹没真实匹配)
    // V4.6: 宽 FOV (>3°) 时 geo_n_max 降到 50, 减少 pairwise 组合 (C(100,2)=4950 -> C(50,2)=1225)
    //       避免高密度星场下错误配对获得高票淹没真实匹配 (Galaxy_Center_02 问题)
    int geo_n_max;
    bool is_wide_fov = (fov_diag > 3.0 * 3600.0);  // >3°
    if (is_wide_fov) {
        geo_n_max = 50;
    } else {
        geo_n_max = std::max(50, params.img_n_target * 2);
    }
    int N_U_eff = std::min(N_U, geo_n_max);

    // tol = 3 * ransac_inlier_threshold (固定 9.0", 与原逻辑一致)
    // 注: 曾尝试用 3*sigma_d 但宽 FOV 时 sigma_d 可达 9"+ 导致 tol=27"+ 过大;
    //     也尝试 min(3*sigma_d, 9.0") 但中 FOV 时 tol=6.0" 过严丢失真实匹配。
    //     保持固定 9.0" 是经验最优值。
    // V4.6: 宽 FOV 时 tol 降到 6.0" (2 像素), 减少错误距离匹配
    double sigma_d_default = 3.0;
    if (params.ransac_inlier_threshold_arcsec > 0.0) {
        sigma_d_default = params.ransac_inlier_threshold_arcsec;
    }
    double tol = 3.0 * sigma_d_default;
    if (is_wide_fov) {
        tol = 2.0 * sigma_d_default;  // 6.0" (2 像素)
    }

    // 大星表时启用距离窗口过滤 (原逻辑: 仅 N_U_eff > 100 时启用)
    // 注: 始终启用会排除中 FOV 的近距离真实匹配, 降低成功率
    // V4.6: 宽 FOV 时强制启用, 排除近距/远距噪声对
    bool  use_window = (N_U_eff > 100) || is_wide_fov;
    double d_lo_win = 0.1 * fov_diag;
    double d_hi_win = 0.9 * fov_diag;

    long long n_pairs_processed = 0;
    long long n_pairs_skipped_window = 0;
    long long n_pairs_skipped_ambiguous = 0;
    long long n_votes_added = 0;

    for (int i = 0; i < N_U_eff; ++i) {
        for (int j = i + 1; j < N_U_eff; ++j) {
            double dx = U[i].x - U[j].x;
            double dy = U[i].y - U[j].y;
            double d_ij = std::sqrt(dx * dx + dy * dy);

            // 大星表距离窗口过滤
            if (use_window && (d_ij < d_lo_win || d_ij > d_hi_win)) {
                n_pairs_skipped_window++;
                continue;
            }

            double q_lo = d_ij - tol;
            double q_hi = d_ij + tol;
            if (q_lo < 0.0) q_lo = 0.0;

            auto w_pairs = kvector_query(kv, q_lo, q_hi);

            // 低区分度对跳过 (候选过多, 回退到 200)
            if (w_pairs.size() > 200) {
                n_pairs_skipped_ambiguous++;
                continue;
            }

            n_pairs_processed++;
            // 对称投票: (i,a) 和 (j,b)
            for (const auto& pr : w_pairs) {
                int a = pr.first;
                int b = pr.second;
                votes[{i, a}] += 1;
                votes[{j, b}] += 1;
                n_votes_added += 2;
            }
        }
    }

    g_polygon_logger.infof("geometric_vote: N_U=%d, N_U_eff=%d (geo_n_max=%d), tol=%.2f\", "
                           "processed=%lld, skip_window=%lld, skip_ambig=%lld, "
                           "votes_added=%lld, vote_entries=%zu",
                           N_U, N_U_eff, geo_n_max, tol, n_pairs_processed,
                           n_pairs_skipped_window, n_pairs_skipped_ambiguous,
                           n_votes_added, votes.size());
}

// ===========================================================================
// Task 8: 共识提取
// ===========================================================================

// ----------------------------------------------------------------------------
// extract_consensus: 从投票矩阵提取候选匹配列表
//   对每个图像星 i 取 top-K_w (默认 3) 个 w 候选, 若 max_vote >= vote_threshold 则加入
//   confidence = max_vote / (max_vote + second_vote + 1.0)
//   按 vote 降序返回
//
// 设计要点 (V4.6 改进):
//   原 V4.5 每个 u 只取 top-1 w, 当 top-1 是错误配对时真实匹配被丢弃,
//   导致 PROSAC 无法找到正确变换 (NGC55_T3_01 max_vote=26 但仅 2 内点)。
//   V4.6 每个 u 取 top-3 w, 增加 candidates 多样性, 让 PROSAC 有更大采样空间。
//   top-2/top-3 的 vote 要求 >= max(vote_threshold, max_vote/2), 避免引入过多噪声。
// ----------------------------------------------------------------------------
std::vector<CandidateMatch> extract_consensus(
    const VoteMap& votes,
    int N_U,
    const IPVSolverParams& params)
{
    std::vector<CandidateMatch> candidates;

    // 固定阈值 (自适应 max_vote/3 过于激进, 会过滤掉真实匹配, 回退到固定阈值)
    // 小星表时降 1 保证候选数
    int vote_threshold = params.vote_threshold;
    if (N_U < 15) {
        vote_threshold = std::max(1, params.vote_threshold - 1);
    }

    // V4.6: 每个 u 取 top-K_w 个 w 候选
    const int K_w = 3;

    // 按图像星索引 i 聚合 (w, vote)
    // V4.11: vote 类型从 int 改为 double (支持 angle bonus 累加)
    std::unordered_map<int, std::vector<std::pair<int,double>>> by_u;
    for (const auto& kv_pair : votes) {
        by_u[kv_pair.first.u].emplace_back(kv_pair.first.w, kv_pair.second);
    }

    int n_multi_added = 0;  // 统计 top-2/top-3 增加的候选数

    for (int i = 0; i < N_U; ++i) {
        auto it = by_u.find(i);
        if (it == by_u.end() || it->second.empty()) continue;

        // 按票数降序排序
        auto& wv = it->second;
        std::sort(wv.begin(), wv.end(),
                  [](const std::pair<int,double>& a, const std::pair<int,double>& b) {
                      return a.second > b.second;
                  });

        int    w_best        = wv[0].first;
        double max_vote      = wv[0].second;
        double second_vote   = (wv.size() >= 2) ? wv[1].second : 0.0;
        double confidence    = max_vote / (max_vote + second_vote + 1.0);

        if (max_vote < (double)vote_threshold) continue;

        // top-1 始终加入
        candidates.push_back({i, w_best, max_vote, confidence});

        // V4.6: top-2/top-3 加入条件: vote >= max(vote_threshold, max_vote/2)
        // 避免引入票数过低的噪声候选
        // V4.11: max_vote 现为 double, secondary_threshold 同步改为 double
        double secondary_threshold = std::max((double)vote_threshold, max_vote / 2.0);
        for (int k = 1; k < std::min(K_w, (int)wv.size()); ++k) {
            if (wv[k].second >= secondary_threshold) {
                int    w_k       = wv[k].first;
                double vote_k    = wv[k].second;
                double conf_k    = vote_k / (vote_k + max_vote + 1.0);
                candidates.push_back({i, w_k, vote_k, conf_k});
                n_multi_added++;
            }
        }
    }

    // 按 vote 降序排序候选列表
    // V4.11: vote 改为 double, 用 < 比较避免浮点相等的边界问题
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateMatch& a, const CandidateMatch& b) {
                  if (a.vote != b.vote) return a.vote > b.vote;
                  return a.confidence > b.confidence;
              });

    g_polygon_logger.infof("extract_consensus: N_U=%d, vote_threshold=%d, K_w=%d, "
                           "n_candidates=%zu (top-1=%zu, top-2/3 增加=%d)",
                           N_U, vote_threshold, K_w,
                           candidates.size(), candidates.size() - n_multi_added,
                           n_multi_added);

    return candidates;
}

} // namespace ipv

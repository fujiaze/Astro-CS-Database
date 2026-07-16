// ============================================================================
// ipv_triangle.cpp - IPV 三角形匹配模块实现
//
// 三角形匹配算法实现:
//   - set_triangle        三角形构造, 边长排序 a>=b>=c
//   - stars_to_triangles  N 颗星 -> C(N,3) 三角形
//   - make_vote_matrix    ba/ca 空间匹配 + 顶点对投票
//   - top_vote_getters    按票数降序提取 top-N 匹配对
//
// 实现说明:
//   1. V4.22: 投票矩阵改回 2D 数组 std::vector<std::vector<int>> (numA x numB)
//      原 V4.20 用稀疏 unordered_map<VoteKey, double>, 累积逻辑不等价
//   2. V4.22: top_vote_getters 添加 AT_MATCH_MINVOTES=2 门槛
//      单票配对直接丢弃
//   3. 不做 sort_triangle_array + find_ba_triangle 二分加速
//      (N=20 时 C(20,3)=1140, 双重循环 1.3M 次比较足够快)
//   4. 不做 scale/rotation 约束 (假定 U/W 同坐标系, 尺度比=1, 任意旋转)
//   5. 自适应星数: max_vote < 3 时扩充 20 -> 40 -> 60
//
// 日期: 2026-07-05
// ============================================================================

#include "ipv_triangle.h"
#include "ipv_log.h"

#include <cmath>
#include <algorithm>
#include <vector>
#include <omp.h>

namespace ipv {

// ---------------------------------------------------------------------------
// 模块级日志器 (对齐 ipv_polygon.cpp 风格)
// ---------------------------------------------------------------------------
static Logger g_triangle_logger;

Logger& triangle_logger() {
    return g_triangle_logger;
}

void init_triangle_logger(const std::string& path) {
    g_triangle_logger.init(path);
}

// ===========================================================================
// 内部辅助: 欧氏距离
// ===========================================================================
static inline double euclid_dist(
    const StarPoint& a,
    const StarPoint& b)
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ===========================================================================
// set_triangle: 三角形构造
//   输入: star_array 中的三颗星 (s1, s2, s3), 索引互不相同
//   输出: tri 字段填充 (a_index/b_index/c_index/ba/ca/a_length)
//   返回: true=有效三角形, false=退化 (a_length < 1e-6, 三点共线或重合)
//
// 边长排序逻辑:
//   d12 = dist(s1, s2),  d23 = dist(s2, s3),  d13 = dist(s1, s3)
//   a = max(d12, d23, d13),  b = mid,  c = min
//   a_index = 最长边对面的顶点 (若最长边为 d12, 对面顶点为 s3)
//   b_index = 次长边对面的顶点
//   c_index = 最短边对面的顶点
//   ba = b/a,  ca = c/a  (a > 0 时)
// ===========================================================================
static bool set_triangle(
    Triangle& tri,
    const std::vector<StarPoint>& stars,
    int s1,
    int s2,
    int s3)
{
    // 三点互异
    if (s1 == s2 || s1 == s3 || s2 == s3) return false;

    double d12 = euclid_dist(stars[s1], stars[s2]);
    double d23 = euclid_dist(stars[s2], stars[s3]);
    double d13 = euclid_dist(stars[s1], stars[s3]);

    // 距离非负
    if (d12 < 0.0 || d23 < 0.0 || d13 < 0.0) return false;

    double a, b, c;

    // 三分排序逻辑 (严格)
    if (d12 >= d23 && d12 >= d13) {
        // 最长边 d12 连接 s1-s2, 对面顶点 s3
        tri.a_index = s3;
        a = d12;
        if (d23 >= d13) {
            // 次长边 d23 连接 s2-s3, 对面顶点 s1
            tri.b_index = s1;
            b = d23;
            // 最短边 d13 连接 s1-s3, 对面顶点 s2
            tri.c_index = s2;
            c = d13;
        } else {
            // 次长边 d13 连接 s1-s3, 对面顶点 s2
            tri.b_index = s2;
            b = d13;
            // 最短边 d23 连接 s2-s3, 对面顶点 s1
            tri.c_index = s1;
            c = d23;
        }
    } else if (d23 > d12 && d23 >= d13) {
        // 最长边 d23 连接 s2-s3, 对面顶点 s1
        tri.a_index = s1;
        a = d23;
        if (d12 > d13) {
            // 次长边 d12 连接 s1-s2, 对面顶点 s3
            tri.b_index = s3;
            b = d12;
            // 最短边 d13 连接 s1-s3, 对面顶点 s2
            tri.c_index = s2;
            c = d13;
        } else {
            // 次长边 d13 连接 s1-s3, 对面顶点 s2
            tri.b_index = s2;
            b = d13;
            // 最短边 d12 连接 s1-s2, 对面顶点 s3
            tri.c_index = s3;
            c = d12;
        }
    } else if (d13 > d12 && d13 > d23) {
        // 最长边 d13 连接 s1-s3, 对面顶点 s2
        tri.a_index = s2;
        a = d13;
        if (d12 > d23) {
            // 次长边 d12 连接 s1-s2, 对面顶点 s3
            tri.b_index = s3;
            b = d12;
            // 最短边 d23 连接 s2-s3, 对面顶点 s1
            tri.c_index = s1;
            c = d23;
        } else {
            // 次长边 d23 连接 s2-s3, 对面顶点 s1
            tri.b_index = s1;
            b = d23;
            // 最短边 d12 连接 s1-s2, 对面顶点 s3
            tri.c_index = s3;
            c = d12;
        }
    } else {
        // 不应到达此处 (三点距离全相等且为 0 的退化情形)
        return false;
    }

    tri.a_length = a;
    if (a > 0.0) {
        tri.ba = b / a;
        tri.ca = c / a;
    } else {
        // a == 0: 三点重合, 设 ba=ca=1.0 让其被忽略
        tri.ba = 1.0;
        tri.ca = 1.0;
    }

    // 退化三角形跳过 (a_length < 1e-6: 三点共线或重合)
    if (a < 1e-6) return false;

    return true;
}

// ===========================================================================
// stars_to_triangles: N 颗星 -> C(N,3) 个三角形
//   三重循环 i < j < k, 对每个三元组调用 set_triangle
//   退化三角形 (a_length < 1e-6) 跳过, 不加入结果
// ===========================================================================
static std::vector<Triangle> stars_to_triangles(
    const std::vector<StarPoint>& stars,
    int n_stars)
{
    std::vector<Triangle> triangles;
    if (n_stars < 3) return triangles;

    // C(N,3) = N*(N-1)*(N-2)/6
    size_t expected = (size_t)n_stars * (n_stars - 1) * (n_stars - 2) / 6;

    // V4.26 OpenMP 并行化: 外层 i 循环并行, 每个线程维护局部 triangles vector,
    // 最后合并. N=60 时生成 34220 个三角形, 串行 ~50ms, 并行后 ~5ms.
    int n_threads = omp_get_max_threads();
    std::vector<std::vector<Triangle>> per_thread(n_threads);
    std::vector<int> per_thread_skipped(n_threads, 0);

    for (int t = 0; t < n_threads; ++t) {
        per_thread[t].reserve(expected / n_threads + 1024);
    }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto& local_tris = per_thread[tid];
        int& local_skipped = per_thread_skipped[tid];

        // schedule(dynamic): 内层 j/k 循环工作量随 i 递减, dynamic 平衡负载
        #pragma omp for schedule(dynamic)
        for (int i = 0; i < n_stars - 2; ++i) {
            for (int j = i + 1; j < n_stars - 1; ++j) {
                for (int k = j + 1; k < n_stars; ++k) {
                    Triangle tri;
                    if (set_triangle(tri, stars, i, j, k)) {
                        local_tris.push_back(tri);
                    } else {
                        ++local_skipped;
                    }
                }
            }
        }
    }

    // 合并线程局部结果 (顺序按 tid 串接, 不影响算法结果:
    // 投票矩阵只依赖三角形集合, 与顺序无关)
    size_t total_valid = 0;
    for (const auto& v : per_thread) total_valid += v.size();
    triangles.reserve(total_valid);
    for (auto& v : per_thread) {
        triangles.insert(triangles.end(),
                         std::make_move_iterator(v.begin()),
                         std::make_move_iterator(v.end()));
    }

    int n_skipped_degenerate = 0;
    for (int s : per_thread_skipped) n_skipped_degenerate += s;

    g_triangle_logger.debugf("stars_to_triangles: N=%d, expected=%zu, "
                             "valid=%zu, skipped_degenerate=%d (并行, %d 线程)",
                             n_stars, expected, triangles.size(),
                             n_skipped_degenerate, n_threads);

    // V4.20: 剪枝 ba > AT_MATCH_RATIO(0.9) 的细长三角形:
    //   - ba = b/a 越小越接近等边, 描述符越稳定
    //   - ba > 0.9 的三角形在 ba/ca 空间聚集, 容易产生虚假匹配
    // 这里用 remove_if 直接剔除
    size_t n_before_prune = triangles.size();
    triangles.erase(
        std::remove_if(triangles.begin(), triangles.end(),
            [](const Triangle& t) { return t.ba > 0.9; }),
        triangles.end());
    size_t n_after_prune = triangles.size();

    g_triangle_logger.debugf("stars_to_triangles: prune_triangle_array "
                             "ba>0.9 剪枝: before=%zu, after=%zu, pruned=%zu",
                             n_before_prune, n_after_prune,
                             n_before_prune - n_after_prune);

    return triangles;
}

// ===========================================================================
// make_vote_matrix: ba/ca 空间匹配 + 顶点对投票
//   对每个 B 侧三角形 triB, 遍历所有 A 侧三角形 triA:
//     若 (ba_A - ba_B)^2 + (ca_A - ca_B)^2 < tolerance^2,
//     且 (V4.23) scale 约束: min_scale <= a_length_A / a_length_B <= max_scale,
//     为 3 个顶点对投票:
//       votes[triA.a_index][triB.a_index]++
//       votes[triA.b_index][triB.b_index]++
//       votes[triA.c_index][triB.c_index]++
//
// V4.22: 用 2D int 数组 vote_matrix[numA][numB]
// V4.23: percent_scale_range=20% scale 约束, 过滤不同尺度的错误三角形匹配
// ===========================================================================
static void make_vote_matrix(
    const std::vector<Triangle>& tris_A,
    const std::vector<Triangle>& tris_B,
    double tolerance,
    std::vector<std::vector<int>>& votes,   // 2D 投票矩阵 [numA][numB]
    int numA,                                // A 侧星数 (行数)
    int numB,                                // B 侧星数 (列数)
    double scale_min = -1.0,                 // V4.23: a_length_A/a_length_B 下限 (-1=不约束)
    double scale_max = -1.0)                 // V4.23: a_length_A/a_length_B 上限 (-1=不约束)
{
    double tol2 = tolerance * tolerance;
    int n_A = (int)tris_A.size();
    int n_B = (int)tris_B.size();
    bool use_scale = (scale_min > 0.0 && scale_max > 0.0);

    // V4.22: 分配并初始化 numA x numB 的 2D 投票矩阵
    votes.assign(numA, std::vector<int>(numB, 0));

    // V4.26 OpenMP 并行化: 投票矩阵共享写入有数据竞争, 采用线程局部矩阵 + 合并方案.
    // 性能瓶颈: N=60 时 n_A*n_B = 34220*34220 = 1.17 亿次比较, 串行 ~600ms.
    // 并行后降到 ~50ms (16 线程).
    //
    // 内存: 每线程 numA*numB ints. 60x60x16 = 230KB, 可接受.
    // 使用 flatten 1D 数组提升缓存命中率 (避免 2D vector 双重指针解引用).
    int n_threads = omp_get_max_threads();
    const size_t flat_size = (size_t)numA * (size_t)numB;
    std::vector<std::vector<int>> local_votes(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        local_votes[t].assign(flat_size, 0);
    }
    std::vector<int> n_rejected_per_thread(n_threads, 0);
    std::vector<int> n_matched_per_thread(n_threads, 0);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<int>& local = local_votes[tid];
        int& local_rejected = n_rejected_per_thread[tid];
        int& local_matched = n_matched_per_thread[tid];

        // schedule(dynamic): 不同 j 的命中数差异大, dynamic 平衡负载
        #pragma omp for schedule(dynamic, 64)
        for (int j = 0; j < n_B; ++j) {
            double ba_B = tris_B[j].ba;
            double ca_B = tris_B[j].ca;
            double a_len_B = tris_B[j].a_length;

            // 顶点索引越界保护 (num_stars > nbright 时)
            if (tris_B[j].a_index >= numB || tris_B[j].b_index >= numB
                || tris_B[j].c_index >= numB) {
                continue;
            }

            int b_a = tris_B[j].a_index;
            int b_b = tris_B[j].b_index;
            int b_c = tris_B[j].c_index;

            for (int i = 0; i < n_A; ++i) {
                // A 侧顶点索引越界保护
                if (tris_A[i].a_index >= numA || tris_A[i].b_index >= numA
                    || tris_A[i].c_index >= numA) {
                    continue;
                }

                double ba_A = tris_A[i].ba;
                double ca_A = tris_A[i].ca;

                double dba = ba_A - ba_B;
                double dca = ca_A - ca_B;

                if (dba * dba + dca * dca < tol2) {
                    // V4.23: scale 约束
                    // ratio = a_length_A / a_length_B (像素/角秒 = 1/s0)
                    if (use_scale && a_len_B > 1e-15) {
                        double ratio = tris_A[i].a_length / a_len_B;
                        if (ratio < scale_min || ratio > scale_max) {
                            ++local_rejected;
                            continue;
                        }
                    }

                    // 匹配! 为 3 个顶点对投票
                    // 写入线程局部矩阵 (1D flatten: index = a * numB + b)
                    local[(size_t)tris_A[i].a_index * numB + b_a]++;
                    local[(size_t)tris_A[i].b_index * numB + b_b]++;
                    local[(size_t)tris_A[i].c_index * numB + b_c]++;
                    ++local_matched;
                }
            }
        }
    }

    // 合并线程局部矩阵到 votes (并行 collapse(2) 加速)
    #pragma omp parallel for collapse(2) schedule(static)
    for (int i = 0; i < numA; ++i) {
        for (int j = 0; j < numB; ++j) {
            int sum = 0;
            size_t idx = (size_t)i * numB + j;
            for (int t = 0; t < n_threads; ++t) {
                sum += local_votes[t][idx];
            }
            votes[i][j] = sum;
        }
    }

    // 汇总计数器
    int n_rejected_by_scale = 0;
    int n_matched_triangles = 0;
    for (int t = 0; t < n_threads; ++t) {
        n_rejected_by_scale += n_rejected_per_thread[t];
        n_matched_triangles += n_matched_per_thread[t];
    }

    g_triangle_logger.infof("make_vote_matrix: n_A=%d, n_B=%d, "
                            "matched_triangles=%d, vote_matrix=%dx%d, "
                            "scale_rejected=%d (range=[%.4f,%.4f]) "
                            "[并行 %d 线程]",
                            n_A, n_B, n_matched_triangles, numA, numB,
                            n_rejected_by_scale,
                            use_scale ? scale_min : -1.0,
                            use_scale ? scale_max : -1.0,
                            n_threads);
}

// ===========================================================================
// top_vote_getters: 提取最高票匹配对
//   遍历 2D vote_matrix, 按票数降序排序, 返回 top N 对
//   N = top_n (通常等于 n_stars_A, 默认 20)
//
// V4.22: 添加 AT_MATCH_MINVOTES=2 门槛
//   - 只允许 vote_count >= AT_MATCH_MINVOTES 的配对进入候选
//   - 单票配对 (vote_count=1) 直接丢弃 (P1-2: 单票噪声污染初始 6 对 TRANS)
//
// V4.25: 贪心去重 (同一 u 和同一 w 只保留票数最高对)
//   - 原算法不去重, 但理想三角形匹配质量高时前 6 对不会出现重复星
//   - IPv 在饱和星密集场景下, 错误配对票数可能高于正确配对,
//     导致 top 6 中同一 u 匹配多个 w (如 u=4 同时匹配 w=36 和 w=6),
//     矛盾目标扭曲 calc_trans_general 最小二乘拟合
//   - 去重策略: 按票数降序贪心遍历, u/w 均未出现才保留
//
// 返回: top_pairs (按票数降序) + 对应的 votes 数组
// ===========================================================================
static void top_vote_getters(
    const std::vector<std::vector<int>>& votes,
    int top_n,
    std::vector<MatchPair>& top_pairs,
    std::vector<double>& top_votes)
{
    top_pairs.clear();
    top_votes.clear();

    if (votes.empty()) return;

    int numA = (int)votes.size();
    int numB = (numA > 0) ? (int)votes[0].size() : 0;
    if (numB == 0) return;

    // V4.22: 遍历整个 2D 数组, 收集 vote_count >= AT_MATCH_MINVOTES 的配对
    std::vector<std::pair<MatchPair, int>> entries;
    for (int i = 0; i < numA; ++i) {
        for (int j = 0; j < numB; ++j) {
            int v = votes[i][j];
            if (v >= AT_MATCH_MINVOTES) {
                entries.push_back({{i, j}, v});
            }
        }
    }

    g_triangle_logger.infof("top_vote_getters: 矩阵=%dx%d, "
                            "候选对数(vote>=%d)=%zu, top_n=%d",
                            numA, numB, AT_MATCH_MINVOTES,
                            entries.size(), top_n);

    if (entries.empty()) {
        g_triangle_logger.warnf("top_vote_getters: 无配对满足 "
                                "vote >= %d, top_pairs 为空",
                                AT_MATCH_MINVOTES);
        return;
    }

    // 按票数降序 (票数相同则按 u, w 升序保证确定性)
    std::sort(entries.begin(), entries.end(),
        [](const std::pair<MatchPair, int>& a,
           const std::pair<MatchPair, int>& b) {
            if (a.second != b.second) return a.second > b.second;
            if (a.first.u != b.first.u) return a.first.u < b.first.u;
            return a.first.w < b.first.w;
        });

    // V4.25: 贪心去重 — 同一 u 和同一 w 只保留票数最高对
    // 避免 top 6 初始对中出现重复星, 防止矛盾目标扭曲初始 TRANS
    std::vector<bool> u_seen(numA, false);
    std::vector<bool> w_seen(numB, false);
    int n_dedup_removed = 0;
    for (const auto& e : entries) {
        if (top_n > 0 && (int)top_pairs.size() >= top_n) break;
        int u = e.first.u;
        int w = e.first.w;
        if (u_seen[u] || w_seen[w]) {
            ++n_dedup_removed;
            continue;
        }
        top_pairs.push_back(e.first);
        top_votes.push_back((double)e.second);
        u_seen[u] = true;
        w_seen[w] = true;
    }

    g_triangle_logger.infof("top_vote_getters: V4.25 去重后 top_pairs=%zu, "
                            "去重剔除=%d",
                            top_pairs.size(), n_dedup_removed);

    // V4.23 调试: 打印前 10 对的票数
    for (int i = 0; i < (int)top_pairs.size() && i < 10; ++i) {
        g_triangle_logger.infof("  [调试] top[%d]: u=%d w=%d, votes=%.0f",
                                 i, top_pairs[i].u, top_pairs[i].w, top_votes[i]);
    }
}

// ===========================================================================
// 单轮匹配: 给定 (n_A, n_B) 执行一次完整 triangle match
//   返回 TriangleMatchResult (不含自适应逻辑)
// V4.23: 添加 s0 参数, 计算 scale 约束 [1/(s0*1.2), 1/(s0*0.8)] (±20%)
// ===========================================================================
static TriangleMatchResult triangle_match_single(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    int n_stars_A,
    int n_stars_B,
    double tolerance,
    double s0 = 0.0)
{
    TriangleMatchResult result;
    result.n_triangles_A = 0;
    result.n_triangles_B = 0;
    result.max_vote      = 0;
    result.success       = false;

    // 边界检查: 至少 3 颗星才能形成三角形
    int N_U = (int)U.size();
    int N_W = (int)W.size();
    if (N_U < 3 || N_W < 3) {
        g_triangle_logger.warn("triangle_match_single: 星点数不足 (U or W < 3)");
        return result;
    }

    // 限制不超过实际星点数
    if (n_stars_A > N_U) n_stars_A = N_U;
    if (n_stars_B > N_W) n_stars_B = N_W;
    if (n_stars_A < 3 || n_stars_B < 3) {
        g_triangle_logger.warn("triangle_match_single: 限制后星数 < 3");
        return result;
    }

    // V4.23: 计算 scale 约束 (percent_scale_range=20%)
    // U 是像素, W 是角秒, 预期 ratio = U.a_length / W.a_length = 1/s0
    // 允许范围 [1/(s0*1.2), 1/(s0*0.8)] (±20%)
    double scale_min = -1.0, scale_max = -1.0;
    if (s0 > 0.0) {
        double expected_ratio = 1.0 / s0;  // 像素/角秒
        scale_min = expected_ratio / 1.2;  // +20% 容差
        scale_max = expected_ratio / 0.8;  // -20% 容差
    }

    g_triangle_logger.infof("triangle_match_single: n_A=%d, n_B=%d, tol=%.4f, "
                            "s0=%.4f, scale=[%.4f,%.4f]",
                            n_stars_A, n_stars_B, tolerance, s0,
                            scale_min, scale_max);

    // 1. 构建三角形 (假定 U/W 已按 flux 降序, 取前 N 颗即最亮 N 颗)
    std::vector<Triangle> tris_A = stars_to_triangles(U, n_stars_A);
    std::vector<Triangle> tris_B = stars_to_triangles(W, n_stars_B);

    result.n_triangles_A = (int)tris_A.size();
    result.n_triangles_B = (int)tris_B.size();

    if (tris_A.empty() || tris_B.empty()) {
        g_triangle_logger.warn("triangle_match_single: 三角形数为 0, 跳过");
        return result;
    }

    // 2. 投票 (V4.22: 2D 数组, V4.23: 添加 scale 约束)
    std::vector<std::vector<int>> votes;
    make_vote_matrix(tris_A, tris_B, tolerance, votes, n_stars_A, n_stars_B,
                     scale_min, scale_max);

    if (votes.empty()) {
        g_triangle_logger.warn("triangle_match_single: 无匹配三角形, votes 为空");
        return result;
    }

    // 3. 提取 top-N 匹配对 (N = n_stars_A)
    //    V4.22: 内部添加 AT_MATCH_MINVOTES=2 门槛过滤单票噪声
    top_vote_getters(votes, n_stars_A, result.top_pairs, result.votes);

    // 4. 统计最大票数 (遍历整个 2D 矩阵, 不受 MINVOTES 过滤影响)
    int max_v = 0;
    int n_candidates = 0;   // vote_count >= AT_MATCH_MINVOTES 的对数
    for (const auto& row : votes) {
        for (int v : row) {
            if (v > max_v) max_v = v;
            if (v >= AT_MATCH_MINVOTES) ++n_candidates;
        }
    }
    result.max_vote = max_v;
    result.success  = (max_v > 0);

    g_triangle_logger.infof("triangle_match_single 完成: tri_A=%d, tri_B=%d, "
                            "vote_matrix=%zux%zu, max_vote=%d, "
                            "候选对数(vote>=%d)=%d, top_pairs=%zu",
                            result.n_triangles_A, result.n_triangles_B,
                            votes.size(),
                            votes.empty() ? 0 : votes[0].size(),
                            max_v, AT_MATCH_MINVOTES, n_candidates,
                            result.top_pairs.size());

    return result;
}

// ===========================================================================
// triangle_match: 主入口 (V4.24: 直接用 60 颗)
//   V4.24: 直接用 60 颗星, 不做自适应扩充
//   之前 V4.22 用 20->40->60 自适应, max_vote>=3 就停止, 导致投票矩阵不稳定
//   直接用 60 颗星, 三角形匹配更准确, 初始 6 对质量更好
//
// V4.23: 保留 s0 参数, 用于 scale 约束 (±20%)
// ===========================================================================
TriangleMatchResult triangle_match(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    int n_stars_A,
    int n_stars_B,
    double tolerance,
    double s0)
{
    g_triangle_logger.infof("=== triangle_match 开始: N_U=%d, N_W=%d, "
                            "n_A0=%d, n_B0=%d, tol=%.4f, s0=%.4f ===",
                            (int)U.size(), (int)W.size(),
                            n_stars_A, n_stars_B, tolerance, s0);

    // V4.24: 直接用 60 颗, 不做自适应扩充
    // 若星数 < 60, triangle_match_single 内部会截断到实际星数
    const int    MIN_VOTES_THRESHOLD = 3;
    const int    adapt_seq[] = {60};
    const int    n_adapt = (int)(sizeof(adapt_seq) / sizeof(adapt_seq[0]));

    TriangleMatchResult best_result;
    best_result.n_triangles_A = 0;
    best_result.n_triangles_B = 0;
    best_result.max_vote      = 0;
    best_result.success       = false;

    for (int stage = 0; stage < n_adapt; ++stage) {
        int nA = adapt_seq[stage];
        int nB = adapt_seq[stage];

        // 第一阶段尊重调用方传入的 n_stars_A/B (若大于 20 则以调用方为准)
        if (stage == 0) {
            if (n_stars_A > nA) nA = n_stars_A;
            if (n_stars_B > nB) nB = n_stars_B;
        }

        // V4.20: 任一侧超过实际星数即停止扩充 (原 && 过于严格)
        // 例: U 有 30 颗、W 有 100 颗, stage=2 时 nA=60>30 但 nB=60<100,
        //     原 && 不满足会浪费一次计算; 改 || 后立即截断
        if (nA > (int)U.size() || nB > (int)W.size()) {
            g_triangle_logger.infof("triangle_match: stage=%d nA=%d nB=%d "
                                    "超过实际星数, 停止扩充",
                                    stage, nA, nB);
            // 用实际星数跑一次
            nA = std::min(nA, (int)U.size());
            nB = std::min(nB, (int)W.size());
            TriangleMatchResult r = triangle_match_single(U, W, nA, nB, tolerance, s0);
            if (r.max_vote > best_result.max_vote) {
                best_result = r;
            }
            break;
        }

        g_triangle_logger.infof("triangle_match: stage=%d, nA=%d, nB=%d",
                                stage, nA, nB);

        TriangleMatchResult r = triangle_match_single(U, W, nA, nB, tolerance, s0);

        // 保留 max_vote 更高的结果
        if (r.max_vote > best_result.max_vote) {
            best_result = r;
        }

        // 成功条件: max_vote >= MIN_VOTES_THRESHOLD
        if (r.max_vote >= MIN_VOTES_THRESHOLD) {
            g_triangle_logger.infof("triangle_match: stage=%d 成功 "
                                    "(max_vote=%d >= %d), 停止自适应",
                                    stage, r.max_vote, MIN_VOTES_THRESHOLD);
            break;
        }

        g_triangle_logger.infof("triangle_match: stage=%d max_vote=%d < %d, "
                                "继续扩充",
                                stage, r.max_vote, MIN_VOTES_THRESHOLD);
    }

    g_triangle_logger.infof("=== triangle_match 结束: tri_A=%d, tri_B=%d, "
                            "top_pairs=%zu, max_vote=%d, success=%d ===",
                            best_result.n_triangles_A,
                            best_result.n_triangles_B,
                            best_result.top_pairs.size(),
                            best_result.max_vote,
                            (int)best_result.success);

    return best_result;
}

} // namespace ipv

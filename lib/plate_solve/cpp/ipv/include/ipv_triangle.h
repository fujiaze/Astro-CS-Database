#ifndef IPV_TRIANGLE_H
#define IPV_TRIANGLE_H

// ============================================================================
// ipv_triangle.h - IPV 三角形匹配模块
//
// 实现 Valdes et al. PASP 107, 1119 (1995) 的三角形匹配算法
// (set_triangle / stars_to_triangles / make_vote_matrix / top_vote_getters)。
//
// 替换原 polygon_match 作为 iter_trans 的初始匹配对来源:
// 1. stars_to_triangles: N 颗星 -> C(N,3) 个三角形 (边长排序 a>=b>=c)
// 2. make_vote_matrix: ba/ca 空间最近邻匹配 + 顶点对投票
// 3. top_vote_getters: 按票数降序提取 top-N 匹配对
//
// 描述符基于边长比值 (ba=b/a, ca=c/a), 翻转/旋转/平移不变, 仅对尺度
// 不变 (两套星表同坐标系时尺度比为 1, 无需显式约束)。
//
// 日期: 2026-07-05
// ============================================================================

#include <vector>
#include "ipv_types.h"

namespace ipv {

// AT_MATCH_MINVOTES=2
// 投票数 < 2 的配对视为单票噪声, 直接丢弃, 不进入 top_pairs
static constexpr int AT_MATCH_MINVOTES = 2;

// ---------------------------------------------------------------------------
// 三角形描述符 (Valdes 1995 风格)
// 边长排序: a >= b >= c
// 描述符: ba = b/a, ca = c/a (基于边长比值, 翻转不变)
// a_index: 最长边 a 对面的顶点索引 (在 star array 中的索引, 0..N-1)
// b_index: 次长边 b 对面的顶点索引
// c_index: 最短边 c 对面的顶点索引
// ---------------------------------------------------------------------------
struct Triangle {
    int    a_index;     // 最长边对面的顶点索引 (在星表中的索引)
    int    b_index;     // 次长边对面的顶点索引
    int    c_index;     // 最短边对面的顶点索引
    double ba;          // b/a (次长边/最长边)
    double ca;          // c/a (最短边/最长边)
    double a_length;    // 最长边长度 (角秒)
};

// ---------------------------------------------------------------------------
// 三角形匹配结果
// ---------------------------------------------------------------------------
struct TriangleMatchResult {
    std::vector<MatchPair> top_pairs;   // 最高票匹配对 (按票数降序)
    std::vector<double>    votes;        // 每个匹配对的票数 (与 top_pairs 一一对应)
    int    n_triangles_A;                // A 侧三角形数
    int    n_triangles_B;                // B 侧三角形数
    int    max_vote;                     // 最大票数
    bool   success;
};

// ---------------------------------------------------------------------------
// 主入口: 三角形匹配
// 输入: U (图像侧星点, 像素坐标), W (星表侧星点, 角秒坐标)
// 输出: 匹配结果 (top_pairs 为初始匹配对, 供 iter_trans 使用)
//
// 参数:
// n_stars_A - A 侧使用星数 (: 默认 60)
// n_stars_B - B 侧使用星数 (: 默认 60)
// tolerance - 描述符匹配容差 (ba, ca 空间欧氏距离阈值, 默认 0.002)
// s0 - 像素尺度 (arcsec/pixel), 用于 scale 约束
// percent_scale_range=20%
// U 是像素, W 是角秒, 预期 ratio = U.a_length/W.a_length = 1/s0
// 允许范围 [1/(s0*1.2), 1/(s0*0.8)] (±20%)
// s0<=0 时不做 scale 约束 (兼容旧调用)
//
// 直接用 60 颗星, 不做自适应扩充
// 若星数 < 60, 内部截断到实际星数
// U/W 已按 flux 降序, 取前 N 颗即最亮 N 颗
// ---------------------------------------------------------------------------
TriangleMatchResult triangle_match(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    int n_stars_A = 60,        // 默认 60 颗
    int n_stars_B = 60,        // 默认 60 颗
    double tolerance = 0.002,  // 默认 0.002 (原 0.001 过严)
    double s0 = 0.0            // 像素尺度 (arcsec/pixel), 用于 scale 约束
);

// 初始化模块日志器 (写文件), 不调用则默认仅输出到 stderr
void init_triangle_logger(const std::string& path);

} // namespace ipv

#endif // IPV_TRIANGLE_H

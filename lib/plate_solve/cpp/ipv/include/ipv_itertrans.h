#ifndef IPV_ITERTRANS_H
#define IPV_ITERTRANS_H

// ============================================================================
// ipv_itertrans.h - iter_trans 多项式 TRANS 拟合模块
//
// 实现 sigma-clip 迭代多项式拟合:
//   - calc_trans: 最小二乘法求解线性/二次/三次 TRANS 系数
//   - iter_trans_inner: sigma-clip 迭代核心 (35% 百分位, HALT_SIGMA, nb==0)
//   - at_match_lists: 5px 半径全量贪心匹配
//   - at_recalc_trans: 用已有匹配对重拟合 (recalc=YES)
//   - iter_trans_solve: 主入口 (iter_trans → atMatchLists → atRecalcTrans)
//
// TRANS 模型 (U→W, U=图像侧像素, W=星表侧角秒):
//   x' = x00 + x10*x + x01*y + x20*x² + x11*x*y + x02*y² + x30*x³ + ...
//   y' = y00 + y10*x + y01*y + y20*x² + y11*x*y + y02*y² + y30*x³ + ...
//
// V4.20: TRANS: U → W (apply_trans(U) ≈ W)
//   U = 图像侧星点 (像素坐标, 原点图像中心)
//   W = 星表侧星点 (角秒坐标, gnomonic xi/eta)
//   常数项 x00/y00 单位: 角秒
//   线性项 x10/x01/y10/y01 单位: 角秒/像素
//   二次项 x20/x11/x02 单位: 角秒/像素²
//
// 系数排列:
//   x00, x10, x01, x20, x11, x02, x30, x21, x12, x03, ...
//
// 日期: 2026-07-05
// ============================================================================

#include <vector>
#include "ipv_types.h"

namespace ipv {

// ---------------------------------------------------------------------------
// 多项式 TRANS (最多 3 阶)
//
// order: 1=线性(6参数), 2=二次(12参数), 3=三次(20参数)
// 系数排列: x00, x10, x01, x20, x11, x02, x30, x21, x12, x03
// ---------------------------------------------------------------------------
struct Trans {
    int    order = 1;       // 1=线性, 2=二次, 3=三次

    // 线性项 (order >= 1)
    double x00 = 0, x10 = 0, x01 = 0;
    double y00 = 0, y10 = 0, y01 = 0;

    // 二次项 (order >= 2)
    double x20 = 0, x11 = 0, x02 = 0;
    double y20 = 0, y11 = 0, y02 = 0;

    // 三次项 (order >= 3)
    double x30 = 0, x21 = 0, x12 = 0, x03 = 0;
    double y30 = 0, y21 = 0, y12 = 0, y03 = 0;

    // 统计字段
    int    nr = 0;          // 用于拟合的对数
    int    nm = 0;          // 匹配上的对数
    double sig = 0;         // 残差标准差 (角秒, 68.3% 百分位)
    double sx = 0;          // X 方向标准差 (3-sigma 裁剪)
    double sy = 0;          // Y 方向标准差 (3-sigma 裁剪)
    bool   valid = false;   // 是否有效
};

// iter_trans 拟合结果
struct IterTransResult {
    Trans                   trans;        // 拟合得到的 TRANS
    std::vector<MatchPair>  inliers;      // 内点匹配对
    std::vector<double>     residuals;    // 每对残差 (角秒, 距离值)
    double rms = 0;                       // RMS (角秒)
    int    n_inliers = 0;                 // 内点数
    int    n_iterations = 0;              // iter_trans 迭代次数
    bool   success = false;               // 是否成功
};

// ---------------------------------------------------------------------------
// 主入口: atFindTrans 等价 (三角形匹配 → iter_trans → atMatchLists → atRecalcTrans)
//
// 输入:
//   U                - 图像侧星点 (像素坐标, 原点图像中心, Y 轴向上)
//   W                - 星表侧星点 (角秒坐标, gnomonic xi/eta)
//   initial_pairs    - 初始匹配对 (来自 triangle_match 的 top_pairs)
//   tolerance_arcsec - atMatchLists 匹配半径 (角秒, 默认 5.0)
//   order            - TRANS 阶数 (1=线性, 2=二次, 3=三次)
//
// 输出: IterTransResult (含 TRANS + inliers + 统计)
//
// 流程:
//   1. initial_pairs 前 start_pairs 对 → iter_trans_inner (RECALC_NO)
//   2. atMatchLists(U, W, TRANS, tolerance) → 全量匹配对
//   3. atRecalcTrans(U, W, 全量匹配对) → 精化 TRANS
//   4. (可选) 第二轮 atMatchLists + atRecalcTrans
// ---------------------------------------------------------------------------
IterTransResult iter_trans_solve(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& initial_pairs,
    double tolerance_arcsec = 5.0,
    int    order = 1
);

// ---------------------------------------------------------------------------
// atRecalcTrans: 用已有匹配对重拟合 TRANS (recalc=YES 模式)
//
// 用于已确定匹配索引后的重拟合, 不做 sigma-clip 剔除
// 输入: U, W, matched_pairs (已匹配对, 固定索引), order
// 输出: IterTransResult (含 TRANS + sig/sx/sy 统计)
// ---------------------------------------------------------------------------
IterTransResult at_recalc_trans(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& matched_pairs,
    int    order = 1
);

// ---------------------------------------------------------------------------
// atApplyTrans: 应用 TRANS 到单点 (V4.20: U → W, 像素 → 角秒)
// 输入: Trans, (ux, uy) 源坐标 (像素, 图像侧 U)
// 输出: (wx, wy) 变换后坐标 (角秒, 星表侧 W)
// ---------------------------------------------------------------------------
inline void apply_trans(const Trans& t, double ux, double uy,
                        double* wx, double* wy) {
    double tx = 0.0, ty = 0.0;
    if (t.order >= 1) {
        tx = t.x00 + t.x10 * ux + t.x01 * uy;
        ty = t.y00 + t.y10 * ux + t.y01 * uy;
    }
    if (t.order >= 2) {
        tx += t.x20 * ux * ux + t.x11 * ux * uy + t.x02 * uy * uy;
        ty += t.y20 * ux * ux + t.y11 * ux * uy + t.y02 * uy * uy;
    }
    if (t.order >= 3) {
        tx += t.x30 * ux * ux * ux + t.x21 * ux * ux * uy + t.x12 * ux * uy * uy + t.x03 * uy * uy * uy;
        ty += t.y30 * ux * ux * ux + t.y21 * ux * ux * uy + t.y12 * ux * uy * uy + t.y03 * uy * uy * uy;
    }
    *wx = tx;
    *wy = ty;
}

// ---------------------------------------------------------------------------
// atMatchLists: 全量匹配 (tolerance 半径最近邻贪心)
//
// 输入: U, W, TRANS (已拟合), tolerance_arcsec (匹配半径, 角秒)
// 输出: 匹配对列表
//
// 算法 (V4.20: U→W 方向):
//   1. 对 U 中每颗星应用 TRANS → W' (predicted W, 角秒坐标系)
//   2. 对 W 中每颗星, 找 W' 中最近邻, 距离 < tolerance_arcsec 则记录
//   3. 按距离升序排序, 贪心分配 (已匹配的星不再参与)
// ---------------------------------------------------------------------------
std::vector<MatchPair> at_match_lists(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const Trans& trans,
    double tolerance_arcsec = 5.0
);

// 初始化模块日志器 (写文件), 不调用则默认仅输出到 stderr
void init_itertrans_logger(const std::string& path);

} // namespace ipv

#endif // IPV_ITERTRANS_H

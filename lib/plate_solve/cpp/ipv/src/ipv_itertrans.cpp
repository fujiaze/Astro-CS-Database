// ============================================================================
// ipv_itertrans.cpp - iter_trans 多项式 TRANS 拟合模块实现
//
// 实现:
//   - calc_trans_general: 通用多项式最小二乘拟合 (order 1/2/3)
//   - iter_trans_inner: sigma-clip 迭代核心 (35% 百分位, HALT_SIGMA, nb==0)
//   - at_match_lists: V4.22 双向最近邻匹配 (U→W + W→U 互为最近邻) + 去重
//   - at_recalc_trans: 用已有匹配对重拟合 + sig/sx/sy 统计
//   - iter_trans_solve: 主入口 (iter_trans → atMatchLists → atRecalcTrans)
//
// 数据流 (V4.20: TRANS 方向 U->W):
//   U = 图像侧星点 (像素坐标, 原点图像中心, Y 轴向上)
//   W = 星表侧星点 (角秒坐标, gnomonic xi/eta)
//   TRANS: U → W (apply_trans(U) ≈ W)
//   MatchPair {u, w}: u 索引 U, w 索引 W
//
// 关键常量:
//   AT_MATCH_PERCENTILE = 0.35   (35% 百分位作为 sigma)
//   AT_MATCH_NSIGMA     = 10.0   (相对剔除阈值 = 10*sigma)
//   AT_MATCH_MAXDIST    = 50.0   (绝对剔除阈值, 角秒)
//   ONE_STDEV_PERCENTILE = 0.683 (1-sigma 百分位, 用于最终 sig)
//   AT_MATCH_REQUIRE_LINEAR = 3, AT_MATCH_STARTN_LINEAR = 6
//
// 日期: 2026-07-05
// ============================================================================

#include "ipv_itertrans.h"
#include "ipv_log.h"

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>

namespace ipv {

// ---------------------------------------------------------------------------
// 常量定义
// ---------------------------------------------------------------------------
static constexpr int    AT_MATCH_REQUIRE_LINEAR    = 3;
static constexpr int    AT_MATCH_STARTN_LINEAR     = 6;
static constexpr int    AT_MATCH_REQUIRE_QUADRATIC = 6;
static constexpr int    AT_MATCH_STARTN_QUADRATIC  = 12;
static constexpr int    AT_MATCH_REQUIRE_CUBIC     = 10;
static constexpr int    AT_MATCH_STARTN_CUBIC      = 20;
static constexpr double AT_MATCH_PERCENTILE        = 0.35;
static constexpr double AT_MATCH_NSIGMA            = 10.0;
static constexpr double AT_MATCH_MAXDIST           = 50.0;    // V4.20: 单位角秒, W 是角秒
static constexpr double ONE_STDEV_PERCENTILE       = 0.683;
static constexpr int    RECALC_YES                 = 1;
static constexpr int    RECALC_NO                  = 0;

// ---------------------------------------------------------------------------
// 模块级日志器
// ---------------------------------------------------------------------------
static Logger g_itertrans_logger;

Logger& itertrans_logger() {
    return g_itertrans_logger;
}

void init_itertrans_logger(const std::string& path) {
    g_itertrans_logger.init(path);
}

// ===========================================================================
// 内部工具函数
// ===========================================================================

// ---------------------------------------------------------------------------
// find_percentile: 百分位查找
// 输入: 已排序数组, 元素数 num, 百分位 perc (0, 1]
// 返回: array[floor(num * perc + 0.5)] (四舍五入取整)
// ---------------------------------------------------------------------------
static double find_percentile(const std::vector<double>& sorted_arr,
                              int num, double perc) {
    if (num <= 0) return 0.0;
    int index = (int)std::floor((double)num * perc + 0.5);
    if (index >= num) index = num - 1;
    if (index < 0) index = 0;
    return sorted_arr[index];
}

// ---------------------------------------------------------------------------
// gauss_solve: N×N 高斯消元法解线性方程组 (带部分主元选取)
//
// 输入: A (n×n 矩阵, row-major), b (n 向量)
// 输出: 解存储在 b 中 (原地修改), A 被消元过程修改
// 返回: true=成功, false=奇异矩阵
// ---------------------------------------------------------------------------
static bool gauss_solve(std::vector<std::vector<double>>& A,
                        std::vector<double>& b) {
    int n = (int)b.size();
    for (int col = 0; col < n; col++) {
        // 部分主元选取: 找当前列绝对值最大的行
        int max_row = col;
        double max_val = std::abs(A[col][col]);
        for (int row = col + 1; row < n; row++) {
            if (std::abs(A[row][col]) > max_val) {
                max_val = std::abs(A[row][col]);
                max_row = row;
            }
        }
        if (max_val < 1e-15) {
            return false;  // 奇异矩阵
        }
        // 交换行
        if (max_row != col) {
            std::swap(A[col], A[max_row]);
            std::swap(b[col], b[max_row]);
        }
        // 消元
        for (int row = col + 1; row < n; row++) {
            double factor = A[row][col] / A[col][col];
            A[row][col] = 0.0;
            for (int j = col + 1; j < n; j++) {
                A[row][j] -= factor * A[col][j];
            }
            b[row] -= factor * b[col];
        }
    }
    // 回代
    for (int row = n - 1; row >= 0; row--) {
        double sum = b[row];
        for (int j = row + 1; j < n; j++) {
            sum -= A[row][j] * b[j];
        }
        b[row] = sum / A[row][row];
    }
    return true;
}

// ---------------------------------------------------------------------------
// monomial_basis: 生成多项式单项式基
// 返回 (i, j) 对列表, i+j <= order, 按 (i+j 升序, i 降序) 排列
// 系数排列: x00, x10, x01, x20, x11, x02, x30, x21, x12, x03
// ---------------------------------------------------------------------------
static std::vector<std::pair<int,int>> monomial_basis(int order) {
    std::vector<std::pair<int,int>> basis;
    for (int deg = 0; deg <= order; deg++) {
        for (int i = deg; i >= 0; i--) {
            int j = deg - i;
            basis.push_back({i, j});
        }
    }
    return basis;
}

// ---------------------------------------------------------------------------
// eval_monomial: 计算 x^i * y^j
// ---------------------------------------------------------------------------
static double eval_monomial(double x, double y, int i, int j) {
    double v = 1.0;
    for (int k = 0; k < i; k++) v *= x;
    for (int k = 0; k < j; k++) v *= y;
    return v;
}

// ---------------------------------------------------------------------------
// pack_trans: 将系数数组打包到 Trans 结构
// 系数排列: [x00, x10, x01, x20, x11, x02, x30, x21, x12, x03, ...]
// ---------------------------------------------------------------------------
static void pack_trans(const std::vector<double>& xc,
                       const std::vector<double>& yc,
                       int order, Trans& t) {
    t.order = order;
    // 线性项 (order >= 1)
    t.x00 = xc[0]; t.x10 = xc[1]; t.x01 = xc[2];
    t.y00 = yc[0]; t.y10 = yc[1]; t.y01 = yc[2];
    // 二次项 (order >= 2)
    if (order >= 2) {
        t.x20 = xc[3]; t.x11 = xc[4]; t.x02 = xc[5];
        t.y20 = yc[3]; t.y11 = yc[4]; t.y02 = yc[5];
    }
    // 三次项 (order >= 3)
    if (order >= 3) {
        t.x30 = xc[6]; t.x21 = xc[7]; t.x12 = xc[8]; t.x03 = xc[9];
        t.y30 = yc[6]; t.y21 = yc[7]; t.y12 = yc[8]; t.y03 = yc[9];
    }
}

// ---------------------------------------------------------------------------
// calc_trans_general: 通用多项式 TRANS 最小二乘拟合
//
// 求解正规方程 M * c = b, 其中:
//   M[a][b] = Σ basis[a](x,y) * basis[b](x,y)
//   b[a]    = Σ basis[a](x,y) * target
//
// 输入: U (源, 像素), W (目标, 角秒), pairs (匹配对), order
// 输出: trans (拟合结果)
// 返回: true=成功, false=失败
//
// 数据流 (V4.20: TRANS 将 U 坐标变换到 W 坐标系):
//   s1 = U[mp.u] (源, 像素, 输入到 TRANS)
//   s2 = W[mp.w] (目标, 角秒, TRANS 输出应匹配)
// ---------------------------------------------------------------------------
static bool calc_trans_general(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& pairs,
    int order,
    Trans& trans
) {
    auto basis = monomial_basis(order);
    int n = (int)basis.size();  // 每个坐标的系数数 (order=1:3, order=2:6, order=3:10)

    if ((int)pairs.size() < n) {
        g_itertrans_logger.warnf("calc_trans_general: 对数 %zu < 所需 %d (order=%d)",
                                  pairs.size(), n, order);
        return false;
    }

    // 构建正规方程: M * c = b
    std::vector<std::vector<double>> M(n, std::vector<double>(n, 0.0));
    std::vector<double> bx(n, 0.0), by(n, 0.0);

    for (const auto& mp : pairs) {
        // 边界检查
        if (mp.u < 0 || mp.u >= (int)U.size() ||
            mp.w < 0 || mp.w >= (int)W.size()) {
            continue;
        }
        const StarPoint& s1 = U[mp.u];  // 源 (像素)
        const StarPoint& s2 = W[mp.w];  // 目标 (角秒)

        // 计算所有基函数值
        std::vector<double> bv(n);
        for (int k = 0; k < n; k++) {
            bv[k] = eval_monomial(s1.x, s1.y, basis[k].first, basis[k].second);
        }

        // 累加正规方程
        for (int a = 0; a < n; a++) {
            for (int b = 0; b < n; b++) {
                M[a][b] += bv[a] * bv[b];
            }
            bx[a] += bv[a] * s2.x;
            by[a] += bv[a] * s2.y;
        }
    }

    // 求解 M * xc = bx (X 系数)
    std::vector<double> xc = bx;
    std::vector<std::vector<double>> M_x = M;
    if (!gauss_solve(M_x, xc)) {
        g_itertrans_logger.warn("calc_trans_general: X 系数求解失败 (奇异矩阵)");
        return false;
    }

    // 求解 M * yc = by (Y 系数)
    std::vector<double> yc = by;
    std::vector<std::vector<double>> M_y = M;
    if (!gauss_solve(M_y, yc)) {
        g_itertrans_logger.warn("calc_trans_general: Y 系数求解失败 (奇异矩阵)");
        return false;
    }

    // 打包到 Trans 结构
    pack_trans(xc, yc, order, trans);
    trans.valid = true;

    g_itertrans_logger.debugf("calc_trans_general: order=%d, n_pairs=%zu, 拟合成功",
                               order, pairs.size());
    return true;
}

// ---------------------------------------------------------------------------
// compute_stddev_clipped: 带单次 3-sigma 裁剪的标准差
//   "A single iteration of 3-sigma clipping is used in the calculation."
// ---------------------------------------------------------------------------
static double compute_stddev_clipped(const std::vector<double>& values,
                                     double n_sigma) {
    if (values.empty()) return 0.0;
    int n = (int)values.size();

    // 第一轮: 计算均值和标准差
    double mean = 0.0;
    for (double v : values) mean += v;
    mean /= n;

    double var = 0.0;
    for (double v : values) var += (v - mean) * (v - mean);
    var /= n;
    double sd = std::sqrt(var);

    if (sd < 1e-15) return 0.0;

    // 3-sigma 裁剪 (单次迭代)
    double lo = mean - n_sigma * sd;
    double hi = mean + n_sigma * sd;

    double clipped_sum = 0.0;
    int clipped_n = 0;
    for (double v : values) {
        if (v >= lo && v <= hi) {
            clipped_sum += v;
            clipped_n++;
        }
    }
    if (clipped_n == 0) return sd;

    double clipped_mean = clipped_sum / clipped_n;
    double clipped_var = 0.0;
    for (double v : values) {
        if (v >= lo && v <= hi) {
            clipped_var += (v - clipped_mean) * (v - clipped_mean);
        }
    }
    clipped_var /= clipped_n;
    return std::sqrt(clipped_var);
}

// ===========================================================================
// iter_trans_inner: sigma-clip 迭代核心
// ===========================================================================

// ---------------------------------------------------------------------------
// iter_trans_inner: sigma-clip 迭代拟合 TRANS
//
// 流程:
//   1. 初始拟合 (recalc_flag=RECALC_NO 用前 start_pairs 对, RECALC_YES 用全部)
//   2. 循环 (最多 max_iterations 次):
//      a. 应用 TRANS, 计算每对 dist²
//      b. 绝对阈值剔除: dist² > MAXDIST²
//      c. sigma = 35% 百分位 (find_percentile)
//      d. HALT_SIGMA: sigma <= halt_sigma → is_ok=true (不退出)
//      e. V4.20 相对阈值剔除: dist² > 10*sigma
//      f. nb==0 → is_ok=true (不退出, 重拟合后退出)
//      g. nr < required_pairs → 失败
//      h. 重拟合
//      i. is_ok → 退出
//   3. 最终 sig = 68.3% 百分位
//
// 输入: U, W, initial_pairs, recalc_flag, max_iterations, halt_sigma, tolerance, order
//   (注: V4.20 后 tolerance 参数保留用于接口兼容, 相对阈值不再使用 (5*tolerance)²)
// 输出: IterTransResult (含 TRANS + inliers + 残差 + 统计)
// ---------------------------------------------------------------------------
static IterTransResult iter_trans_inner(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& initial_pairs,
    int    recalc_flag,
    int    max_iterations,
    double halt_sigma,
    double tolerance,
    int    order
) {
    IterTransResult result;
    result.success = false;

    // 根据阶数设置 required_pairs / start_pairs
    int required_pairs, start_pairs;
    switch (order) {
        case 1:
            required_pairs = AT_MATCH_REQUIRE_LINEAR;
            start_pairs    = AT_MATCH_STARTN_LINEAR;
            break;
        case 2:
            required_pairs = AT_MATCH_REQUIRE_QUADRATIC;
            start_pairs    = AT_MATCH_STARTN_QUADRATIC;
            break;
        case 3:
            required_pairs = AT_MATCH_REQUIRE_CUBIC;
            start_pairs    = AT_MATCH_STARTN_CUBIC;
            break;
        default:
            g_itertrans_logger.warnf("iter_trans_inner: 无效阶数 %d", order);
            return result;
    }

    if ((int)initial_pairs.size() < required_pairs) {
        g_itertrans_logger.warnf("iter_trans_inner: 初始对数 %zu < 所需 %d",
                                  initial_pairs.size(), required_pairs);
        return result;
    }

    // 初始工作集
    std::vector<MatchPair> working;
    if (recalc_flag == RECALC_YES) {
        working = initial_pairs;  // 使用全部
    } else {
        int n = std::min((int)initial_pairs.size(), start_pairs);
        working.assign(initial_pairs.begin(), initial_pairs.begin() + n);
    }

    if ((int)working.size() < required_pairs) {
        g_itertrans_logger.warnf("iter_trans_inner: 工作集 %zu < 所需 %d",
                                  working.size(), required_pairs);
        return result;
    }

    // 初始拟合 (用前 start_pairs 对)
    Trans trans;
    trans.order = order;
    if (!calc_trans_general(U, W, working, order, trans)) {
        g_itertrans_logger.warn("iter_trans_inner: 初始 calc_trans 失败");
        return result;
    }

    g_itertrans_logger.infof("iter_trans_inner: 初始拟合完成, 初始工作集=%zu, order=%d",
                              working.size(), order);

    // V4.23 调试: 打印初始 TRANS 和前 6 对的残差
    {
        g_itertrans_logger.infof("  [调试] 初始 TRANS: x00=%.4f y00=%.4f, x10=%.6f x01=%.6f y10=%.6f y01=%.6f",
                                 trans.x00, trans.y00, trans.x10, trans.x01, trans.y10, trans.y01);
        for (size_t i = 0; i < working.size() && i < 6; ++i) {
            const StarPoint& u = U[working[i].u];
            const StarPoint& w = W[working[i].w];
            double wp, wpy;
            apply_trans(trans, u.x, u.y, &wp, &wpy);
            double dx = wp - w.x;
            double dy = wpy - w.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            g_itertrans_logger.infof("  [调试] 初始对[%zu]: u=%d w=%d, U=(%.2f,%.2f), W=(%.2f,%.2f), "
                                     "pred=(%.2f,%.2f), dist=%.4f\" (dx=%.2f dy=%.2f)",
                                     i, working[i].u, working[i].w,
                                     u.x, u.y, w.x, w.y, wp, wpy, dist, dx, dy);
        }
    }

    // V4.23: 工作集扩展 (nr = nbright)
    // 用 start_pairs 对做首次 calc_trans, 然后立即将 nr 扩展为 nbright (全部对) 进入迭代
    // 之前 BUG: working 始终只有 start_pairs(6) 对, 6 对中 4 对被剔除后剩 2 < 3 required → 失败
    // 修复: 初始拟合成功后, 将 working 扩展为全部 initial_pairs, 迭代在全部对上做 sigma-clip
    if (recalc_flag == RECALC_NO) {
        size_t before = working.size();
        working = initial_pairs;
        g_itertrans_logger.infof("iter_trans_inner: V4.23 工作集扩展 %zu->%zu (nr=nbright)",
                                  before, working.size());
    }

    // sigma-clip 迭代
    bool is_ok = true;
    int iters_so_far = 0;
    double max_dist2 = AT_MATCH_MAXDIST * AT_MATCH_MAXDIST;

    while (iters_so_far < max_iterations) {
        int nr = (int)working.size();
        int nb = 0;

        // V4.20: 残差 = apply_trans(U) - W (U=像素, W=角秒)
        std::vector<double> dist2(nr), dist2_sorted(nr);
        for (int i = 0; i < nr; i++) {
            // V4.21 边界检查
            if (working[i].u < 0 || working[i].u >= (int)U.size() ||
                working[i].w < 0 || working[i].w >= (int)W.size()) {
                g_itertrans_logger.warnf("iter_trans_inner: 索引越界 i=%d, u=%d (U.size=%zu), "
                                          "w=%d (W.size=%zu), 跳过",
                                          i, working[i].u, U.size(),
                                          working[i].w, W.size());
                dist2[i] = 0.0;
                dist2_sorted[i] = 0.0;
                continue;
            }
            const StarPoint& u = U[working[i].u];
            double wp, wpy;
            apply_trans(trans, u.x, u.y, &wp, &wpy);
            const StarPoint& w = W[working[i].w];
            double dx = wp - w.x;
            double dy = wpy - w.y;
            dist2[i] = dx * dx + dy * dy;
            dist2_sorted[i] = dist2[i];
        }
        std::sort(dist2_sorted.begin(), dist2_sorted.end());

        // --- 绝对阈值剔除: dist² > MAXDIST² ---
        std::vector<MatchPair> surviving;
        std::vector<double> surviving_dist2;
        for (int i = 0; i < nr; i++) {
            if (dist2[i] > max_dist2) {
                nb++;
            } else {
                surviving.push_back(working[i]);
                surviving_dist2.push_back(dist2[i]);
            }
        }
        working = surviving;
        int new_nr = (int)working.size();

        g_itertrans_logger.debugf("iter %d: 绝对剔除 nr=%d→%d, nb=%d",
                                   iters_so_far, nr, new_nr, nb);

        // --- V4.28: tol 预过滤 (防止 sigma(35%) 被 5-50" 中等错配拉大) ---
        // 问题: workset 扩展 6→60 后, 绝对剔除(50")后仍有 dist 在 5-50" 的中等错配,
        //        这些错配拉大 sigma(35%), 导致相对阈值 10*sigma 失效 (rel_thresh 远大于 tol)
        // 修复: 第一次迭代时, 在 sigma 计算前用 tolerance 预过滤, 只保留 dist < tol 的对
        //       条件: 预过滤后剩余对数 >= required_pairs 且确实剔除了对
        if (iters_so_far == 0 && new_nr > required_pairs) {
            double tol2 = tolerance * tolerance;
            std::vector<MatchPair> tol_surviving;
            std::vector<double> tol_surviving_dist2;
            int tol_nb = 0;
            for (int i = 0; i < new_nr; i++) {
                if (surviving_dist2[i] > tol2) {
                    tol_nb++;
                } else {
                    tol_surviving.push_back(working[i]);
                    tol_surviving_dist2.push_back(surviving_dist2[i]);
                }
            }
            // 只在预过滤后剩余对数 >= required_pairs 且确实剔除了对时执行
            if ((int)tol_surviving.size() >= required_pairs && tol_nb > 0) {
                int old_nr = new_nr;
                working = tol_surviving;
                surviving_dist2 = tol_surviving_dist2;
                new_nr = (int)working.size();
                nb += tol_nb;
                // 重建 dist2_sorted (只含 tol 预过滤后的 surviving)
                dist2_sorted.assign(surviving_dist2.begin(), surviving_dist2.end());
                std::sort(dist2_sorted.begin(), dist2_sorted.end());
                g_itertrans_logger.infof("iter %d: V4.28 tol 预过滤 %d→%d (tol=%.2f\", 剔除中等错配 %d 对)",
                                          iters_so_far, old_nr, new_nr, tolerance, tol_nb);
            }
        }

        // --- 计算 sigma (35% 百分位) ---
        // dist2_sorted 在剔除前排序, 剔除的都是大值 (在排序数组末尾)
        // 所以 dist2_sorted[0..new_nr-1] 仍是 surviving 的有序子集
        double sigma;
        if (new_nr < 2) {
            if (new_nr == 0) {
                g_itertrans_logger.warn("iter_trans_inner: 所有对被绝对阈值剔除");
                return result;
            }
            sigma = 0.0;
        } else {
            sigma = find_percentile(dist2_sorted, new_nr, AT_MATCH_PERCENTILE);
        }

        g_itertrans_logger.debugf("iter %d: sigma(35%%)=%.6f 角秒²", iters_so_far, sigma);

        // --- V4.28: sigma 钳制 (防止 sigma 被 5-50" 中等错配拉大导致相对阈值失效) ---
        // 问题: workset 60 对中存在 39-41 对错配, 绝对剔除(50")后仍有 5-50" 中等错配,
        //        这些错配拉大 sigma(35%) 到 7.91-139.06 角秒² (正常帧 sigma≈1-3)
        //        导致相对阈值 10*sigma = 79-1390 角秒² (8.94-37.28"), 远大于 tol=5", 无法清除中等错配
        // 修复: 当 sigma > tolerance² 时, 钳制为 tolerance², 使相对阈值从 10*sigma 降为 10*tolerance²
        //        对于 tol=5": rel_thresh 从 1390(37.3") 降为 250(15.8"), 能剔除 dist>15.8" 的中等错配
        //        注: tol 预过滤未触发时 (初始拟合质量差, tol 过滤后剩余 < required_pairs), sigma 钳制作为兜底
        if (sigma > tolerance * tolerance && tolerance > 0) {
            g_itertrans_logger.infof("iter %d: V4.28 sigma 钳制 %.6f → %.6f (tol=%.2f\", rel_thresh %.6f→%.6f)",
                                      iters_so_far, sigma, tolerance * tolerance,
                                      tolerance,
                                      AT_MATCH_NSIGMA * sigma,
                                      AT_MATCH_NSIGMA * tolerance * tolerance);
            sigma = tolerance * tolerance;
        }

        // --- HALT_SIGMA 检查 ---
        if (sigma <= halt_sigma) {
            is_ok = true;
            // 不退出, 继续剔除+重拟合 (break 被注释掉)
        }

        // --- V4.20: 相对阈值只用 NSIGMA*sigma (不再 min with (5*tau)²) ---
        double rel_threshold = AT_MATCH_NSIGMA * sigma;

        surviving.clear();
        for (int i = 0; i < new_nr; i++) {
            if (surviving_dist2[i] > rel_threshold) {
                nb++;
            } else {
                surviving.push_back(working[i]);
            }
        }
        working = surviving;
        new_nr = (int)working.size();

        g_itertrans_logger.debugf("iter %d: 相对剔除 nr=%d, nb=%d, rel_thresh=%.6f",
                                   iters_so_far, new_nr, nb, rel_threshold);

        // --- nb==0: 没有需剔除的, 设 is_ok=true ---
        if (nb == 0) {
            is_ok = true;
            // 不退出, 重拟合后退出 (break 被注释掉)
        }

        // --- nr < required_pairs: 失败 ---
        if (new_nr < required_pairs) {
            g_itertrans_logger.warnf("iter_trans_inner: 剩余 %d < 所需 %d, 失败",
                                      new_nr, required_pairs);
            is_ok = false;
            break;
        }

        // --- 重拟合 ---
        if (!calc_trans_general(U, W, working, order, trans)) {
            g_itertrans_logger.warn("iter_trans_inner: 重拟合 calc_trans 失败");
            return result;
        }

        iters_so_far++;
        if (is_ok) break;
    }

    // --- 最终统计 ---
    int nr = (int)working.size();
    trans.nr = nr;

    // 计算最终残差 (用最终 TRANS 重新计算, V4.20: apply_trans(U) - W)
    std::vector<double> final_dist2(nr);
    std::vector<double> final_dx(nr), final_dy(nr);
    double sum_d2 = 0.0;
    for (int i = 0; i < nr; i++) {
        // V4.21 边界检查
        if (working[i].u < 0 || working[i].u >= (int)U.size() ||
            working[i].w < 0 || working[i].w >= (int)W.size()) {
            g_itertrans_logger.warnf("iter_trans_inner: 索引越界 i=%d, u=%d (U.size=%zu), "
                                      "w=%d (W.size=%zu), 跳过",
                                      i, working[i].u, U.size(),
                                      working[i].w, W.size());
            final_dist2[i] = 0.0;
            final_dx[i] = 0.0;
            final_dy[i] = 0.0;
            continue;
        }
        const StarPoint& u = U[working[i].u];
        double wp, wpy;
        apply_trans(trans, u.x, u.y, &wp, &wpy);
        const StarPoint& w = W[working[i].w];
        double dx = wp - w.x;
        double dy = wpy - w.y;
        final_dist2[i] = dx * dx + dy * dy;
        final_dx[i] = dx;
        final_dy[i] = dy;
        sum_d2 += final_dist2[i];
    }

    // sig = 68.3% 百分位 (ONE_STDEV_PERCENTILE)
    if (nr > 1) {
        std::vector<double> d2_sorted = final_dist2;
        std::sort(d2_sorted.begin(), d2_sorted.end());
        trans.sig = find_percentile(d2_sorted, nr, ONE_STDEV_PERCENTILE);
    } else {
        trans.sig = 0.0;
    }
    trans.nm = nr;
    trans.valid = true;

    // 填充结果
    result.trans = trans;
    result.inliers = working;
    result.residuals.resize(nr);
    for (int i = 0; i < nr; i++) {
        result.residuals[i] = std::sqrt(final_dist2[i]);
    }
    result.rms = (nr > 0) ? std::sqrt(sum_d2 / nr) : 0.0;
    result.n_inliers = nr;
    result.n_iterations = iters_so_far;
    result.success = (nr >= required_pairs);

    g_itertrans_logger.infof("iter_trans_inner: 完成, nr=%d, sig=%.4f, rms=%.4f, iters=%d, success=%d",
                              nr, trans.sig, result.rms, result.n_iterations,
                              result.success ? 1 : 0);

    return result;
}

// ===========================================================================
// 公开接口实现
// ===========================================================================

// ---------------------------------------------------------------------------
// at_match_lists: V4.22 双向最近邻匹配 + 去重
//
// 算法 (V4.22: 双向匹配, 替代 V4.20 单向贪心):
//   1. 对 U 中每颗星应用 TRANS → U_pred (predicted W, 角秒坐标系)
//   2. U→W (A→B): 对每个 U[i], 在 W 中找最近邻 W[j_i], 距离 ≤ tolerance 才记录
//   3. W→U (B→A): 对每个 W[j], 在 U_pred 中找最近邻 U_pred[i_j], 距离 ≤ tolerance 才记录
//   4. 双向配对成立: A→B 的 (i, j_i) 与 B→A 的 (i_j, j) 互为最近邻, 即 i_of[j_of[i]] == i
//   5. remove_repeated_elements: 双向匹配理论上已保证无重复, 此处再校验去重并输出日志
//
// 注: TRANS 方向保持 V4.20 (U→W), W→U 方向通过遍历 U_pred 找最近邻实现
//     (不计算 trans 的逆, 简化实现, 与任务描述简化方案一致)
// ---------------------------------------------------------------------------
std::vector<MatchPair> at_match_lists(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const Trans& trans,
    double tolerance_arcsec
) {
    std::vector<MatchPair> matches;

    if (U.empty() || W.empty()) {
        return matches;
    }

    double tol2 = tolerance_arcsec * tolerance_arcsec;

    // 1. 对 U 中每颗星应用 TRANS → U_pred (predicted W, 角秒)
    std::vector<std::pair<double, double>> U_pred(U.size());
    for (size_t i = 0; i < U.size(); i++) {
        apply_trans(trans, U[i].x, U[i].y, &U_pred[i].first, &U_pred[i].second);
    }

    // V4.21 大小一致性检查
    if (U_pred.size() != U.size()) {
        g_itertrans_logger.warnf("at_match_lists: U_pred.size=%zu != U.size=%zu",
                                  U_pred.size(), U.size());
    }

    // 2. U→W (A→B): 对每个 U[i], 在 W 中找最近邻 W[j_i]
    //    j_of[i] = U[i] 的最近邻 W 索引 (距离 ≤ tolerance 才有效, 否则 -1)
    std::vector<int> j_of(U.size(), -1);
    int n_AB = 0;  // U→W 单向匹配数 (距离 ≤ tolerance)

    for (size_t i = 0; i < U.size(); i++) {
        double best_d2 = -1.0;
        int    best_j  = -1;
        for (size_t j = 0; j < W.size(); j++) {
            double dx = U_pred[i].first  - W[j].x;
            double dy = U_pred[i].second - W[j].y;
            double d2 = dx * dx + dy * dy;
            if (best_j < 0 || d2 < best_d2) {
                best_d2 = d2;
                best_j  = (int)j;
            }
        }
        if (best_j >= 0 && best_d2 <= tol2) {
            j_of[i] = best_j;
            n_AB++;
        }
    }

    // 3. W→U (B→A): 对每个 W[j], 在 U_pred 中找最近邻 U_pred[i_j]
    //    i_of[j] = W[j] 的最近邻 U 索引 (距离 ≤ tolerance 才有效, 否则 -1)
    std::vector<int> i_of(W.size(), -1);
    int n_BA = 0;  // W→U 单向匹配数 (距离 ≤ tolerance)

    for (size_t j = 0; j < W.size(); j++) {
        double best_d2 = -1.0;
        int    best_i  = -1;
        for (size_t i = 0; i < U_pred.size(); i++) {
            double dx = U_pred[i].first  - W[j].x;
            double dy = U_pred[i].second - W[j].y;
            double d2 = dx * dx + dy * dy;
            if (best_i < 0 || d2 < best_d2) {
                best_d2 = d2;
                best_i  = (int)i;
            }
        }
        if (best_i >= 0 && best_d2 <= tol2) {
            i_of[j] = best_i;
            n_BA++;
        }
    }

    // 4. 双向配对成立: A→B 的 (i, j_i) 与 B→A 的 (i_j, j) 互为最近邻
    //    即 i_of[j_of[i]] == i
    std::vector<MatchPair> bidir_matches;
    for (size_t i = 0; i < U.size(); i++) {
        int j = j_of[i];
        if (j < 0) continue;             // U→W 未匹配
        if (i_of[j] < 0) continue;       // W→U 未匹配 (距离超阈值)
        if (i_of[j] == (int)i) {
            // 互为最近邻, 配对成立
            bidir_matches.push_back({(int)i, j});
        }
    }

    // 5. remove_repeated_elements: 双向匹配理论上已保证无重复
    //    (每个 U[i] 只对应一个 j_of[i], 每个 W[j] 只对应一个 i_of[j],
    //     互为最近邻的配对中同一 U/W 不会出现两次)
    //    此处再校验去重, 并输出日志
    std::vector<bool> u_seen(U.size(), false);
    std::vector<bool> w_seen(W.size(), false);
    int n_dedup = 0;
    for (const auto& mp : bidir_matches) {
        if (!u_seen[mp.u] && !w_seen[mp.w]) {
            matches.push_back(mp);
            u_seen[mp.u] = true;
            w_seen[mp.w] = true;
            n_dedup++;
        }
    }

    g_itertrans_logger.infof("at_match_lists: U=%zu, W=%zu, tol=%.2f\", "
                              "U→W单向=%d, W→U单向=%d, 双向=%zu, 去重后=%d",
                              U.size(), W.size(), tolerance_arcsec,
                              n_AB, n_BA, bidir_matches.size(), n_dedup);

    return matches;
}

// ---------------------------------------------------------------------------
// at_recalc_trans: 用已有匹配对重拟合 TRANS (recalc=YES 模式)
//
// 流程:
//   - 用全部匹配对拟合 TRANS (不剔除, 因为已经是内点)
//   - 计算 sig (68.3% 百分位), sx/sy (3-sigma 裁剪标准差)
// ---------------------------------------------------------------------------
IterTransResult at_recalc_trans(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& matched_pairs,
    int    order
) {
    IterTransResult result;
    result.success = false;

    // 根据阶数设置最小对数
    int min_pairs;
    switch (order) {
        case 1: min_pairs = AT_MATCH_REQUIRE_LINEAR;    break;
        case 2: min_pairs = AT_MATCH_REQUIRE_QUADRATIC; break;
        case 3: min_pairs = AT_MATCH_REQUIRE_CUBIC;     break;
        default:
            g_itertrans_logger.warnf("at_recalc_trans: 无效阶数 %d", order);
            return result;
    }

    if ((int)matched_pairs.size() < min_pairs) {
        g_itertrans_logger.warnf("at_recalc_trans: 匹配对数 %zu < 所需 %d",
                                  matched_pairs.size(), min_pairs);
        return result;
    }

    // 用全部匹配对拟合 TRANS (不剔除)
    Trans trans;
    trans.order = order;
    if (!calc_trans_general(U, W, matched_pairs, order, trans)) {
        g_itertrans_logger.warn("at_recalc_trans: calc_trans 失败");
        return result;
    }

    // 计算残差 (V4.20: apply_trans(U) - W, U=像素, W=角秒)
    int n = (int)matched_pairs.size();
    std::vector<double> dist2(n), dx_arr(n), dy_arr(n);
    double sum_d2 = 0.0;
    for (int i = 0; i < n; i++) {
        // V4.21 边界检查
        if (matched_pairs[i].u < 0 || matched_pairs[i].u >= (int)U.size() ||
            matched_pairs[i].w < 0 || matched_pairs[i].w >= (int)W.size()) {
            g_itertrans_logger.warnf("at_recalc_trans: 索引越界 i=%d, u=%d (U.size=%zu), "
                                      "w=%d (W.size=%zu), 跳过",
                                      i, matched_pairs[i].u, U.size(),
                                      matched_pairs[i].w, W.size());
            dist2[i] = 0.0;
            dx_arr[i] = 0.0;
            dy_arr[i] = 0.0;
            continue;
        }
        const StarPoint& u = U[matched_pairs[i].u];
        double wp, wpy;
        apply_trans(trans, u.x, u.y, &wp, &wpy);
        const StarPoint& w = W[matched_pairs[i].w];
        double dx = wp - w.x;
        double dy = wpy - w.y;
        dist2[i] = dx * dx + dy * dy;
        dx_arr[i] = dx;
        dy_arr[i] = dy;
        sum_d2 += dist2[i];
    }

    // 统计字段
    trans.nr = n;
    trans.nm = n;

    // sig = 68.3% 百分位 (ONE_STDEV_PERCENTILE)
    std::vector<double> dist2_sorted = dist2;
    std::sort(dist2_sorted.begin(), dist2_sorted.end());
    trans.sig = (n > 1) ? find_percentile(dist2_sorted, n, ONE_STDEV_PERCENTILE) : 0.0;

    // sx, sy = 3-sigma 裁剪标准差
    trans.sx = compute_stddev_clipped(dx_arr, 3.0);
    trans.sy = compute_stddev_clipped(dy_arr, 3.0);

    // recalc=YES 二轮: sigma-clip 离群点后重拟合
    // sig 是 dist² 的 68.3% 百分位 (≈1-sigma²), 3*sqrt(sig) 为 3-sigma 裁剪阈值
    bool sigma_clipped = false;
    if (trans.sig > 0 && n > min_pairs) {
        double clip_thresh = 3.0 * std::sqrt(trans.sig);
        double clip_thresh2 = clip_thresh * clip_thresh;

        std::vector<MatchPair> clipped_pairs;
        clipped_pairs.reserve(n);
        for (int i = 0; i < n; i++) {
            if (dist2[i] <= clip_thresh2) {
                clipped_pairs.push_back(matched_pairs[i]);
            }
        }

        int n_clipped_out = n - (int)clipped_pairs.size();
        if ((int)clipped_pairs.size() >= min_pairs && n_clipped_out > 0) {
            g_itertrans_logger.infof("at_recalc_trans: 二轮 sigma-clip (thresh=%.3f\", 剔除=%d, 保留=%zu)",
                                      clip_thresh, n_clipped_out, clipped_pairs.size());
            // 二轮: 用裁剪后的对重拟合
            Trans trans2;
            trans2.order = order;
            if (calc_trans_general(U, W, clipped_pairs, order, trans2)) {
                trans = trans2;
                trans.nr = (int)clipped_pairs.size();
                trans.nm = (int)clipped_pairs.size();

                // 重新计算残差 (用 clipped_pairs + 新 trans)
                int n2 = (int)clipped_pairs.size();
                std::vector<double> dist2_2(n2), dx_arr2(n2), dy_arr2(n2);
                double sum_d2_2 = 0.0;
                for (int i = 0; i < n2; i++) {
                    if (clipped_pairs[i].u < 0 || clipped_pairs[i].u >= (int)U.size() ||
                        clipped_pairs[i].w < 0 || clipped_pairs[i].w >= (int)W.size()) {
                        dist2_2[i] = 0.0;
                        dx_arr2[i] = 0.0;
                        dy_arr2[i] = 0.0;
                        continue;
                    }
                    const StarPoint& u = U[clipped_pairs[i].u];
                    double wp, wpy;
                    apply_trans(trans, u.x, u.y, &wp, &wpy);
                    const StarPoint& w = W[clipped_pairs[i].w];
                    double dx = wp - w.x;
                    double dy = wpy - w.y;
                    dist2_2[i] = dx * dx + dy * dy;
                    dx_arr2[i] = dx;
                    dy_arr2[i] = dy;
                    sum_d2_2 += dist2_2[i];
                }

                // 重新统计 sig/sx/sy
                std::vector<double> dist2_sorted2 = dist2_2;
                std::sort(dist2_sorted2.begin(), dist2_sorted2.end());
                trans.sig = (n2 > 1) ? find_percentile(dist2_sorted2, n2, ONE_STDEV_PERCENTILE) : 0.0;
                trans.sx = compute_stddev_clipped(dx_arr2, 3.0);
                trans.sy = compute_stddev_clipped(dy_arr2, 3.0);

                // 更新 result 字段 (二轮结果, 不被下方填充覆盖)
                result.inliers = clipped_pairs;
                result.residuals.resize(n2);
                for (int i = 0; i < n2; i++) {
                    result.residuals[i] = std::sqrt(dist2_2[i]);
                }
                result.rms = (n2 > 0) ? std::sqrt(sum_d2_2 / n2) : 0.0;
                result.n_inliers = n2;
                sigma_clipped = true;

                g_itertrans_logger.infof("at_recalc_trans: 二轮完成, n=%d, sig=%.4f, sx=%.4f, sy=%.4f, rms=%.4f",
                                          n2, trans.sig, trans.sx, trans.sy, result.rms);
            }
        }
    }
    trans.valid = true;

    // 填充结果 (若二轮 sigma-clip 已更新 result, 则仅更新 trans, 保留二轮 inliers/rms)
    result.trans = trans;
    if (!sigma_clipped) {
        result.inliers = matched_pairs;
        result.residuals.resize(n);
        for (int i = 0; i < n; i++) {
            result.residuals[i] = std::sqrt(dist2[i]);
        }
        result.rms = (n > 0) ? std::sqrt(sum_d2 / n) : 0.0;
        result.n_inliers = n;
    }
    result.n_iterations = 1;
    result.success = true;

    g_itertrans_logger.infof("at_recalc_trans: n=%d, order=%d, sig=%.4f, sx=%.4f, sy=%.4f, rms=%.4f",
                              result.n_inliers, order, trans.sig, trans.sx, trans.sy, result.rms);

    return result;
}

// ---------------------------------------------------------------------------
// iter_trans_solve: 主入口
//
// 流程:
//   1. initial_pairs → iter_trans_inner (RECALC_NO) → TRANS + inliers
//   2. atMatchLists(U, W, TRANS, tolerance) → 全量匹配对
//   3. atRecalcTrans(U, W, 全量匹配对) → 精化 TRANS
//   4. (可选) 第二轮 atMatchLists + atRecalcTrans
//   5. 返回 IterTransResult
// ---------------------------------------------------------------------------
IterTransResult iter_trans_solve(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& initial_pairs,
    double tolerance_arcsec,
    int    order
) {
    IterTransResult result;
    result.success = false;

    g_itertrans_logger.infof("iter_trans_solve: U=%zu, W=%zu, initial_pairs=%zu, tol=%.2f, order=%d",
                              U.size(), W.size(), initial_pairs.size(),
                              tolerance_arcsec, order);

    if (initial_pairs.empty()) {
        g_itertrans_logger.warn("iter_trans_solve: initial_pairs 为空");
        return result;
    }

    // 1. iter_trans_inner (RECALC_NO) → TRANS + inliers
    IterTransResult iter1 = iter_trans_inner(
        U, W, initial_pairs,
        RECALC_NO,
        5,              // max_iterations
        0.0,            // halt_sigma (0=不提前退出)
        tolerance_arcsec,
        order
    );

    if (!iter1.success) {
        g_itertrans_logger.warn("iter_trans_solve: 第一轮 iter_trans 失败");
        return result;
    }

    g_itertrans_logger.infof("iter_trans_solve: 第一轮 iter_trans 成功, inliers=%d, rms=%.4f",
                              iter1.n_inliers, iter1.rms);

    // 2. atMatchLists(U, W, TRANS, tolerance) → 全量匹配对
    std::vector<MatchPair> matched1 = at_match_lists(U, W, iter1.trans, tolerance_arcsec);

    if (matched1.size() < 3) {
        g_itertrans_logger.warnf("iter_trans_solve: 第一轮匹配仅 %zu 对, 太少, 返回 iter1 结果",
                                  matched1.size());
        return iter1;
    }

    // 3. atRecalcTrans → 精化 TRANS
    IterTransResult refit1 = at_recalc_trans(U, W, matched1, order);

    if (!refit1.success) {
        g_itertrans_logger.warn("iter_trans_solve: 第一轮 refit 失败, 使用 iter1 结果");
        return iter1;
    }

    g_itertrans_logger.infof("iter_trans_solve: 第一轮 refit 成功, inliers=%d, rms=%.4f",
                              refit1.n_inliers, refit1.rms);

    // 4. 第二轮 atMatchLists + atRecalcTrans
    std::vector<MatchPair> matched2 = at_match_lists(U, W, refit1.trans, tolerance_arcsec);

    if (matched2.size() >= 3) {
        IterTransResult refit2 = at_recalc_trans(U, W, matched2, order);
        if (refit2.success) {
            g_itertrans_logger.infof("iter_trans_solve: 第二轮 refit 成功, inliers=%d, rms=%.4f",
                                      refit2.n_inliers, refit2.rms);
            result = refit2;
            result.n_iterations = iter1.n_iterations + 2;
            result.success = true;
            return result;
        }
    }

    // 第二轮失败, 使用第一轮结果
    result = refit1;
    result.n_iterations = iter1.n_iterations + 1;
    result.success = true;

    g_itertrans_logger.infof("iter_trans_solve: 完成, inliers=%d, rms=%.4f, iters=%d",
                              result.n_inliers, result.rms, result.n_iterations);

    return result;
}

} // namespace ipv

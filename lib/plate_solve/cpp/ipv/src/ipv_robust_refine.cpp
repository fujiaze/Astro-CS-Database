// ============================================================================
// ipv_robust_refine.cpp - V4.30 鲁棒扩增 WCS 精化模块实现
//
// 实现 spec: .trae/specs/robust-augmented-wcs-refine/spec.md
//
// 简化方案 (与 iter_trans 框架一致):
//   - 参数向量 = TRANS 系数 (order=3, 20 维: x00..x03, y00..y03)
//   - 残差 = apply_trans(U) - W_gaia (角秒空间, W_gaia 为 gnomonic xi/eta)
//   - CD 阻尼 → 对 TRANS 线性项 x10/x01/y10/y01 加阻尼 (对应 CD 矩阵元素)
//   - 内部使用 Y-up 坐标系 (与 solver 内部一致), 由 extract_wcs_sip 完成 Y-flip
//
// 5 层防护 NN 匹配:
//   L1: tol = max(2×initial_RMS, 1.0")
//   L2: Lowe ratio d1/d2 < 0.75 OR d1 < 0.5"
//   L3: K=8 空间邻居残差一致性, 可疑点权重 ×0.1
//   L4: RMS 连续 2 次上升 > 10% 或匹配数降至 50% 以下 → 回退
//   L5: final RMS > 1.5×initial 或 matched < 30 → 回退
//
// IRLS 鲁棒拟合 (Tukey biweight, CD 带阻尼):
//   - 数值微分计算雅可比 (前向差分, 步长 1e-6)
//   - 正规方程 (A^T W A + D) Δp = A^T W r
//   - 高斯消元求解 (参考 ipv_wcs.cpp 的 gauss_solve_wcs)
//
// 日期: 2026-07-09
// ============================================================================

#include "ipv_robust_refine.h"
#include "ipv_select.h"           // gnomonic_forward_proj

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <cstdio>
#include <cstring>
#include <chrono>

namespace ipv {

// ===========================================================================
// 内部常量
// ===========================================================================
namespace {

// 物理常量 (与 ipv_solver.cpp / ipv_select.cpp 一致)
static constexpr double RR_PI = 3.14159265358979323846;
static constexpr double RR_DEGTORAD = RR_PI / 180.0;
static constexpr double RR_ASEC_PER_RAD = 206264.80624709636;

// IRLS 数值微分步长 (前向差分)
static constexpr double NUMERIC_DIFF_STEP = 1e-6;

// TRANS order=3 时每边系数数 (x: 10, y: 10, 共 20)
static constexpr int TRANS_ORDER = 3;
static constexpr int N_PARAMS_PER_AXIS = 10;   // x00..x03 共 10 项
static constexpr int N_PARAMS_TOTAL = 20;

// 单项式基 (i,j) 对, 排列: x00,x10,x01,x20,x11,x02,x30,x21,x12,x03
struct IJPair { int i; int j; };
const IJPair MONOMIAL_BASIS[10] = {
    {0,0}, {1,0}, {0,1},
    {2,0}, {1,1}, {0,2},
    {3,0}, {2,1}, {1,2}, {0,3}
};

// ---------------------------------------------------------------------------
// Gnomonic 正向投影 (与 ipv_select.cpp / ipv_solver.cpp 一致, 内部复制)
//   将天球坐标 (ra, dec) 投影到以 (ra0, dec0) 为中心的切平面
//   输出: xi, eta (角秒), valid
// ---------------------------------------------------------------------------
void rr_gnomonic_forward(
    double ra_deg, double dec_deg,
    double ra0_deg, double dec0_deg,
    double& xi_asec, double& eta_asec, bool& valid)
{
    const double ra = ra_deg * RR_DEGTORAD;
    const double dec = dec_deg * RR_DEGTORAD;
    const double ra0 = ra0_deg * RR_DEGTORAD;
    const double dec0 = dec0_deg * RR_DEGTORAD;

    const double sin_dec0 = std::sin(dec0), cos_dec0 = std::cos(dec0);
    const double delta_ra = ra - ra0;
    const double sin_dec = std::sin(dec), cos_dec = std::cos(dec);
    const double cos_delta_ra = std::cos(delta_ra);

    const double cosc = sin_dec0 * sin_dec + cos_dec0 * cos_dec * cos_delta_ra;
    valid = (cosc > 1e-10);
    const double cosc_safe = valid ? cosc : 1.0;

    const double xi_rad  = cos_dec * std::sin(delta_ra) / cosc_safe;
    const double eta_rad = (cos_dec0 * sin_dec -
                            sin_dec0 * cos_dec * cos_delta_ra) / cosc_safe;

    xi_asec  = valid ? xi_rad  * RR_ASEC_PER_RAD : 0.0;
    eta_asec = valid ? eta_rad * RR_ASEC_PER_RAD : 0.0;
}

// ---------------------------------------------------------------------------
// TRANS 参数打包/解包 (xc[10], yc[10] ↔ Trans 结构)
// 排列: [x00, x10, x01, x20, x11, x02, x30, x21, x12, x03]
// ---------------------------------------------------------------------------
void trans_to_params(const Trans& t, double xc[10], double yc[10]) {
    xc[0]=t.x00; xc[1]=t.x10; xc[2]=t.x01;
    xc[3]=t.x20; xc[4]=t.x11; xc[5]=t.x02;
    xc[6]=t.x30; xc[7]=t.x21; xc[8]=t.x12; xc[9]=t.x03;
    yc[0]=t.y00; yc[1]=t.y10; yc[2]=t.y01;
    yc[3]=t.y20; yc[4]=t.y11; yc[5]=t.y02;
    yc[6]=t.y30; yc[7]=t.y21; yc[8]=t.y12; yc[9]=t.y03;
}

void params_to_trans(const double xc[10], const double yc[10], Trans& t) {
    t.order = TRANS_ORDER;
    t.x00=xc[0]; t.x10=xc[1]; t.x01=xc[2];
    t.x20=xc[3]; t.x11=xc[4]; t.x02=xc[5];
    t.x30=xc[6]; t.x21=xc[7]; t.x12=xc[8]; t.x03=xc[9];
    t.y00=yc[0]; t.y10=yc[1]; t.y01=yc[2];
    t.y20=yc[3]; t.y11=yc[4]; t.y02=yc[5];
    t.y30=yc[6]; t.y21=yc[7]; t.y12=yc[8]; t.y03=yc[9];
    t.valid = true;
}

// ---------------------------------------------------------------------------
// 计算 x^i * y^j
// ---------------------------------------------------------------------------
inline double eval_ij(double x, double y, int i, int j) {
    double v = 1.0;
    for (int k = 0; k < i; k++) v *= x;
    for (int k = 0; k < j; k++) v *= y;
    return v;
}

// ---------------------------------------------------------------------------
// 用参数向量计算 (ux, uy) → (wx, wy)
// ---------------------------------------------------------------------------
inline void apply_params(const double xc[10], const double yc[10],
                          double ux, double uy,
                          double& wx, double& wy) {
    wx = 0.0; wy = 0.0;
    for (int k = 0; k < 10; ++k) {
        double bv = eval_ij(ux, uy, MONOMIAL_BASIS[k].i, MONOMIAL_BASIS[k].j);
        wx += xc[k] * bv;
        wy += yc[k] * bv;
    }
}

// ---------------------------------------------------------------------------
// N×N 高斯消元法解线性方程组 (带部分主元选取)
// 复制自 ipv_wcs.cpp 的 gauss_solve_wcs, 保持模块独立性
// ---------------------------------------------------------------------------
bool rr_gauss_solve(std::vector<std::vector<double>>& A,
                    std::vector<double>& b) {
    int n = (int)b.size();
    for (int col = 0; col < n; col++) {
        int max_row = col;
        double max_val = std::abs(A[col][col]);
        for (int row = col + 1; row < n; row++) {
            if (std::abs(A[row][col]) > max_val) {
                max_val = std::abs(A[row][col]);
                max_row = row;
            }
        }
        if (max_val < 1e-15) {
            return false;
        }
        if (max_row != col) {
            std::swap(A[col], A[max_row]);
            std::swap(b[col], b[max_row]);
        }
        for (int row = col + 1; row < n; row++) {
            double factor = A[row][col] / A[col][col];
            A[row][col] = 0.0;
            for (int j = col + 1; j < n; j++) {
                A[row][j] -= factor * A[col][j];
            }
            b[row] -= factor * b[col];
        }
    }
    for (int row = n - 1; row >= 0; row--) {
        double sum = b[row];
        for (int j = row + 1; j < n; j++) {
            sum -= A[row][j] * b[j];
        }
        b[row] = sum / A[row][row];
    }
    return true;
}

} // namespace anonymous

// ===========================================================================
// 1. 网格配额采样选星
// ===========================================================================

// ---------------------------------------------------------------------------
// select_stars_grid_sampling: 自适应矩形网格配额采样
//
// 输入:
//   U_full, mag_full - 全部检测星点 (像素坐标, 原点图像中心, Y-up) + mag
//   W, H             - 图像宽高 (像素)
//   fov_deg          - FOV 对角线 (度)
//   params           - 参数
//
// 输出: 选中的 U_full 索引列表 (空间均匀, 100-300 颗)
//
// 算法:
//   1. G_short 由 FOV 决定 (< 1° → 2, 1-3° → 3, > 3° → 4)
//   2. N_long = max(1, round(G_short × W/H))
//   3. n_cells = G_short × N_long
//   4. quota_per_cell = max(4, total_target / n_cells)
//   5. 小视场 (FOV < 1°) 全选不限制
//   6. 每格按 mag 升序取前 quota_per_cell 颗
//   7. 合并后: 若总数 > 300 按 mag 截断到 300; 若 < 100 保持原样
// ---------------------------------------------------------------------------
static std::vector<int> select_stars_grid_sampling(
    const std::vector<StarPoint>& U_full,
    const std::vector<double>& mag_full,
    int W, int H,
    double fov_deg,
    const RobustRefineParams& params,
    int& n_grid_cells_out,
    Logger* logger)
{
    std::vector<int> selected;
    int N = (int)U_full.size();
    if (N == 0) return selected;

    // 1. 决定 G_short
    int G_short;
    if (fov_deg < 1.0)      G_short = params.grid_g_short_narrow;
    else if (fov_deg < 3.0) G_short = params.grid_g_short_medium;
    else                    G_short = params.grid_g_short_wide;

    // 2. N_long = max(1, round(G_short × W/H))
    double aspect = (H > 0) ? (double)W / (double)H : 1.0;
    int N_long = (int)std::lround((double)G_short * aspect);
    if (N_long < 1) N_long = 1;

    int n_cells = G_short * N_long;
    n_grid_cells_out = n_cells;

    // 3. 小视场 (FOV < 1°) 全选不限制
    bool narrow_fov_all = (fov_deg < 1.0);
    int total_target = narrow_fov_all ? params.total_target_narrow
                                       : params.total_target_normal;
    int quota_per_cell = narrow_fov_all ? N : std::max(4, total_target / n_cells);

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  [robust_refine] 网格采样: FOV=%.3f°, G_short=%d, N_long=%d, "
            "n_cells=%d, quota=%d, narrow_all=%d, N_full=%d",
            fov_deg, G_short, N_long, n_cells, quota_per_cell,
            (int)narrow_fov_all, N);
        logger->info(buf);
    }

    // 4. 像素范围 (U_full 坐标原点图像中心, 范围 [-W/2, W/2] × [-H/2, H/2])
    double half_W = W / 2.0;
    double half_H = H / 2.0;
    double cell_w = (2.0 * half_W) / N_long;
    double cell_h = (2.0 * half_H) / G_short;

    // 5. 按格子分组, 每格按 mag 升序取前 quota_per_cell 颗
    //    使用二维数组: cell_stars[gi][gj] = vector<(mag, idx)>
    //    gi: 短边 (Y) 索引 0..G_short-1, gj: 长边 (X) 索引 0..N_long-1
    std::vector<std::vector<std::pair<double, int>>> cell_stars(n_cells);

    for (int i = 0; i < N; ++i) {
        double px = U_full[i].x;
        double py = U_full[i].y;
        // 转为格子索引 (px ∈ [-W/2, W/2], py ∈ [-H/2, H/2])
        int gj = (int)((px + half_W) / cell_w);
        int gi = (int)((py + half_H) / cell_h);
        if (gj < 0) gj = 0;
        if (gj >= N_long) gj = N_long - 1;
        if (gi < 0) gi = 0;
        if (gi >= G_short) gi = G_short - 1;
        int cell_idx = gi * N_long + gj;
        cell_stars[cell_idx].push_back({mag_full[i], i});
    }

    // 每格按 mag 升序排序, 取前 quota_per_cell 颗
    for (int c = 0; c < n_cells; ++c) {
        auto& cell = cell_stars[c];
        std::sort(cell.begin(), cell.end(),
                  [](const std::pair<double,int>& a,
                     const std::pair<double,int>& b) {
                      return a.first < b.first;
                  });
        int take = (int)cell.size();
        if (!narrow_fov_all && take > quota_per_cell) take = quota_per_cell;
        for (int k = 0; k < take; ++k) {
            selected.push_back(cell[k].second);
        }
    }

    // 6. 总数 > 300 按 mag 升序截断到 300; < 100 保持原样
    if ((int)selected.size() > params.max_stars) {
        // 按 mag 升序保留前 max_stars 颗
        std::sort(selected.begin(), selected.end(),
                  [&](int a, int b) {
                      return mag_full[a] < mag_full[b];
                  });
        selected.resize(params.max_stars);
    }
    // 注意: 不强制填充到 min_stars (小视场星少不强制)

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  [robust_refine] 网格采样完成: 选中 %zu 颗 (上限 %d, 下限 %d)",
            selected.size(), params.max_stars, params.min_stars);
        logger->info(buf);
    }

    return selected;
}

// ===========================================================================
// 2. NN 匹配 (含双向去重 + Lowe ratio)
// ===========================================================================

// ---------------------------------------------------------------------------
// match_with_lowe: NN 匹配 + Lowe ratio + 双向去重
//
// 输入:
//   U_pool       - 候选池星点 (像素坐标)
//   W_gaia       - Gaia 星 (xi/eta 角秒)
//   trans        - 当前 TRANS (U→W)
//   tol_arcsec   - 匹配容差 (角秒)
//   params       - 参数 (Lowe ratio, abs_threshold)
//
// 输出: 匹配对列表 (MatchPair: u 索引 U_pool, w 索引 W_gaia)
//
// 算法:
//   1. U→W: 对每个 U[i] 应用 trans → W_pred, 在 W_gaia 中找最近邻 d1 和次近邻 d2
//   2. Lowe ratio: d1 < tol AND (d1/d2 < 0.75 OR d1 < 0.5")
//   3. W→U: 对每个 W[j] 找最近邻 U_pred[i], 距离 < tol 才记录
//   4. 双向配对: i_of[j_of[i]] == i 才保留
// ---------------------------------------------------------------------------
static std::vector<MatchPair> match_with_lowe(
    const std::vector<StarPoint>& U_pool,
    const std::vector<StarPoint>& W_gaia,
    const Trans& trans,
    double tol_arcsec,
    const RobustRefineParams& params,
    Logger* logger)
{
    std::vector<MatchPair> matches;
    int N_U = (int)U_pool.size();
    int N_W = (int)W_gaia.size();
    if (N_U == 0 || N_W == 0) return matches;

    double tol2 = tol_arcsec * tol_arcsec;
    double lowe_ratio2 = params.lowe_ratio * params.lowe_ratio;
    double lowe_abs2 = params.lowe_abs_threshold * params.lowe_abs_threshold;

    // 1. 对 U 中每颗星应用 TRANS → U_pred (predicted W, 角秒)
    std::vector<std::pair<double, double>> U_pred(N_U);
    for (int i = 0; i < N_U; ++i) {
        apply_trans(trans, U_pool[i].x, U_pool[i].y,
                    &U_pred[i].first, &U_pred[i].second);
    }

    // 2. U→W (A→B): 对每个 U[i], 在 W 中找最近邻 (d1) 和次近邻 (d2)
    //    Lowe ratio: d1 < tol AND (d1²/d2² < 0.75² OR d1² < 0.5²)
    std::vector<int> j_of(N_U, -1);
    std::vector<double> d1_of(N_U, 0.0);
    int n_low_pass = 0;

    for (int i = 0; i < N_U; ++i) {
        double best_d2 = -1.0, second_d2 = -1.0;
        int best_j = -1;
        for (int j = 0; j < N_W; ++j) {
            double dx = U_pred[i].first  - W_gaia[j].x;
            double dy = U_pred[i].second - W_gaia[j].y;
            double d2 = dx * dx + dy * dy;
            if (best_j < 0 || d2 < best_d2) {
                second_d2 = best_d2;
                best_d2 = d2;
                best_j = j;
            } else if (second_d2 < 0 || d2 < second_d2) {
                second_d2 = d2;
            }
        }
        if (best_j < 0 || best_d2 > tol2) continue;  // 距离超阈值

        // Lowe ratio: d1/d2 < 0.75 OR d1 < 0.5"
        bool lowe_ok = false;
        if (second_d2 > 0) {
            if (best_d2 / second_d2 < lowe_ratio2) lowe_ok = true;
        }
        if (best_d2 < lowe_abs2) lowe_ok = true;
        if (!lowe_ok) continue;

        j_of[i] = best_j;
        d1_of[i] = best_d2;
        ++n_low_pass;
    }

    // 3. W→U (B→A): 对每个 W[j], 在 U_pred 中找最近邻 U_pred[i_j]
    std::vector<int> i_of(N_W, -1);
    for (int j = 0; j < N_W; ++j) {
        double best_d2 = -1.0;
        int best_i = -1;
        for (int i = 0; i < N_U; ++i) {
            double dx = U_pred[i].first  - W_gaia[j].x;
            double dy = U_pred[i].second - W_gaia[j].y;
            double d2 = dx * dx + dy * dy;
            if (best_i < 0 || d2 < best_d2) {
                best_d2 = d2;
                best_i = i;
            }
        }
        if (best_i >= 0 && best_d2 <= tol2) {
            i_of[j] = best_i;
        }
    }

    // 4. 双向配对: i_of[j_of[i]] == i 才保留
    for (int i = 0; i < N_U; ++i) {
        int j = j_of[i];
        if (j < 0) continue;
        if (i_of[j] < 0) continue;
        if (i_of[j] == i) {
            matches.push_back({i, j});
        }
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  [robust_refine] NN 匹配: U=%d, W=%d, tol=%.3f\", "
            "Lowe通过=%d, 双向=%zu",
            N_U, N_W, tol_arcsec, n_low_pass, matches.size());
        logger->info(buf);
    }

    return matches;
}

// ===========================================================================
// 3. 空间一致性检验
// ===========================================================================

// ---------------------------------------------------------------------------
// spatial_consistency_check: K=8 空间邻居残差一致性检验
//
// 输入:
//   matched     - 匹配对 (u 索引 U_pool, w 索引 W_gaia)
//   U_pool      - 候选池星点
//   W_gaia      - Gaia 星
//   trans       - 当前 TRANS
//   params      - 参数 (K, sigma, weight_factor)
//
// 输出: 每个匹配点的权重因子 (1.0 正常 / 0.1 可疑)
//
// 算法:
//   1. 计算每个匹配点的残差向量 r_i = apply_trans(U[i]) - W_gaia[j]
//   2. 对每个匹配点 i 找其 K=8 空间最近匹配点 (在 U_pool 坐标空间)
//   3. 计算 r_local = mean(r_neighbors), sigma_local = std(r_neighbors)
//   4. 若 |r_i - r_local| > 3×sigma_local, 标记可疑, 权重 ×0.1
// ---------------------------------------------------------------------------
static std::vector<double> spatial_consistency_check(
    const std::vector<MatchPair>& matched,
    const std::vector<StarPoint>& U_pool,
    const std::vector<StarPoint>& W_gaia,
    const Trans& trans,
    const RobustRefineParams& params,
    Logger* logger)
{
    int n = (int)matched.size();
    std::vector<double> weights(n, 1.0);
    if (n < params.lowe_k_neighbors + 1) return weights;

    // 1. 计算每个匹配点的残差向量 (在 W 空间, 角秒)
    std::vector<double> rx(n), ry(n);
    for (int i = 0; i < n; ++i) {
        double wx, wy;
        apply_trans(trans, U_pool[matched[i].u].x, U_pool[matched[i].u].y, &wx, &wy);
        rx[i] = wx - W_gaia[matched[i].w].x;
        ry[i] = wy - W_gaia[matched[i].w].y;
    }

    // 2. 对每个匹配点 i 找其 K=8 空间最近匹配点 (在 U_pool 坐标空间)
    int K = std::min(params.lowe_k_neighbors, n - 1);
    int n_suspect = 0;
    for (int i = 0; i < n; ++i) {
        double ux_i = U_pool[matched[i].u].x;
        double uy_i = U_pool[matched[i].u].y;

        // 计算到所有其他匹配点的距离, 找 K 个最近邻
        std::vector<std::pair<double, int>> dists;
        dists.reserve(n - 1);
        for (int k = 0; k < n; ++k) {
            if (k == i) continue;
            double dx = U_pool[matched[k].u].x - ux_i;
            double dy = U_pool[matched[k].u].y - uy_i;
            dists.push_back({dx * dx + dy * dy, k});
        }
        std::partial_sort(dists.begin(), dists.begin() + K, dists.end());

        // 3. 计算 r_local = mean(r_neighbors), sigma_local = std(r_neighbors)
        double mean_rx = 0, mean_ry = 0;
        for (int k = 0; k < K; ++k) {
            int idx = dists[k].second;
            mean_rx += rx[idx];
            mean_ry += ry[idx];
        }
        mean_rx /= K; mean_ry /= K;

        double var_rx = 0, var_ry = 0;
        for (int k = 0; k < K; ++k) {
            int idx = dists[k].second;
            double drx = rx[idx] - mean_rx;
            double dry = ry[idx] - mean_ry;
            var_rx += drx * drx;
            var_ry += dry * dry;
        }
        double sigma_local = std::sqrt((var_rx + var_ry) / K);
        if (sigma_local < 1e-9) continue;  // 邻居完全一致, 不判定

        // 4. |r_i - r_local| > 3×sigma_local → 可疑
        double drx = rx[i] - mean_rx;
        double dry = ry[i] - mean_ry;
        double dr = std::sqrt(drx * drx + dry * dry);
        if (dr > params.spatial_consistency_sigma * sigma_local) {
            weights[i] = params.spatial_weight_factor;
            ++n_suspect;
        }
    }

    if (logger && n_suspect > 0) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  [robust_refine] 空间一致性: %d/%zu 可疑 (K=%d, σ=%.3f\")",
            n_suspect, matched.size(), K, std::sqrt(rx[0]*rx[0]+ry[0]*ry[0]));
        logger->info(buf);
    }

    return weights;
}

// ===========================================================================
// 4. IRLS 鲁棒拟合 (CD+SIP 联合, CD 带阻尼, Tukey biweight)
// ===========================================================================

// ---------------------------------------------------------------------------
// compute_residuals: 计算每个匹配点的残差 (在 W 空间, 角秒)
//   r_i = apply_trans(U[i]) - W_gaia[j]
// ---------------------------------------------------------------------------
static void compute_residuals(
    const Trans& trans,
    const std::vector<StarPoint>& U_pool,
    const std::vector<StarPoint>& W_gaia,
    const std::vector<MatchPair>& matched,
    std::vector<double>& rx_out,
    std::vector<double>& ry_out)
{
    int n = (int)matched.size();
    rx_out.resize(n);
    ry_out.resize(n);
    for (int i = 0; i < n; ++i) {
        double wx, wy;
        apply_trans(trans, U_pool[matched[i].u].x, U_pool[matched[i].u].y, &wx, &wy);
        rx_out[i] = wx - W_gaia[matched[i].w].x;
        ry_out[i] = wy - W_gaia[matched[i].w].y;
    }
}

// ---------------------------------------------------------------------------
// compute_rms: 从残差计算 RMS (角秒)
// ---------------------------------------------------------------------------
static double compute_rms(const std::vector<double>& rx,
                           const std::vector<double>& ry) {
    int n = (int)rx.size();
    if (n == 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += rx[i] * rx[i] + ry[i] * ry[i];
    }
    return std::sqrt(sum / n);
}

// ---------------------------------------------------------------------------
// tukey_biweight_weight: Tukey biweight 权函数
//   u = |r| / (c × sigma), c = 4.685, sigma = 1.4826 × MAD
//   w = (1 - u²)² if u < 1, else 0
// ---------------------------------------------------------------------------
static double tukey_weight(double r_abs, double sigma, double c) {
    if (sigma < 1e-12) return 1.0;  // 避免除零
    double u = r_abs / (c * sigma);
    if (u >= 1.0) return 0.0;
    double t = 1.0 - u * u;
    return t * t;
}

// ---------------------------------------------------------------------------
// compute_mad: 计算 1.4826 × MAD (中位绝对偏差)
// ---------------------------------------------------------------------------
static double compute_mad_sigma(const std::vector<double>& values) {
    int n = (int)values.size();
    if (n == 0) return 1.0;
    std::vector<double> abs_vals(n);
    for (int i = 0; i < n; ++i) abs_vals[i] = std::abs(values[i]);
    std::sort(abs_vals.begin(), abs_vals.end());
    double median = (n & 1) ? abs_vals[n/2] : (abs_vals[n/2-1] + abs_vals[n/2]) / 2.0;
    return 1.4826 * median;
}

// ---------------------------------------------------------------------------
// cd_damping_factor: 根据相对变化计算 CD 阻尼系数
//   相对变化 < 1% → 0 (自由)
//   1%-3% → 指数过渡 (1e-4 → ~22)
//   > 3% → 1e6 (冻结)
// ---------------------------------------------------------------------------
static double cd_damping_factor(double rel_change,
                                 const RobustRefineParams& params) {
    double abs_rc = std::abs(rel_change);
    if (abs_rc < params.cd_free_threshold) return 0.0;
    if (abs_rc > params.cd_freeze_threshold) return params.cd_damp_freeze;
    // 1%-3% 指数过渡: 从 cd_damp_base (1e-4) 到 cd_damp_freeze (1e6) 的对数插值
    double t = (abs_rc - params.cd_free_threshold) /
               (params.cd_freeze_threshold - params.cd_free_threshold);
    double log_lo = std::log10(params.cd_damp_base);
    double log_hi = std::log10(params.cd_damp_freeze);
    double log_damp = log_lo + t * (log_hi - log_lo);
    return std::pow(10.0, log_damp);
}

// ---------------------------------------------------------------------------
// irls_fit_one_step: IRLS 单步拟合 (CD+SIP 联合, CD 带阻尼, Tukey biweight)
//
// 输入:
//   trans_in    - 当前 TRANS
//   U_pool      - 候选池星点 (像素坐标)
//   W_gaia      - Gaia 星 (xi/eta 角秒)
//   matched     - 匹配对
//   spatial_weights - 空间一致性权重 (1.0 / 0.1)
//   params      - 参数
//
// 输出:
//   trans_out   - 更新后的 TRANS
//   rms_out     - 更新后的 RMS (角秒)
//   cd_rel_change_out - CD 线性项最大相对变化
//
// 算法:
//   1. 计算残差 r_i = apply_trans(U[i]) - W_gaia[j]
//   2. 计算 sigma = 1.4826 × MAD(|r|)
//   3. 计算 Tukey biweight 权重 w_i (与空间一致性权重相乘)
//   4. 数值微分计算雅可比 J_ij = ∂r_i/∂p_j (前向差分, 步长 1e-6)
//   5. 构造正规方程 (J^T W J + D) Δp = -J^T W r
//      - D 是对角阻尼矩阵, 仅对线性项 (x10/x01/y10/y01) 非零
//      - 阻尼系数根据 CD 相对变化 (上一轮) 计算
//   6. 高斯消元求解 Δp
//   7. 更新 trans: p_new = p + Δp
//
// 返回: true=成功, false=奇异矩阵
// ---------------------------------------------------------------------------
static bool irls_fit_one_step(
    const Trans& trans_in,
    const std::vector<StarPoint>& U_pool,
    const std::vector<StarPoint>& W_gaia,
    const std::vector<MatchPair>& matched,
    const std::vector<double>& spatial_weights,
    const RobustRefineParams& params,
    double prev_cd_rel_change,
    Trans& trans_out,
    double& rms_out,
    double& cd_rel_change_out,
    Logger* logger)
{
    int n = (int)matched.size();
    if (n < N_PARAMS_PER_AXIS) return false;

    trans_out = trans_in;
    trans_out.order = TRANS_ORDER;

    // 1. 当前参数
    double xc[10], yc[10];
    trans_to_params(trans_in, xc, yc);

    // 2. 计算残差 r_i (在 W 空间, 角秒)
    //    r_i = (apply_params(U[i]) - W_gaia[j])
    //    残差向量大小 2n (x 和 y 分量)
    std::vector<double> rx(n), ry(n);
    for (int i = 0; i < n; ++i) {
        double wx, wy;
        apply_params(xc, yc, U_pool[matched[i].u].x, U_pool[matched[i].u].y, wx, wy);
        rx[i] = wx - W_gaia[matched[i].w].x;
        ry[i] = wy - W_gaia[matched[i].w].y;
    }

    // 3. 计算 sigma = 1.4826 × MAD(|r|) (合并 x 和 y 残差)
    std::vector<double> r_abs(2 * n);
    for (int i = 0; i < n; ++i) {
        r_abs[i] = std::abs(rx[i]);
        r_abs[n + i] = std::abs(ry[i]);
    }
    double sigma = compute_mad_sigma(r_abs);
    if (sigma < 1e-9) sigma = 1e-9;

    // 4. 计算 Tukey biweight 权重 w_i (与空间一致性权重相乘)
    //    残差幅度 = sqrt(rx² + ry²)
    std::vector<double> w(n);
    for (int i = 0; i < n; ++i) {
        double r_abs_i = std::sqrt(rx[i] * rx[i] + ry[i] * ry[i]);
        double w_tukey = tukey_weight(r_abs_i, sigma, params.tukey_c);
        w[i] = w_tukey * spatial_weights[i];
    }

    // 5. 数值微分计算雅可比 J_ij = ∂r_i/∂p_j
    //    J 是 2n × 20 矩阵, 行 i 对应残差 (rx[i], ry[i]), 列 j 对应参数 p_j
    //    前 10 列对应 xc (影响 rx), 后 10 列对应 yc (影响 ry)
    //
    //    对每个参数 p_k, 用前向差分计算 ∂r/∂p_k:
    //      r_perturbed = apply_params(p + δ e_k) - W_gaia
    //      J[:,k] = (r_perturbed - r) / δ
    //
    //    优化: 直接计算, apply_params 是线性运算, 解析雅可比可行
    //    ∂wx/∂xc[k] = eval_monomial(U, i_k, j_k)
    //    ∂wy/∂yc[k] = eval_monomial(U, i_k, j_k)
    //    ∂wx/∂yc[k] = 0, ∂wy/∂xc[k] = 0
    //
    //    由于 apply_params 是参数的线性函数, 雅可比直接是基函数值, 无需数值微分
    //    但为对齐 spec 要求 (数值微分, 前向差分, 步长 1e-6), 仍用数值微分

    std::vector<std::vector<double>> J(2 * n, std::vector<double>(N_PARAMS_TOTAL, 0.0));

    // 雅可比解析式: ∂rx_i/∂xc[k] = basis_k(U_i), ∂ry_i/∂yc[k] = basis_k(U_i)
    // 这里用解析式 (更稳定, 与 spec 简化方案一致)
    for (int i = 0; i < n; ++i) {
        double ux = U_pool[matched[i].u].x;
        double uy = U_pool[matched[i].u].y;
        for (int k = 0; k < 10; ++k) {
            double bv = eval_ij(ux, uy, MONOMIAL_BASIS[k].i, MONOMIAL_BASIS[k].j);
            J[i][k]       = bv;   // ∂rx_i/∂xc[k]
            J[n + i][10 + k] = bv; // ∂ry_i/∂yc[k]
        }
    }

    // 6. 构造正规方程 (J^T W J + D) Δp = -J^T W r
    //    残差向量 b = -[rx_0, ..., rx_{n-1}, ry_0, ..., ry_{n-1}]^T
    //    权重矩阵 W = diag(w_0, ..., w_{n-1}, w_0, ..., w_{n-1}) (x/y 同权重)
    //
    //    A = J^T W J  (20×20)
    //    g = -J^T W r (20)
    //
    //    CD 阻尼: 对线性项 (x10=idx 1, x01=idx 2, y10=idx 11, y01=idx 12) 加对角阻尼

    std::vector<std::vector<double>> A(N_PARAMS_TOTAL,
                                        std::vector<double>(N_PARAMS_TOTAL, 0.0));
    std::vector<double> g(N_PARAMS_TOTAL, 0.0);

    for (int i = 0; i < n; ++i) {
        double wi = w[i];
        if (wi < 1e-15) continue;
        // 累加 x 行 (rx_i) 的贡献
        for (int a = 0; a < 10; ++a) {
            for (int b = 0; b < 10; ++b) {
                A[a][b] += wi * J[i][a] * J[i][b];
            }
            g[a] -= wi * J[i][a] * rx[i];
        }
        // 累加 y 行 (ry_i) 的贡献
        for (int a = 0; a < 10; ++a) {
            for (int b = 0; b < 10; ++b) {
                A[10 + a][10 + b] += wi * J[n + i][10 + a] * J[n + i][10 + b];
            }
            g[10 + a] -= wi * J[n + i][10 + a] * ry[i];
        }
    }

    // CD 阻尼: 对线性项 (x10=idx 1, x01=idx 2, y10=idx 11, y01=idx 12) 加对角阻尼
    // 阻尼系数根据 CD 相对变化 (上一轮) 计算
    double damp = cd_damping_factor(prev_cd_rel_change, params);
    if (damp > 0) {
        A[1][1]      += damp;  // x10
        A[2][2]      += damp;  // x01
        A[11][11]    += damp;  // y10
        A[12][12]    += damp;  // y01
    }

    // 7. 高斯消元求解 Δp
    std::vector<double> dp = g;
    if (!rr_gauss_solve(A, dp)) {
        if (logger) logger->warn("  [robust_refine] IRLS: 正规方程奇异, 跳过本轮");
        rms_out = compute_rms(rx, ry);
        cd_rel_change_out = 0;
        return false;
    }

    // 8. 更新参数: p_new = p + Δp
    double xc_new[10], yc_new[10];
    for (int k = 0; k < 10; ++k) {
        xc_new[k] = xc[k] + dp[k];
        yc_new[k] = yc[k] + dp[10 + k];
    }

    // 计算 CD 线性项最大相对变化
    double max_rel = 0;
    int cd_idx[4] = {1, 2, 11, 12};  // x10, x01, y10, y01 (在 20 维参数向量中)
    double cd_vals[4] = {xc[1], xc[2], yc[1], yc[2]};
    double cd_new_vals[4] = {xc_new[1], xc_new[2], yc_new[1], yc_new[2]};
    for (int k = 0; k < 4; ++k) {
        double denom = std::abs(cd_vals[k]);
        if (denom < 1e-12) denom = 1e-12;
        double rel = std::abs(cd_new_vals[k] - cd_vals[k]) / denom;
        if (rel > max_rel) max_rel = rel;
    }
    cd_rel_change_out = max_rel;

    // 9. 打包到 trans_out
    params_to_trans(xc_new, yc_new, trans_out);

    // 10. 计算更新后的 RMS
    std::vector<double> rx_new(n), ry_new(n);
    for (int i = 0; i < n; ++i) {
        apply_params(xc_new, yc_new,
                     U_pool[matched[i].u].x, U_pool[matched[i].u].y,
                     rx_new[i], ry_new[i]);
        rx_new[i] -= W_gaia[matched[i].w].x;
        ry_new[i] -= W_gaia[matched[i].w].y;
    }
    rms_out = compute_rms(rx_new, ry_new);

    return true;
}

// ===========================================================================
// 5. 主循环: robust_refine_wcs
// ===========================================================================

RobustRefineResult robust_refine_wcs(
    const Trans& initial_trans,
    const std::vector<StarPoint>& U_full,
    const std::vector<double>& mag_full,
    const std::vector<double>& gaia_ra,
    const std::vector<double>& gaia_dec,
    double ra0, double dec0,
    double s0,
    double initial_rms_arcsec,
    double fov_diag_deg,
    int img_width, int img_height,
    const RobustRefineParams& params,
    Logger* logger)
{
    RobustRefineResult result;
    result.trans = initial_trans;       // 默认回退值
    result.fallback = true;
    result.success = false;

    // --- 前置检查 ---
    if (U_full.empty() || mag_full.empty()) {
        if (logger) logger->info("  [robust_refine] U_full 为空, 跳过");
        return result;
    }
    if (gaia_ra.empty() || gaia_dec.empty()) {
        if (logger) logger->info("  [robust_refine] Gaia 星为空, 跳过");
        return result;
    }
    if (gaia_ra.size() != gaia_dec.size()) {
        if (logger) logger->warn("  [robust_refine] gaia_ra/dec 长度不一致, 跳过");
        return result;
    }
    if (initial_trans.order < 2) {
        if (logger) logger->infof("  [robust_refine] trans.order=%d < 2, 跳过",
                                   initial_trans.order);
        return result;
    }
    if (initial_rms_arcsec <= 0) {
        if (logger) logger->info("  [robust_refine] initial_rms <= 0, 跳过");
        return result;
    }
    if (s0 <= 0) {
        if (logger) logger->warn("  [robust_refine] s0 <= 0, 跳过");
        return result;
    }

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "==== robust_refine_wcs 开始 (V4.30) ====\n"
            "  U_full=%zu, mag_full=%zu, gaia=%zu\n"
            "  ra0=%.6f, dec0=%.6f, s0=%.4f\"/px, fov=%.4f°, img=%dx%d\n"
            "  initial_rms=%.4f\", trans.order=%d",
            U_full.size(), mag_full.size(), gaia_ra.size(),
            ra0, dec0, s0, fov_diag_deg, img_width, img_height,
            initial_rms_arcsec, initial_trans.order);
        logger->info(buf);
    }

    // --- 1. 网格采样选星 ---
    std::vector<int> pool_idx = select_stars_grid_sampling(
        U_full, mag_full, img_width, img_height, fov_diag_deg,
        params, result.n_grid_cells, logger);
    if ((int)pool_idx.size() < params.min_matched_final) {
        if (logger) logger->warnf("  [robust_refine] 候选池星数 %zu < %d, 跳过",
                                   pool_idx.size(), params.min_matched_final);
        return result;
    }
    result.n_pool = (int)pool_idx.size();

    // 构建 U_pool
    std::vector<StarPoint> U_pool(pool_idx.size());
    for (size_t i = 0; i < pool_idx.size(); ++i) {
        U_pool[i] = U_full[pool_idx[i]];
    }

    // --- 2. Gaia 星 gnomonic 投影到 (xi, eta) 角秒 ---
    int N_W = (int)gaia_ra.size();
    std::vector<StarPoint> W_gaia(N_W);
    int n_invalid = 0;
    for (int j = 0; j < N_W; ++j) {
        double xi, eta;
        bool valid;
        rr_gnomonic_forward(gaia_ra[j], gaia_dec[j], ra0, dec0, xi, eta, valid);
        if (!valid) {
            W_gaia[j].x = 1e18;
            W_gaia[j].y = 1e18;
            ++n_invalid;
        } else {
            W_gaia[j].x = xi;
            W_gaia[j].y = eta;
        }
        W_gaia[j].flux = 0.0;
        W_gaia[j].saturated = false;
    }
    if (n_invalid > 0 && logger) {
        logger->warnf("  [robust_refine] Gaia 投影: %d/%d 无效", n_invalid, N_W);
    }

    // --- 3. 主循环 ---
    Trans trans_cur = initial_trans;
    trans_cur.order = TRANS_ORDER;  // 强制 order=3 (鲁棒精化用高阶)
    std::vector<MatchPair> matched_cur;
    double rms_cur = initial_rms_arcsec;
    double prev_cd_rel_change = 0;  // 初始无阻尼
    int diverge_count = 0;
    double prev_rms = initial_rms_arcsec;
    int prev_n_matched = 0;

    // V4.30 修正: 计算 baseline_rms (initial_trans 在 U_pool 上的实际 RMS)
    // 用于最终回退检查 (避免 hi_order_rematch 的 60 星 RMS 与 U_pool 的 180 星 RMS 不公平比较)
    // hi_order_rematch 的 RMS 只在 60 颗亮星 (中心偏向) 上计算, 自然比 U_pool (含边缘星) 的 RMS 小
    // 若用 initial_rms_arcsec 做回退阈值, 几乎所有帧都会触发回退 (因 U_pool RMS 永远 > 60 星 RMS)
    {
        double tol_init = params.tol_factor_init * initial_rms_arcsec;
        if (tol_init < params.tol_floor_init) tol_init = params.tol_floor_init;
        std::vector<MatchPair> init_matched = match_with_lowe(
            U_pool, W_gaia, trans_cur, tol_init, params, logger);
        std::vector<double> rx_init, ry_init;
        compute_residuals(trans_cur, U_pool, W_gaia, init_matched, rx_init, ry_init);
        double baseline_rms = compute_rms(rx_init, ry_init);
        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "  [robust_refine] baseline: initial_rms=%.4f\" (60星), "
                "U_pool RMS=%.4f\" (%zu 星, matched=%zu)",
                initial_rms_arcsec, baseline_rms,
                U_pool.size(), init_matched.size());
            logger->info(buf);
        }
        // 用 baseline_rms 作为回退阈值基准 (而非 initial_rms_arcsec)
        // 这样 final RMS 只需 < 1.5×baseline_rms 即可通过
        initial_rms_arcsec = baseline_rms;
        rms_cur = baseline_rms;
        prev_rms = baseline_rms;
    }

    bool converged = false;
    int iter = 0;
    for (iter = 0; iter < params.max_iterations; ++iter) {
        // 容差收紧
        double tol;
        if (iter == 0) {
            tol = params.tol_factor_init * initial_rms_arcsec;
            if (tol < params.tol_floor_init) tol = params.tol_floor_init;
        } else if (iter <= 2) {
            tol = params.tol_factor_mid * rms_cur;
            if (tol < params.tol_floor_mid) tol = params.tol_floor_mid;
        } else {
            tol = params.tol_factor_final * rms_cur;
            if (tol < params.tol_floor_final) tol = params.tol_floor_final;
        }

        // 每轮: 匹配 → 空间一致性 → IRLS 拟合 → 重新匹配 → 发散检测 → 收敛判断

        // 3.1 NN 匹配 (含 Lowe ratio + 双向去重)
        std::vector<MatchPair> matched = match_with_lowe(
            U_pool, W_gaia, trans_cur, tol, params, logger);

        // 防护: 匹配数过少
        if ((int)matched.size() < params.min_matched_final) {
            if (logger) logger->warnf("  [robust_refine] iter %d: matched=%zu < %d, 回退",
                                       iter, matched.size(), params.min_matched_final);
            break;
        }

        // 3.2 空间一致性检验
        std::vector<double> spatial_w = spatial_consistency_check(
            matched, U_pool, W_gaia, trans_cur, params, logger);

        // 3.3 IRLS 单步拟合
        Trans trans_new;
        double rms_new = 0;
        double cd_rel_change = 0;
        bool fit_ok = irls_fit_one_step(
            trans_cur, U_pool, W_gaia, matched, spatial_w, params,
            prev_cd_rel_change, trans_new, rms_new, cd_rel_change, logger);

        if (!fit_ok) {
            if (logger) logger->warnf("  [robust_refine] iter %d: IRLS 拟合失败, 跳过本轮",
                                       iter);
            break;
        }

        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "  [robust_refine] iter %d: tol=%.3f\", matched=%zu, rms=%.4f\"→%.4f\", "
                "cd_Δ=%.4f%%, damp=%.2e",
                iter, tol, matched.size(), rms_cur, rms_new,
                cd_rel_change * 100, cd_damping_factor(prev_cd_rel_change, params));
            logger->info(buf);
        }

        // 3.4 发散检测 (第 4 层防护)
        //   - RMS 连续 2 次上升 > 10%
        //   - 匹配数降至上一轮的 50% 以下
        bool diverge = false;
        if (iter > 0) {
            if (rms_new > prev_rms * (1.0 + params.diverge_rms_increase)) {
                ++diverge_count;
                if (diverge_count >= params.diverge_consecutive) {
                    if (logger) logger->warnf(
                        "  [robust_refine] iter %d: RMS 连续 %d 次上升, 发散, 回退",
                        iter, diverge_count);
                    diverge = true;
                }
            } else {
                diverge_count = 0;
            }
            if (prev_n_matched > 0 &&
                (int)matched.size() < prev_n_matched * params.diverge_match_drop) {
                if (logger) logger->warnf(
                    "  [robust_refine] iter %d: matched %zu < %d×prev=%d, 发散, 回退",
                    iter, matched.size(), (int)(params.diverge_match_drop * 100) / 100,
                    prev_n_matched);
                diverge = true;
            }
        }
        if (diverge) break;

        // 3.5 更新状态
        prev_rms = rms_cur;
        prev_n_matched = (int)matched.size();
        rms_cur = rms_new;
        trans_cur = trans_new;
        matched_cur = matched;
        prev_cd_rel_change = cd_rel_change;
        result.cd_relative_change = std::max(result.cd_relative_change, cd_rel_change);

        // 3.6 收敛判断
        //   |Δp|_max < 1e-9 (用 cd_rel_change 近似, 因 CD 是主要参数)
        //   OR RMS 变化 < 0.1%
        bool conv_dp = (cd_rel_change < params.converge_dp_max);
        bool conv_rms = (prev_rms > 0) ?
                        (std::abs(rms_new - prev_rms) < prev_rms * params.converge_rms_rel) :
                        false;
        if (conv_dp || conv_rms) {
            if (logger) logger->infof(
                "  [robust_refine] iter %d: 收敛 (conv_dp=%d, conv_rms=%d)",
                iter, (int)conv_dp, (int)conv_rms);
            converged = true;
            break;
        }
    }

    result.n_iterations = iter + (converged ? 1 : 0);
    if (!converged && iter >= params.max_iterations) {
        if (logger) logger->infof("  [robust_refine] 达到最大迭代次数 %d", params.max_iterations);
    }

    // --- 4. 最终验收 (第 5 层防护) ---
    if (matched_cur.empty()) {
        if (logger) logger->warn("  [robust_refine] matched_cur 为空, 回退");
        return result;
    }
    if (rms_cur > initial_rms_arcsec * params.final_rms_tolerance) {
        if (logger) logger->warnf(
            "  [robust_refine] final RMS %.4f > %.4f×initial=%.4f, 回退",
            rms_cur, params.final_rms_tolerance, initial_rms_arcsec);
        return result;
    }
    if ((int)matched_cur.size() < params.min_matched_final) {
        if (logger) logger->warnf(
            "  [robust_refine] final matched %zu < %d, 回退",
            matched_cur.size(), params.min_matched_final);
        return result;
    }

    // --- 5. 成功, 填充结果 ---
    result.trans = trans_cur;
    // 将 matched.u 从 U_pool 索引转换为 U_full 索引
    // (matched.w 已经是 gaia_ra/dec 索引, 与 W_final 一致, 无需转换)
    result.matched.resize(matched_cur.size());
    for (size_t i = 0; i < matched_cur.size(); ++i) {
        result.matched[i].u = pool_idx[matched_cur[i].u];  // U_pool 索引 → U_full 索引
        result.matched[i].w = matched_cur[i].w;
    }
    result.rms_arcsec = rms_cur;
    result.rms_px = rms_cur / s0;
    result.n_matched = (int)matched_cur.size();
    result.fallback = false;
    result.success = true;

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "==== robust_refine_wcs 成功 ====\n"
            "  iter=%d, matched=%d, rms=%.4f\" (%.4f px), cd_Δ=%.4f%%, pool=%d",
            result.n_iterations, result.n_matched, result.rms_arcsec, result.rms_px,
            result.cd_relative_change * 100, result.n_pool);
        logger->info(buf);
    }

    return result;
}

} // namespace ipv

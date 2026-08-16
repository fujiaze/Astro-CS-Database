// ============================================================================
// ipv_solver.cpp - IPV 主求解器实现 ( 统一求解)
//
// 统一求解路径 (无 flip_mode 区分):
// 1. StarSelector (复用 ipv_select)
// 2. triangle_match (三角形匹配, 替代 polygon_match)
// 3. iter_trans_solve (sigma-clip 多项式拟合)
// 4. iterative_reproject (固定索引迭代重投影)
// 5. extract_wcs_sip (从 TRANS 提取 WCS + SIP)
//
// 关键约定 :
// - U (图像侧): 像素坐标, 原点图像中心, Y 轴向上
// - W (星表侧): 角秒坐标, 原点投影中心 (gnomonic xi/eta)
// - TRANS: U(像素) -> W(角秒), 线性项单位 = 角秒/像素, 常数项单位 = 角秒
// - CD = trans.x10 / 3600 (直接, 不用 M^-1)
// - SIP A/B = cd_inv · trans_high_order (解析公式)
// - SIP AP/BP = 网格反变换法 (NB_GRID_POINTS=7)
// - iterative_reproject 收敛阈值: sqrt(x00² + y00²) < 0.01" (角秒, 直接)
//
// 日期: 2026-07-05
// ============================================================================

#include "ipv_solver.h"

#include "ipv_select.h"
#include "ipv_triangle.h"
#include "ipv_itertrans.h"
#include "ipv_wcs.h"
#include "ipv_robust_refine.h"   // 鲁棒扩增精化
#include "ipv_log.h"

#include <chrono>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdio>
#include <omp.h>

namespace ipv {

namespace {
// 计算两个时间点之间的毫秒数 (用于阶段耗时日志)
inline double elapsed_ms(std::chrono::steady_clock::time_point start,
                         std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 物理常量 (与 ipv_select.cpp 保持一致)
static constexpr double IPV_PI = 3.14159265358979323846;
static constexpr double IPV_DEGTORAD = IPV_PI / 180.0;
static constexpr double IPV_RADTODEG = 180.0 / IPV_PI;
static constexpr double IPV_ASEC_PER_RAD = 206264.80624709636;

// Gnomonic 反向投影 (切平面 → 天球)
// 输入: xi_asec, eta_asec (角秒, 切平面坐标), ra0_deg, dec0_deg (切点)
// 输出: ra_deg, dec_deg (天球坐标)
// 公式 (标准 gnomonic 反投影):
// xi_rad = xi_asec / ARCSEC_PER_RAD
// eta_rad = eta_asec / ARCSEC_PER_RAD
// rho = sqrt(xi² + eta²)
// c = atan(rho)
// dec = asin(cos(c)*sin(dec0) + eta*sin(c)*cos(dec0)/rho)
// ra = ra0 + atan2(xi*sin(c), rho*cos(dec0)*cos(c) - eta*sin(dec0)*sin(c))
// rho = 0 时返回 (ra0, dec0)
void gnomonic_inverse_proj(
    double xi_asec, double eta_asec,
    double ra0_deg, double dec0_deg,
    double& ra_deg, double& dec_deg)
{
    const double ra0 = ra0_deg * IPV_DEGTORAD;
    const double dec0 = dec0_deg * IPV_DEGTORAD;
    const double xi_rad  = xi_asec  / IPV_ASEC_PER_RAD;
    const double eta_rad = eta_asec / IPV_ASEC_PER_RAD;
    const double rho = std::sqrt(xi_rad * xi_rad + eta_rad * eta_rad);

    if (rho < 1e-12) {
        ra_deg  = ra0_deg;
        dec_deg = dec0_deg;
        return;
    }

    const double c = std::atan(rho);
    const double sin_c = std::sin(c);
    const double cos_c = std::cos(c);
    const double sin_dec0 = std::sin(dec0);
    const double cos_dec0 = std::cos(dec0);

    const double dec = std::asin(cos_c * sin_dec0 +
                                  eta_rad * sin_c * cos_dec0 / rho);
    const double ra = ra0 + std::atan2(xi_rad * sin_c,
                                       rho * cos_dec0 * cos_c -
                                       eta_rad * sin_dec0 * sin_c);

    ra_deg  = ra  * IPV_RADTODEG;
    dec_deg = dec * IPV_RADTODEG;

    // 规范化 ra 到 [0, 360)
    while (ra_deg < 0.0)    ra_deg += 360.0;
    while (ra_deg >= 360.0) ra_deg -= 360.0;
}

// Gnomonic 正向投影 (与 ipv_select.cpp 一致, 内部复制以避免符号依赖)
// 将天球坐标 (ra, dec) 投影到以 (ra0, dec0) 为中心的切平面
// 输出: xi, eta (角秒), valid
void gnomonic_forward_proj_solver(
    double ra_deg, double dec_deg,
    double ra0_deg, double dec0_deg,
    double& xi_asec, double& eta_asec, bool& valid)
{
    const double ra = ra_deg * IPV_DEGTORAD;
    const double dec = dec_deg * IPV_DEGTORAD;
    const double ra0 = ra0_deg * IPV_DEGTORAD;
    const double dec0 = dec0_deg * IPV_DEGTORAD;

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

    xi_asec  = valid ? xi_rad  * IPV_ASEC_PER_RAD : 0.0;
    eta_asec = valid ? eta_rad * IPV_ASEC_PER_RAD : 0.0;
}

} // namespace

// ===========================================================================
// iterative_reproject: 迭代重投影 (固定索引策略)
//
// 流程:
// 1. apply_match: 用 TRANS 常数项 x00/y00 反推新中心
// (: TRANS 常数项已是角秒, 直接用, 不乘 s0)
// 2. project_catalog_stars: 用新中心重新 gnomonic 投影 Gaia
// 3. update_stars_positions: 按固定索引更新 W 坐标 (不重新匹配!)
// 4. atRecalcTrans: 用相同匹配对重拟合 TRANS
// 5. 收敛判定: sqrt(x00² + y00²) < 0.01" (角秒, 直接, 不乘 s0)
//
// 关键: 匹配关系保持不变 (固定索引), 只更新 W 的坐标, 然后重拟合 TRANS。
// ===========================================================================
IterativeReprojectResult iterative_reproject(
    const std::vector<StarPoint>& U,
    const std::vector<double>& gaia_ra,
    const std::vector<double>& gaia_dec,
    const Trans& initial_trans,
    const std::vector<MatchPair>& initial_inliers,
    double ra0, double dec0,
    double s0,
    int img_width, int img_height,
    Logger* logger)
{
    IterativeReprojectResult result;
    result.trans = initial_trans;
    result.matched = initial_inliers;
    result.ra0 = ra0;
    result.dec0 = dec0;
    result.success = false;

    if (logger) {
        logger->infof("  [诊断] iterative_reproject 入口: U.size=%zu, gaia_ra.size=%zu, "
                      "gaia_dec.size=%zu, initial_inliers.size=%zu, s0=%.4f, img=%dx%d",
                      U.size(), gaia_ra.size(), gaia_dec.size(),
                      initial_inliers.size(), s0, img_width, img_height);
        // 检查 inliers 索引是否在 U 范围内
        int u_oob = 0, w_oob = 0;
        for (const auto& mp : initial_inliers) {
            if (mp.u < 0 || mp.u >= (int)U.size()) u_oob++;
            if (mp.w < 0 || mp.w >= (int)gaia_ra.size()) w_oob++;
        }
        if (u_oob > 0 || w_oob > 0) {
            logger->warnf("  [诊断] inliers 索引越界: u_oob=%d, w_oob=%d", u_oob, w_oob);
        }
        // 检查 gaia_ra 和 gaia_dec 长度一致
        if (gaia_ra.size() != gaia_dec.size()) {
            logger->warnf("  [诊断] gaia_ra/dec 长度不一致: %zu vs %zu",
                          gaia_ra.size(), gaia_dec.size());
        }
    }

    if (gaia_ra.empty() || gaia_dec.empty() || initial_inliers.empty()) {
        if (logger) logger->warn("iterative_reproject: 输入为空, 跳过");
        return result;
    }
    if (gaia_ra.size() != gaia_dec.size()) {
        if (logger) logger->warn("iterative_reproject: gaia_ra/dec 长度不匹配, 跳过");
        return result;
    }

    const int N_W = (int)gaia_ra.size();
    const int MAX_ITERS = 5;
    const double CONV_THRESH_ARCSEC = 0.01;  // 收敛阈值 0.01 arcsec

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "iterative_reproject: 开始 (N_W=%d, n_inliers=%d, s0=%.4f\"/px, "
            "max_iter=%d, conv_thresh=%.4f\")",
            N_W, (int)initial_inliers.size(), s0, MAX_ITERS, CONV_THRESH_ARCSEC);
        logger->info(buf);
    }

    double ra_cur = ra0;
    double dec_cur = dec0;
    Trans trans_cur = initial_trans;
    // 固定索引策略: 固定 inliers_cur, 不重新匹配
    std::vector<MatchPair> inliers_cur = initial_inliers;

    // 初始收敛值 (: TRANS 常数项已是角秒, 不乘 s0)
    double conv = std::sqrt(
        trans_cur.x00 * trans_cur.x00 +
        trans_cur.y00 * trans_cur.y00);

    int trial = 0;
    bool converged = false;

    while (conv > CONV_THRESH_ARCSEC && trial < MAX_ITERS) {
        // 1. apply_match: 用 TRANS 常数项反推新中心
        // TRANS: U(像素)→W(角秒), 常数项 x00/y00 直接是角秒
        const double xi_center_asec  = trans_cur.x00;
        const double eta_center_asec = trans_cur.y00;

        if (logger) {
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "  iter %d: ra_cur=%.6f dec_cur=%.6f, xi_center=%.4f\" eta_center=%.4f\", "
                "conv=%.4f\" (thresh=%.4f\")",
                trial, ra_cur, dec_cur, xi_center_asec, eta_center_asec,
                conv, CONV_THRESH_ARCSEC);
            logger->info(buf);
        }

        // 反投影到天球 (de-project)
        double ra_new, dec_new;
        gnomonic_inverse_proj(xi_center_asec, eta_center_asec,
                              ra_cur, dec_cur, ra_new, dec_new);

        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "  iter %d: 新中心 ra=%.6f dec=%.6f (Δra=%.6f° Δdec=%.6f°)",
                trial, ra_new, dec_new,
                ra_new - ra_cur, dec_new - dec_cur);
            logger->info(buf);
        }

        // 更新中心
        ra_cur = ra_new;
        dec_cur = dec_new;

        // 2. project_catalog_stars: 重新 gnomonic 投影 Gaia 星
        // OpenMP 并行化: gnomonic_forward_proj_solver 是纯函数, 可安全并行
        std::vector<StarPoint> W_new(N_W);
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < N_W; ++i) {
            double xi, eta;
            bool valid;
            gnomonic_forward_proj_solver(gaia_ra[i], gaia_dec[i],
                                          ra_cur, dec_cur, xi, eta, valid);
            if (!valid) {
                W_new[i].x = 1e18;
                W_new[i].y = 1e18;
            } else {
                // W 直接用角秒 (TRANS: U(像素)->W(角秒))
                W_new[i].x = xi;   // 角秒
                W_new[i].y = eta;  // 角秒
            }
            W_new[i].flux = 0.0;
            W_new[i].saturated = false;
        }

        // 3. update_stars_positions + atRecalcTrans:
        // 按固定 inliers_cur 重拟合 TRANS (不重新匹配!)
        // 过滤掉无效 W_new (坐标 1e18) 的 inliers
        std::vector<MatchPair> valid_inliers;
        valid_inliers.reserve(inliers_cur.size());
        for (const auto& mp : inliers_cur) {
            if (mp.u < 0 || mp.u >= (int)U.size()) continue;
            if (mp.w < 0 || mp.w >= N_W) continue;
            const StarPoint& w = W_new[mp.w];
            if (std::fabs(w.x) < 1e10 && std::fabs(w.y) < 1e10) {
                valid_inliers.push_back(mp);
            }
        }

        if ((int)valid_inliers.size() < 3) {
            if (logger) logger->warnf("  iter %d: 有效 inliers 不足 3 (%d), 终止迭代",
                                       trial, (int)valid_inliers.size());
            break;
        }

        // 4. atRecalcTrans: 用固定 inliers + 更新坐标重拟合 TRANS
        IterTransResult refit = at_recalc_trans(U, W_new, valid_inliers, trans_cur.order);
        if (!refit.success) {
            if (logger) logger->warnf("  iter %d: at_recalc_trans 失败, 终止迭代", trial);
            break;
        }

        if (logger) {
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "  iter %d: at_recalc_trans 成功, n=%d, sig=%.4f, rms=%.4f, "
                "x00=%.4f y00=%.4f (asec)",
                trial, refit.n_inliers, refit.trans.sig, refit.rms,
                refit.trans.x00, refit.trans.y00);
            logger->info(buf);
        }

        trans_cur = refit.trans;
        inliers_cur = valid_inliers;

        // 5. 收敛判定 (TRANS 常数项 < 0.01"; : x00/y00 已是角秒, 不乘 s0)
        conv = std::sqrt(
            trans_cur.x00 * trans_cur.x00 +
            trans_cur.y00 * trans_cur.y00);

        trial++;

        if (conv <= CONV_THRESH_ARCSEC) {
            converged = true;
            if (logger) logger->infof("  iter %d: 收敛 (TRANS 常数项 = %.4f\" < %.4f\")",
                                       trial, conv, CONV_THRESH_ARCSEC);
            break;
        }
    }

    if (!converged && logger) {
        logger->infof("iterative_reproject: 达到最大迭代次数 %d, 未收敛 (conv=%.4f\")",
                      trial, conv);
    }

    // 最终结果
    result.trans = trans_cur;
    result.matched = inliers_cur;
    result.ra0 = ra_cur;
    result.dec0 = dec_cur;
    result.n_matched = (int)inliers_cur.size();
    result.n_iterations = trial;
    result.convergence = conv;
    result.success = ((int)inliers_cur.size() >= 3);

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "iterative_reproject: 完成 %s, ra=%.6f→%.6f dec=%.6f→%.6f, "
            "n_matched=%d (固定), iters=%d, conv=%.4f\"",
            converged ? "(收敛)" : "(未收敛)",
            ra0, ra_cur, dec0, dec_cur,
            result.n_matched, trial, conv);
        logger->info(buf);
    }

    return result;
}

// ===========================================================================
// 构造 / 析构
// ===========================================================================

IPVSolver::IPVSolver() = default;
IPVSolver::~IPVSolver() = default;

void IPVSolver::set_gaia_handle(intptr_t handle) {
    gaia_handle_ = handle;
}

void IPVSolver::set_detector_handle(intptr_t handle) {
    detector_handle_ = handle;
}

// ===========================================================================
// solve: 主求解函数 ( 统一路径)
//
// 流程:
// 1. StarSelector (复用 ipv_select)
// 2. triangle_match (三角形匹配)
// 3. iter_trans_solve (sigma-clip 多项式 TRANS 拟合)
// 4. iterative_reproject (固定索引迭代重投影)
// 5. extract_wcs_sip (从 TRANS 提取 WCS + SIP)
// ===========================================================================
void IPVSolver::solve(
    const std::string& image_path,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    WcsFitResult* result
) {
    // 失败结果 (聚合体零初始化)
    WcsFitResult fail_result{};
    fail_result.trans_order = 0;

    // 1. 初始化日志
    if (params.log_dir != nullptr && params.log_dir[0] != '\0') {
        std::string dir(params.log_dir);
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
            dir.pop_back();
        }
        logger_.init(dir + "/ipv_solver.log");
        // 同步初始化子模块日志器, 使 triangle/itertrans 日志写入文件
        init_triangle_logger(dir + "/ipv_triangle.log");
        init_itertrans_logger(dir + "/ipv_itertrans.log");
    }

    logger_.info("==== IPVSolver::solve 开始 (V4.19 统一求解) ====");
    logger_.infof("  image_path=%s", image_path.c_str());
    logger_.infof("  ra0=%.6f deg, dec0=%.6f deg", ra0, dec0);
    logger_.infof("  focal_length=%.3f mm, pixel_size=%.3f um",
                  focal_length_mm, pixel_size_um);
    logger_.infof("  gaia_handle=%lld, detector_handle=%lld",
                  (long long)gaia_handle_, (long long)detector_handle_);

    // 2. StarSelector + triangle_match
    // 直接用 60 颗星做三角形匹配, 不做自适应扩充
    // 之前 用 20 颗起跑, 投票矩阵不稳定, 初始 6 对中出现重复星
    // (同一 u 匹配多个 w), 导致初始 TRANS 被扭曲, iter_trans 失败
    // C(60,3)=34220 三角形, 配合 scale 约束 (±20%) 过滤错误配对
    auto t_sel_start = std::chrono::steady_clock::now();
    StarSelection selection;
    int ret = 0;
    TriangleMatchResult tri_result{};  // 零初始化 (max_vote=0, success=false)
    int vote_threshold = params.vote_threshold;
    int n_target_used = 0;

    for (int n_target : {60}) {
        // 用 n_target 覆盖 img_n_target, 重新选星 + 查询 Gaia
        IPVSolverParams p_adapt = params;
        p_adapt.img_n_target = n_target;

        selection = StarSelection{};
        ret = ipv_select(image_path, ra0, dec0, focal_length_mm, pixel_size_um,
                         p_adapt, selection, &logger_);
        if (ret != 0 || !selection.success) {
            logger_.warnf("[V4.22] ipv_select 失败 n_target=%d, ret=%d, 跳过扩充",
                          n_target, ret);
            continue;
        }

        auto t_tri_start = std::chrono::steady_clock::now();
        // 传入 selection.s0 启用 scale 约束 (±20%)
        tri_result = triangle_match(selection.U, selection.W, n_target, n_target,
                                     0.002, selection.s0);
        auto t_tri_end = std::chrono::steady_clock::now();

        logger_.infof("[V4.22] n_target=%d, max_vote=%d, threshold=%d",
                      n_target, tri_result.max_vote, vote_threshold);
        logger_.infof("  [阶段] triangle_match (n=%d): %.2f ms, max_vote=%d, top_pairs=%d, success=%d",
                      n_target, elapsed_ms(t_tri_start, t_tri_end),
                      tri_result.max_vote,
                      (int)tri_result.top_pairs.size(),
                      (int)tri_result.success);

        n_target_used = n_target;
        if (tri_result.max_vote >= vote_threshold) {
            logger_.infof("[V4.22] max_vote=%d >= threshold=%d, 停止扩充",
                          tri_result.max_vote, vote_threshold);
            break;
        }
        logger_.warnf("[V4.22] max_vote=%d < threshold=%d, 扩充星数到下一档",
                      tri_result.max_vote, vote_threshold);
    }
    auto t_sel_end = std::chrono::steady_clock::now();
    logger_.infof("  [阶段] StarSelector+triangle_match (V4.22 自适应): %.2f ms, n_target_used=%d",
                  elapsed_ms(t_sel_start, t_sel_end), n_target_used);

    if (ret != 0 || !selection.success) {
        logger_.error("StarSelector 失败, 终止求解");
        logger_.errorf("  诊断: ret=%d, success=%d, img=%dx%d, fov=%.4f deg",
                       ret, (int)selection.success,
                       selection.img_width, selection.img_height,
                       selection.fov_diag_deg);
        *result = fail_result;
        return;
    }

    // 3. 提取 U / W / s0 / 图像尺寸
    const std::vector<StarPoint>& U = selection.U;
    const std::vector<StarPoint>& W = selection.W;
    double s0 = selection.s0;
    int img_width = selection.img_width;
    int img_height = selection.img_height;

    logger_.infof("  N_U=%d, N_W=%d, s0=%.4f arcsec/px, img=%dx%d, fov_diag=%.4f deg, n_target_used=%d",
                  (int)U.size(), (int)W.size(), s0, img_width, img_height,
                  selection.fov_diag_deg, n_target_used);

    // 4. triangle_match 失败判断 (循环结束后统一判断)
    if (!tri_result.success || tri_result.max_vote < 3) {
        logger_.error("triangle_match 失败或票数不足, 终止求解");
        logger_.errorf("  诊断: success=%d, max_vote=%d (阈值=3), n_target_used=%d",
                       (int)tri_result.success, tri_result.max_vote, n_target_used);
        *result = fail_result;
        return;
    }

    // 5. iter_trans 求解 (sigma-clip 多项式拟合)
    // order 阶数回退机制 (order=3 失败→order=2→order=1)
    // 原因: 部分帧三角形匹配产生大量错误配对, 绝对阈值剔除后幸存对数不足 order=3 所需的 10 对
    // 策略: 从 order=3 逐阶降级, 直到成功或全部失败
    auto t_it_start = std::chrono::steady_clock::now();
    IterTransResult it_result;
    bool it_success = false;
    for (int it_order = 3; it_order >= 1; --it_order) {
        it_result = iter_trans_solve(
            U, W, tri_result.top_pairs, 5.0, it_order);
        if (it_result.success && it_result.n_inliers >= 3) {
            it_success = true;
            if (it_order < 3) {
                logger_.warnf("iter_trans_solve order=3 失败, 回退到 order=%d 成功 (n_inliers=%d)",
                              it_order, it_result.n_inliers);
            }
            break;
        }
        logger_.warnf("iter_trans_solve order=%d 失败 (success=%d, n_inliers=%d)",
                      it_order, (int)it_result.success, it_result.n_inliers);
    }
    auto t_it_end = std::chrono::steady_clock::now();
    // 诊断: it_result 字段检查 (定位崩溃)
    logger_.infof("  [诊断] it_result 检查: n_inliers=%d, inliers.size=%zu, rms=%.4f, "
                  "n_iterations=%d, success=%d, trans.order=%d, trans.valid=%d",
                  it_result.n_inliers, it_result.inliers.size(), it_result.rms,
                  it_result.n_iterations, (int)it_result.success,
                  it_result.trans.order, (int)it_result.trans.valid);
    logger_.infof("  [诊断] it_result.trans: x00=%.4f y00=%.4f x10=%.6f x01=%.6f y10=%.6f y01=%.6f",
                  it_result.trans.x00, it_result.trans.y00,
                  it_result.trans.x10, it_result.trans.x01,
                  it_result.trans.y10, it_result.trans.y01);
    logger_.infof("  [诊断] it_result.inliers 前5对:");
    for (size_t i = 0; i < it_result.inliers.size() && i < 5; ++i) {
        logger_.infof("    inliers[%zu]: u=%d w=%d", i,
                      it_result.inliers[i].u, it_result.inliers[i].w);
    }
    logger_.infof("  [诊断] 准备打印阶段耗时日志...");
    // 强制刷新日志
    fflush(stdout);
    fflush(stderr);
    logger_.infof("  [阶段] iter_trans_solve: %.2f ms, n_inliers=%d, rms=%.4f, iters=%d, success=%d",
                  elapsed_ms(t_it_start, t_it_end),
                  it_result.n_inliers, it_result.rms,
                  it_result.n_iterations, (int)it_result.success);

    if (!it_success) {
        logger_.error("iter_trans_solve 全部阶数失败, 终止求解");
        logger_.errorf("  诊断: success=%d, n_inliers=%d (阈值=3)",
                       (int)it_result.success, it_result.n_inliers);
        *result = fail_result;
        return;
    }

    logger_.infof("  iter_trans TRANS: order=%d, x00=%.4f y00=%.4f (px), "
                  "x10=%.6f x01=%.6f y10=%.6f y01=%.6f",
                  it_result.trans.order,
                  it_result.trans.x00, it_result.trans.y00,
                  it_result.trans.x10, it_result.trans.x01,
                  it_result.trans.y10, it_result.trans.y01);

    // 6. 迭代重投影 (固定索引策略)
    logger_.info("  [诊断] 准备调用 iterative_reproject...");
    fflush(stdout); fflush(stderr);
    auto t_rep_start = std::chrono::steady_clock::now();
    IterativeReprojectResult rep_result = iterative_reproject(
        U, selection.gaia_ra, selection.gaia_dec,
        it_result.trans, it_result.inliers,
        ra0, dec0, s0, img_width, img_height, &logger_);
    auto t_rep_end = std::chrono::steady_clock::now();
    logger_.infof("  [诊断] iterative_reproject 返回: success=%d, n_matched=%d, "
                  "n_iterations=%d, convergence=%.4f, ra0=%.6f, dec0=%.6f, "
                  "matched.size=%zu, trans.order=%d",
                  (int)rep_result.success, rep_result.n_matched,
                  rep_result.n_iterations, rep_result.convergence,
                  rep_result.ra0, rep_result.dec0,
                  rep_result.matched.size(), rep_result.trans.order);
    fflush(stdout); fflush(stderr);
    logger_.infof("  [阶段] iterative_reproject: %.2f ms, success=%d, "
                  "ra=%.6f→%.6f, dec=%.6f→%.6f, n_matched=%d, iters=%d, conv=%.4f\"",
                  elapsed_ms(t_rep_start, t_rep_end), (int)rep_result.success,
                  ra0, rep_result.ra0, dec0, rep_result.dec0,
                  rep_result.n_matched, rep_result.n_iterations,
                  rep_result.convergence);

    if (!rep_result.success || rep_result.n_matched < 3) {
        logger_.error("iterative_reproject 失败或匹配数不足, 终止求解");
        logger_.errorf("  诊断: success=%d, n_matched=%d (阈值=3)",
                       (int)rep_result.success, rep_result.n_matched);
        *result = fail_result;
        return;
    }

    // 7. 用收敛后的中心重新投影 Gaia, 得到 W_final (与 rep_result.matched 索引一致)
    // W 直接用角秒 (TRANS: U(像素)->W(角秒))
    const int N_W = (int)selection.gaia_ra.size();
    std::vector<StarPoint> W_final(N_W);
    int n_invalid_proj = 0;
    for (int i = 0; i < N_W; ++i) {
        double xi, eta;
        bool valid;
        gnomonic_forward_proj_solver(
            selection.gaia_ra[i], selection.gaia_dec[i],
            rep_result.ra0, rep_result.dec0, xi, eta, valid);
        if (!valid) {
            W_final[i].x = 1e18;
            W_final[i].y = 1e18;
            ++n_invalid_proj;
        } else {
            W_final[i].x = xi;    // 角秒 (不再 /s0)
            W_final[i].y = eta;   // 角秒 (不再 /s0)
        }
        W_final[i].flux = 0.0;
        W_final[i].saturated = false;
    }

    if (n_invalid_proj > 0) {
        logger_.warnf("  W_final 投影: %d 颗无效 (将过滤)", n_invalid_proj);
    }

    // 7.5. 高阶重匹配 (iterative_reproject 末尾 recalc=YES 二轮精化)
    // 用收敛后的高阶 TRANS 在 U-W_final 空间双向匹配, 重拟合 TRANS (最多 5 次迭代)
    // 仅当 trans.order > 1 时触发 (order=1 无畸变项, 重匹配无意义)
    if (rep_result.trans.order > 1) {
        auto t_himatch_start = std::chrono::steady_clock::now();
        const int order_hi = rep_result.trans.order;
        // AT_MATCH_REQUIRE_CUBIC=10, QUADRATIC=6, LINEAR=3 (ipv_itertrans.cpp 内 static 常量)
        const int min_pairs_hi = (order_hi == 3) ? 10 :
                                  (order_hi == 2) ? 6 : 3;

        std::vector<MatchPair> hi_matches = at_match_lists(
            U, W_final, rep_result.trans, 5.0);  // 5" tolerance
        logger_.infof("  [阶段] hi_order_rematch iter 0: n_matched=%zu (min=%d, order=%d)",
                      hi_matches.size(), min_pairs_hi, order_hi);

        if ((int)hi_matches.size() >= min_pairs_hi) {
            IterTransResult hi_result = at_recalc_trans(U, W_final, hi_matches, order_hi);
            if (hi_result.success) {
                // 迭代精化 (最多 5 次)
                for (int iter = 0; iter < 5; ++iter) {
                    std::vector<MatchPair> new_matches = at_match_lists(
                        U, W_final, hi_result.trans, 5.0);
                    if ((int)new_matches.size() < min_pairs_hi) break;
                    IterTransResult new_result = at_recalc_trans(
                        U, W_final, new_matches, order_hi);
                    if (!new_result.success) break;

                    // 收敛判断: 匹配数变化 ≤1 且 RMS 变化 < 1%
                    int dn = (int)new_matches.size() - (int)hi_matches.size();
                    double drms = std::fabs(new_result.rms - hi_result.rms);
                    bool conv_match = (std::abs(dn) <= 1);
                    bool conv_rms = (hi_result.rms > 0) ?
                                    (drms < hi_result.rms * 0.01) : true;

                    hi_result = new_result;
                    hi_matches = new_matches;

                    if (conv_match && conv_rms) {
                        logger_.infof("  [阶段] hi_order_rematch iter %d: 收敛 (dn=%d, drms=%.4f)",
                                      iter + 1, dn, drms);
                        break;
                    }
                    logger_.infof("  [阶段] hi_order_rematch iter %d: n_matched=%zu, rms=%.4f",
                                  iter + 1, hi_matches.size(), hi_result.rms);
                }
                // 用精化后的结果替换 rep_result
                rep_result.trans = hi_result.trans;
                rep_result.matched = hi_matches;
                rep_result.n_matched = (int)hi_matches.size();
            }
        }
        auto t_himatch_end = std::chrono::steady_clock::now();
        logger_.infof("  [阶段] hi_order_rematch: %.2f ms, n_matched=%d, trans.order=%d",
                      elapsed_ms(t_himatch_start, t_himatch_end),
                      rep_result.n_matched, rep_result.trans.order);
    }

    // 8.5 : 鲁棒扩增精化 (网格采样 + IRLS + CD 阻尼)
    // 在 hi_order_rematch 之后、extract_wcs_sip 之前
    // 失败时回退到 hi_order_rematch 结果, 不破坏 99.87% 成功率
    bool robust_refine_applied = false;
    if (!selection.U_full.empty() && rep_result.trans.order > 1) {
        auto t_robust_start = std::chrono::steady_clock::now();
        // 初始 RMS: hi_order_rematch 的 sig (角秒, 68.3% 百分位)
        double initial_rms_arcsec = rep_result.trans.sig;
        if (initial_rms_arcsec <= 0) initial_rms_arcsec = 1.0;

        RobustRefineResult rob = robust_refine_wcs(
            rep_result.trans,
            selection.U_full, selection.mag_full,
            selection.gaia_ra, selection.gaia_dec,
            rep_result.ra0, rep_result.dec0,
            s0, initial_rms_arcsec,
            selection.fov_diag_deg,
            img_width, img_height,
            RobustRefineParams{}, &logger_);

        if (rob.success && !rob.fallback) {
            rep_result.trans = rob.trans;
            rep_result.matched = rob.matched;     // matched.u 已转 U_full 索引
            rep_result.n_matched = rob.n_matched;
            robust_refine_applied = true;
            logger_.infof("  [阶段] robust_refine: 成功, n_matched=%d, rms=%.4f\", "
                          "iter=%d, pool=%d, cd_Δ=%.4f%%",
                          rob.n_matched, rob.rms_arcsec, rob.n_iterations,
                          rob.n_pool, rob.cd_relative_change * 100);
        } else {
            logger_.infof("  [阶段] robust_refine: 回退 (fallback=%d), 使用 hi_order_rematch 结果",
                          (int)rob.fallback);
        }
        auto t_robust_end = std::chrono::steady_clock::now();
        logger_.infof("  [阶段] robust_refine: %.2f ms",
                      elapsed_ms(t_robust_start, t_robust_end));
    }

    // 8.6 选择传给 extract_wcs_sip 的 U 向量
    // 若 robust_refine 成功, matched.u 是 U_full 索引, 必须传 U_full
    // 否则 matched.u 是 U (60 颗) 索引, 传 U
    const std::vector<StarPoint>& U_for_wcs =
        robust_refine_applied ? selection.U_full : U;

    // 9. WCS+SIP 从 TRANS 提取
    logger_.infof("  [诊断] 准备调用 extract_wcs_sip: W_final.size=%zu, matched.size=%zu, "
                  "U_for_wcs.size=%zu (robust=%d)",
                  W_final.size(), rep_result.matched.size(),
                  U_for_wcs.size(), (int)robust_refine_applied);

    auto t_wcs_start = std::chrono::steady_clock::now();
    extract_wcs_sip(
        rep_result.trans, rep_result.ra0, rep_result.dec0,
        img_width, img_height, s0,
        U_for_wcs, W_final, rep_result.matched, result, &logger_);
    auto t_wcs_end = std::chrono::steady_clock::now();

    {
        double wcs_ms = elapsed_ms(t_wcs_start, t_wcs_end);
        logger_.infof("  [阶段] extract_wcs_sip: %.2f ms, success=%d, n_pairs=%d, "
                      "rms_px=%.3f, rms_arcsec=%.3f, trans_order=%d, sip_order=%d",
                      wcs_ms, (int)result->success, result->n_pairs,
                      result->rms_px, result->rms_arcsec,
                      result->trans_order, result->sip.order);
    }

    // v1.3: 缓存最终权威 inlier 数据 (供 WCS Gate v2 双层闭环)
    cache_last_inliers_(
        rep_result.matched, U_for_wcs,
        selection.gaia_ra, selection.gaia_dec,
        rep_result.trans, s0,
        rep_result.ra0, rep_result.dec0,
        img_width, img_height,
        robust_refine_applied);

    // 9. 最终日志
    logger_.info("==== IPVSolver::solve 完成 (V4.22) ====");
    logger_.infof("  最终: n_pairs=%d, rms_px=%.3f, rms_arcsec=%.3f, "
                  "trans_order=%d, sip_order=%d, success=%d",
                  result->n_pairs, result->rms_px, result->rms_arcsec,
                  result->trans_order, result->sip.order, (int)result->success);
}

// ===========================================================================
// solve_from_memory: 从内存像素数据求解 (不读文件)
//
// 与 solve() 算法完全一致, 区别:
// - 直接接受 float* pixels 参数, 跳过文件读取
// - 调用 ipv_select_from_memory 而非 ipv_select
// ===========================================================================
void IPVSolver::solve_from_memory(
    const float* pixels,
    int width, int height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    WcsFitResult* result
) {
    // 失败结果 (聚合体零初始化)
    WcsFitResult fail_result{};
    fail_result.trans_order = 0;

    // 1. 初始化日志
    if (params.log_dir != nullptr && params.log_dir[0] != '\0') {
        std::string dir(params.log_dir);
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
            dir.pop_back();
        }
        logger_.init(dir + "/ipv_solver.log");
        init_triangle_logger(dir + "/ipv_triangle.log");
        init_itertrans_logger(dir + "/ipv_itertrans.log");
    }

    logger_.info("==== IPVSolver::solve_from_memory 开始 (V4.19 统一求解) ====");
    logger_.infof("  pixels=%dx%d (内存数据)", width, height);
    logger_.infof("  ra0=%.6f deg, dec0=%.6f deg", ra0, dec0);
    logger_.infof("  focal_length=%.3f mm, pixel_size=%.3f um",
                  focal_length_mm, pixel_size_um);
    logger_.infof("  gaia_handle=%lld, detector_handle=%lld",
                  (long long)gaia_handle_, (long long)detector_handle_);

    // 2. StarSelector + triangle_match
    auto t_sel_start = std::chrono::steady_clock::now();
    StarSelection selection;
    int ret = 0;
    TriangleMatchResult tri_result{};
    int vote_threshold = params.vote_threshold;
    int n_target_used = 0;

    for (int n_target : {60}) {
        IPVSolverParams p_adapt = params;
        p_adapt.img_n_target = n_target;

        selection = StarSelection{};
        ret = ipv_select_from_memory(pixels, width, height, ra0, dec0,
                                      focal_length_mm, pixel_size_um,
                                      p_adapt, selection, &logger_);
        if (ret != 0 || !selection.success) {
            logger_.warnf("[V4.22] ipv_select_from_memory 失败 n_target=%d, ret=%d, 跳过扩充",
                          n_target, ret);
            continue;
        }

        auto t_tri_start = std::chrono::steady_clock::now();
        tri_result = triangle_match(selection.U, selection.W, n_target, n_target,
                                     0.002, selection.s0);
        auto t_tri_end = std::chrono::steady_clock::now();

        logger_.infof("[V4.22] n_target=%d, max_vote=%d, threshold=%d",
                      n_target, tri_result.max_vote, vote_threshold);
        logger_.infof("  [阶段] triangle_match (n=%d): %.2f ms, max_vote=%d, top_pairs=%d, success=%d",
                      n_target, elapsed_ms(t_tri_start, t_tri_end),
                      tri_result.max_vote,
                      (int)tri_result.top_pairs.size(),
                      (int)tri_result.success);

        n_target_used = n_target;
        if (tri_result.max_vote >= vote_threshold) {
            logger_.infof("[V4.22] max_vote=%d >= threshold=%d, 停止扩充",
                          tri_result.max_vote, vote_threshold);
            break;
        }
        logger_.warnf("[V4.22] max_vote=%d < threshold=%d, 扩充星数到下一档",
                      tri_result.max_vote, vote_threshold);
    }
    auto t_sel_end = std::chrono::steady_clock::now();
    logger_.infof("  [阶段] StarSelector+triangle_match (V4.22 自适应): %.2f ms, n_target_used=%d",
                  elapsed_ms(t_sel_start, t_sel_end), n_target_used);

    if (ret != 0 || !selection.success) {
        logger_.error("StarSelector 失败, 终止求解");
        logger_.errorf("  诊断: ret=%d, success=%d, img=%dx%d, fov=%.4f deg",
                       ret, (int)selection.success,
                       selection.img_width, selection.img_height,
                       selection.fov_diag_deg);
        *result = fail_result;
        return;
    }

    // 3. 提取 U / W / s0 / 图像尺寸
    const std::vector<StarPoint>& U = selection.U;
    const std::vector<StarPoint>& W = selection.W;
    double s0 = selection.s0;
    int img_width = selection.img_width;
    int img_height = selection.img_height;

    logger_.infof("  N_U=%d, N_W=%d, s0=%.4f arcsec/px, img=%dx%d, fov_diag=%.4f deg, n_target_used=%d",
                  (int)U.size(), (int)W.size(), s0, img_width, img_height,
                  selection.fov_diag_deg, n_target_used);

    // 4. triangle_match 失败判断
    if (!tri_result.success || tri_result.max_vote < 3) {
        logger_.error("triangle_match 失败或票数不足, 终止求解");
        logger_.errorf("  诊断: success=%d, max_vote=%d (阈值=3), n_target_used=%d",
                       (int)tri_result.success, tri_result.max_vote, n_target_used);
        *result = fail_result;
        return;
    }

    // 5. iter_trans 求解
    auto t_it_start = std::chrono::steady_clock::now();
    IterTransResult it_result;
    bool it_success = false;
    for (int it_order = 3; it_order >= 1; --it_order) {
        it_result = iter_trans_solve(
            U, W, tri_result.top_pairs, 5.0, it_order);
        if (it_result.success && it_result.n_inliers >= 3) {
            it_success = true;
            if (it_order < 3) {
                logger_.warnf("iter_trans_solve order=3 失败, 回退到 order=%d 成功 (n_inliers=%d)",
                              it_order, it_result.n_inliers);
            }
            break;
        }
        logger_.warnf("iter_trans_solve order=%d 失败 (success=%d, n_inliers=%d)",
                      it_order, (int)it_result.success, it_result.n_inliers);
    }
    auto t_it_end = std::chrono::steady_clock::now();
    logger_.infof("  [诊断] it_result 检查: n_inliers=%d, inliers.size=%zu, rms=%.4f, "
                  "n_iterations=%d, success=%d, trans.order=%d, trans.valid=%d",
                  it_result.n_inliers, it_result.inliers.size(), it_result.rms,
                  it_result.n_iterations, (int)it_result.success,
                  it_result.trans.order, (int)it_result.trans.valid);
    logger_.infof("  [诊断] it_result.trans: x00=%.4f y00=%.4f x10=%.6f x01=%.6f y10=%.6f y01=%.6f",
                  it_result.trans.x00, it_result.trans.y00,
                  it_result.trans.x10, it_result.trans.x01,
                  it_result.trans.y10, it_result.trans.y01);
    logger_.infof("  [诊断] it_result.inliers 前5对:");
    for (size_t i = 0; i < it_result.inliers.size() && i < 5; ++i) {
        logger_.infof("    inliers[%zu]: u=%d w=%d", i,
                      it_result.inliers[i].u, it_result.inliers[i].w);
    }
    logger_.infof("  [诊断] 准备打印阶段耗时日志...");
    fflush(stdout);
    fflush(stderr);
    logger_.infof("  [阶段] iter_trans_solve: %.2f ms, n_inliers=%d, rms=%.4f, iters=%d, success=%d",
                  elapsed_ms(t_it_start, t_it_end),
                  it_result.n_inliers, it_result.rms,
                  it_result.n_iterations, (int)it_result.success);

    if (!it_success) {
        logger_.error("iter_trans_solve 全部阶数失败, 终止求解");
        logger_.errorf("  诊断: success=%d, n_inliers=%d (阈值=3)",
                       (int)it_result.success, it_result.n_inliers);
        *result = fail_result;
        return;
    }

    logger_.infof("  iter_trans TRANS: order=%d, x00=%.4f y00=%.4f (px), "
                  "x10=%.6f x01=%.6f y10=%.6f y01=%.6f",
                  it_result.trans.order,
                  it_result.trans.x00, it_result.trans.y00,
                  it_result.trans.x10, it_result.trans.x01,
                  it_result.trans.y10, it_result.trans.y01);

    // 6. 迭代重投影 (固定索引策略)
    logger_.info("  [诊断] 准备调用 iterative_reproject...");
    fflush(stdout); fflush(stderr);
    auto t_rep_start = std::chrono::steady_clock::now();
    IterativeReprojectResult rep_result = iterative_reproject(
        U, selection.gaia_ra, selection.gaia_dec,
        it_result.trans, it_result.inliers,
        ra0, dec0, s0, img_width, img_height, &logger_);
    auto t_rep_end = std::chrono::steady_clock::now();
    logger_.infof("  [诊断] iterative_reproject 返回: success=%d, n_matched=%d, "
                  "n_iterations=%d, convergence=%.4f, ra0=%.6f, dec0=%.6f, "
                  "matched.size=%zu, trans.order=%d",
                  (int)rep_result.success, rep_result.n_matched,
                  rep_result.n_iterations, rep_result.convergence,
                  rep_result.ra0, rep_result.dec0,
                  rep_result.matched.size(), rep_result.trans.order);
    fflush(stdout); fflush(stderr);
    logger_.infof("  [阶段] iterative_reproject: %.2f ms, success=%d, "
                  "ra=%.6f→%.6f, dec=%.6f→%.6f, n_matched=%d, iters=%d, conv=%.4f\"",
                  elapsed_ms(t_rep_start, t_rep_end), (int)rep_result.success,
                  ra0, rep_result.ra0, dec0, rep_result.dec0,
                  rep_result.n_matched, rep_result.n_iterations,
                  rep_result.convergence);

    if (!rep_result.success || rep_result.n_matched < 3) {
        logger_.error("iterative_reproject 失败或匹配数不足, 终止求解");
        logger_.errorf("  诊断: success=%d, n_matched=%d (阈值=3)",
                       (int)rep_result.success, rep_result.n_matched);
        *result = fail_result;
        return;
    }

    // 7. 用收敛后的中心重新投影 Gaia, 得到 W_final
    const int N_W = (int)selection.gaia_ra.size();
    std::vector<StarPoint> W_final(N_W);
    int n_invalid_proj = 0;
    for (int i = 0; i < N_W; ++i) {
        double xi, eta;
        bool valid;
        gnomonic_forward_proj_solver(
            selection.gaia_ra[i], selection.gaia_dec[i],
            rep_result.ra0, rep_result.dec0, xi, eta, valid);
        if (!valid) {
            W_final[i].x = 1e18;
            W_final[i].y = 1e18;
            ++n_invalid_proj;
        } else {
            W_final[i].x = xi;
            W_final[i].y = eta;
        }
        W_final[i].flux = 0.0;
        W_final[i].saturated = false;
    }

    if (n_invalid_proj > 0) {
        logger_.warnf("  W_final 投影: %d 颗无效 (将过滤)", n_invalid_proj);
    }

    // 7.5. 高阶重匹配
    if (rep_result.trans.order > 1) {
        auto t_himatch_start = std::chrono::steady_clock::now();
        const int order_hi = rep_result.trans.order;
        const int min_pairs_hi = (order_hi == 3) ? 10 :
                                  (order_hi == 2) ? 6 : 3;

        std::vector<MatchPair> hi_matches = at_match_lists(
            U, W_final, rep_result.trans, 5.0);
        logger_.infof("  [阶段] hi_order_rematch iter 0: n_matched=%zu (min=%d, order=%d)",
                      hi_matches.size(), min_pairs_hi, order_hi);

        if ((int)hi_matches.size() >= min_pairs_hi) {
            IterTransResult hi_result = at_recalc_trans(U, W_final, hi_matches, order_hi);
            if (hi_result.success) {
                for (int iter = 0; iter < 5; ++iter) {
                    std::vector<MatchPair> new_matches = at_match_lists(
                        U, W_final, hi_result.trans, 5.0);
                    if ((int)new_matches.size() < min_pairs_hi) break;
                    IterTransResult new_result = at_recalc_trans(
                        U, W_final, new_matches, order_hi);
                    if (!new_result.success) break;

                    int dn = (int)new_matches.size() - (int)hi_matches.size();
                    double drms = std::fabs(new_result.rms - hi_result.rms);
                    bool conv_match = (std::abs(dn) <= 1);
                    bool conv_rms = (hi_result.rms > 0) ?
                                    (drms < hi_result.rms * 0.01) : true;

                    hi_result = new_result;
                    hi_matches = new_matches;

                    if (conv_match && conv_rms) {
                        logger_.infof("  [阶段] hi_order_rematch iter %d: 收敛 (dn=%d, drms=%.4f)",
                                      iter + 1, dn, drms);
                        break;
                    }
                    logger_.infof("  [阶段] hi_order_rematch iter %d: n_matched=%zu, rms=%.4f",
                                  iter + 1, hi_matches.size(), hi_result.rms);
                }
                rep_result.trans = hi_result.trans;
                rep_result.matched = hi_matches;
                rep_result.n_matched = (int)hi_matches.size();
            }
        }
        auto t_himatch_end = std::chrono::steady_clock::now();
        logger_.infof("  [阶段] hi_order_rematch: %.2f ms, n_matched=%d, trans.order=%d",
                      elapsed_ms(t_himatch_start, t_himatch_end),
                      rep_result.n_matched, rep_result.trans.order);
    }

    // 8.5 : 鲁棒扩增精化
    bool robust_refine_applied = false;
    if (!selection.U_full.empty() && rep_result.trans.order > 1) {
        auto t_robust_start = std::chrono::steady_clock::now();
        double initial_rms_arcsec = rep_result.trans.sig;
        if (initial_rms_arcsec <= 0) initial_rms_arcsec = 1.0;

        RobustRefineResult rob = robust_refine_wcs(
            rep_result.trans,
            selection.U_full, selection.mag_full,
            selection.gaia_ra, selection.gaia_dec,
            rep_result.ra0, rep_result.dec0,
            s0, initial_rms_arcsec,
            selection.fov_diag_deg,
            img_width, img_height,
            RobustRefineParams{}, &logger_);

        if (rob.success && !rob.fallback) {
            rep_result.trans = rob.trans;
            rep_result.matched = rob.matched;
            rep_result.n_matched = rob.n_matched;
            robust_refine_applied = true;
            logger_.infof("  [阶段] robust_refine: 成功, n_matched=%d, rms=%.4f\", "
                          "iter=%d, pool=%d, cd_Δ=%.4f%%",
                          rob.n_matched, rob.rms_arcsec, rob.n_iterations,
                          rob.n_pool, rob.cd_relative_change * 100);
        } else {
            logger_.infof("  [阶段] robust_refine: 回退 (fallback=%d), 使用 hi_order_rematch 结果",
                          (int)rob.fallback);
        }
        auto t_robust_end = std::chrono::steady_clock::now();
        logger_.infof("  [阶段] robust_refine: %.2f ms",
                      elapsed_ms(t_robust_start, t_robust_end));
    }

    // 8.6 选择传给 extract_wcs_sip 的 U 向量
    const std::vector<StarPoint>& U_for_wcs =
        robust_refine_applied ? selection.U_full : U;

    // 9. WCS+SIP 从 TRANS 提取
    logger_.infof("  [诊断] 准备调用 extract_wcs_sip: W_final.size=%zu, matched.size=%zu, "
                  "U_for_wcs.size=%zu (robust=%d)",
                  W_final.size(), rep_result.matched.size(),
                  U_for_wcs.size(), (int)robust_refine_applied);

    auto t_wcs_start = std::chrono::steady_clock::now();
    extract_wcs_sip(
        rep_result.trans, rep_result.ra0, rep_result.dec0,
        img_width, img_height, s0,
        U_for_wcs, W_final, rep_result.matched, result, &logger_);
    auto t_wcs_end = std::chrono::steady_clock::now();

    {
        double wcs_ms = elapsed_ms(t_wcs_start, t_wcs_end);
        logger_.infof("  [阶段] extract_wcs_sip: %.2f ms, success=%d, n_pairs=%d, "
                      "rms_px=%.3f, rms_arcsec=%.3f, trans_order=%d, sip_order=%d",
                      wcs_ms, (int)result->success, result->n_pairs,
                      result->rms_px, result->rms_arcsec,
                      result->trans_order, result->sip.order);
    }

    // v1.3: 缓存最终权威 inlier 数据 (供 WCS Gate v2 双层闭环)
    cache_last_inliers_(
        rep_result.matched, U_for_wcs,
        selection.gaia_ra, selection.gaia_dec,
        rep_result.trans, s0,
        rep_result.ra0, rep_result.dec0,
        img_width, img_height,
        robust_refine_applied);

    // 9. 最终日志
    logger_.info("==== IPVSolver::solve_from_memory 完成 (V4.22) ====");
    logger_.infof("  最终: n_pairs=%d, rms_px=%.3f, rms_arcsec=%.3f, "
                  "trans_order=%d, sip_order=%d, success=%d",
                  result->n_pairs, result->rms_px, result->rms_arcsec,
                  result->trans_order, result->sip.order, (int)result->success);
}

// ===========================================================================
// solve_post_select - 选星后通用求解流程
//
// 从 triangle_match 开始到 extract_wcs_sip 结束的完整流程。
// 供 solve_from_detections_v1 (路径 A) 和 solve_from_memory_with_callback (路径 B) 共享。
// 算法与 solve_from_memory 中相应部分完全一致, 仅提取为独立方法。
// ===========================================================================
void IPVSolver::solve_post_select(
    StarSelection& selection,
    const IPVSolverParams& params,
    double ra0,
    double dec0,
    WcsFitResult* result
) {
    WcsFitResult fail_result{};
    fail_result.trans_order = 0;

    // 1. triangle_match (n_target=60, 与 solve_from_memory 一致)
    auto t_sel_start = std::chrono::steady_clock::now();
    TriangleMatchResult tri_result{};
    int vote_threshold = params.vote_threshold;
    const int n_target = 60;  // 与 solve_from_memory 的 {60} 一致

    auto t_tri_start = std::chrono::steady_clock::now();
    tri_result = triangle_match(selection.U, selection.W, n_target, n_target,
                                 0.002, selection.s0);
    auto t_tri_end = std::chrono::steady_clock::now();

    logger_.infof("[P02-002] n_target=%d, max_vote=%d, threshold=%d",
                  n_target, tri_result.max_vote, vote_threshold);
    logger_.infof("  [阶段] triangle_match (n=%d): %.2f ms, max_vote=%d, top_pairs=%d, success=%d",
                  n_target, elapsed_ms(t_tri_start, t_tri_end),
                  tri_result.max_vote,
                  (int)tri_result.top_pairs.size(),
                  (int)tri_result.success);
    auto t_sel_end = std::chrono::steady_clock::now();
    logger_.infof("  [阶段] triangle_match: %.2f ms, n_target_used=%d",
                  elapsed_ms(t_sel_start, t_sel_end), n_target);

    if (!selection.success) {
        logger_.error("selection 失败, 终止求解");
        *result = fail_result;
        return;
    }

    // 2. 提取 U / W / s0 / 图像尺寸
    const std::vector<StarPoint>& U = selection.U;
    const std::vector<StarPoint>& W = selection.W;
    double s0 = selection.s0;
    int img_width = selection.img_width;
    int img_height = selection.img_height;

    logger_.infof("  N_U=%d, N_W=%d, s0=%.4f arcsec/px, img=%dx%d, fov_diag=%.4f deg",
                  (int)U.size(), (int)W.size(), s0, img_width, img_height,
                  selection.fov_diag_deg);

    // 3. triangle_match 失败判断
    if (!tri_result.success || tri_result.max_vote < 3) {
        logger_.error("triangle_match 失败或票数不足, 终止求解");
        logger_.errorf("  诊断: success=%d, max_vote=%d (阈值=3)",
                       (int)tri_result.success, tri_result.max_vote);
        *result = fail_result;
        return;
    }

    // 4. iter_trans 求解 (order 阶数回退机制)
    auto t_it_start = std::chrono::steady_clock::now();
    IterTransResult it_result;
    bool it_success = false;
    for (int it_order = 3; it_order >= 1; --it_order) {
        it_result = iter_trans_solve(
            U, W, tri_result.top_pairs, 5.0, it_order);
        if (it_result.success && it_result.n_inliers >= 3) {
            it_success = true;
            if (it_order < 3) {
                logger_.warnf("iter_trans_solve order=3 失败, 回退到 order=%d 成功 (n_inliers=%d)",
                              it_order, it_result.n_inliers);
            }
            break;
        }
        logger_.warnf("iter_trans_solve order=%d 失败 (success=%d, n_inliers=%d)",
                      it_order, (int)it_result.success, it_result.n_inliers);
    }
    auto t_it_end = std::chrono::steady_clock::now();
    logger_.infof("  [阶段] iter_trans_solve: %.2f ms, n_inliers=%d, rms=%.4f, iters=%d, success=%d",
                  elapsed_ms(t_it_start, t_it_end),
                  it_result.n_inliers, it_result.rms,
                  it_result.n_iterations, (int)it_result.success);

    if (!it_success) {
        logger_.error("iter_trans_solve 全部阶数失败, 终止求解");
        *result = fail_result;
        return;
    }

    // 5. 迭代重投影 (固定索引策略)
    auto t_rep_start = std::chrono::steady_clock::now();
    IterativeReprojectResult rep_result = iterative_reproject(
        U, selection.gaia_ra, selection.gaia_dec,
        it_result.trans, it_result.inliers,
        ra0, dec0, s0, img_width, img_height, &logger_);
    auto t_rep_end = std::chrono::steady_clock::now();
    logger_.infof("  [阶段] iterative_reproject: %.2f ms, success=%d, "
                  "ra=%.6f→%.6f, dec=%.6f→%.6f, n_matched=%d, iters=%d, conv=%.4f\"",
                  elapsed_ms(t_rep_start, t_rep_end), (int)rep_result.success,
                  ra0, rep_result.ra0, dec0, rep_result.dec0,
                  rep_result.n_matched, rep_result.n_iterations,
                  rep_result.convergence);

    if (!rep_result.success || rep_result.n_matched < 3) {
        logger_.error("iterative_reproject 失败或匹配数不足, 终止求解");
        *result = fail_result;
        return;
    }

    // 6. 用收敛后的中心重新投影 Gaia, 得到 W_final
    const int N_W = (int)selection.gaia_ra.size();
    std::vector<StarPoint> W_final(N_W);
    int n_invalid_proj = 0;
    for (int i = 0; i < N_W; ++i) {
        double xi, eta;
        bool valid;
        gnomonic_forward_proj_solver(
            selection.gaia_ra[i], selection.gaia_dec[i],
            rep_result.ra0, rep_result.dec0, xi, eta, valid);
        if (!valid) {
            W_final[i].x = 1e18;
            W_final[i].y = 1e18;
            ++n_invalid_proj;
        } else {
            W_final[i].x = xi;
            W_final[i].y = eta;
        }
        W_final[i].flux = 0.0;
        W_final[i].saturated = false;
    }

    if (n_invalid_proj > 0) {
        logger_.warnf("  W_final 投影: %d 颗无效 (将过滤)", n_invalid_proj);
    }

    // 7. 高阶重匹配
    if (rep_result.trans.order > 1) {
        auto t_himatch_start = std::chrono::steady_clock::now();
        const int order_hi = rep_result.trans.order;
        const int min_pairs_hi = (order_hi == 3) ? 10 :
                                  (order_hi == 2) ? 6 : 3;

        std::vector<MatchPair> hi_matches = at_match_lists(
            U, W_final, rep_result.trans, 5.0);
        logger_.infof("  [阶段] hi_order_rematch iter 0: n_matched=%zu (min=%d, order=%d)",
                      hi_matches.size(), min_pairs_hi, order_hi);

        if ((int)hi_matches.size() >= min_pairs_hi) {
            IterTransResult hi_result = at_recalc_trans(U, W_final, hi_matches, order_hi);
            if (hi_result.success) {
                for (int iter = 0; iter < 5; ++iter) {
                    std::vector<MatchPair> new_matches = at_match_lists(
                        U, W_final, hi_result.trans, 5.0);
                    if ((int)new_matches.size() < min_pairs_hi) break;
                    IterTransResult new_result = at_recalc_trans(
                        U, W_final, new_matches, order_hi);
                    if (!new_result.success) break;

                    int dn = (int)new_matches.size() - (int)hi_matches.size();
                    double drms = std::fabs(new_result.rms - hi_result.rms);
                    bool conv_match = (std::abs(dn) <= 1);
                    bool conv_rms = (hi_result.rms > 0) ?
                                    (drms < hi_result.rms * 0.01) : true;

                    hi_result = new_result;
                    hi_matches = new_matches;

                    if (conv_match && conv_rms) {
                        logger_.infof("  [阶段] hi_order_rematch iter %d: 收敛 (dn=%d, drms=%.4f)",
                                      iter + 1, dn, drms);
                        break;
                    }
                    logger_.infof("  [阶段] hi_order_rematch iter %d: n_matched=%zu, rms=%.4f",
                                  iter + 1, hi_matches.size(), hi_result.rms);
                }
                rep_result.trans = hi_result.trans;
                rep_result.matched = hi_matches;
                rep_result.n_matched = (int)hi_matches.size();
            }
        }
        auto t_himatch_end = std::chrono::steady_clock::now();
        logger_.infof("  [阶段] hi_order_rematch: %.2f ms, n_matched=%d, trans.order=%d",
                      elapsed_ms(t_himatch_start, t_himatch_end),
                      rep_result.n_matched, rep_result.trans.order);
    }

    // 8. : 鲁棒扩增精化
    bool robust_refine_applied = false;
    if (!selection.U_full.empty() && rep_result.trans.order > 1) {
        auto t_robust_start = std::chrono::steady_clock::now();
        double initial_rms_arcsec = rep_result.trans.sig;
        if (initial_rms_arcsec <= 0) initial_rms_arcsec = 1.0;

        RobustRefineResult rob = robust_refine_wcs(
            rep_result.trans,
            selection.U_full, selection.mag_full,
            selection.gaia_ra, selection.gaia_dec,
            rep_result.ra0, rep_result.dec0,
            s0, initial_rms_arcsec,
            selection.fov_diag_deg,
            img_width, img_height,
            RobustRefineParams{}, &logger_);

        if (rob.success && !rob.fallback) {
            rep_result.trans = rob.trans;
            rep_result.matched = rob.matched;
            rep_result.n_matched = rob.n_matched;
            robust_refine_applied = true;
            logger_.infof("  [阶段] robust_refine: 成功, n_matched=%d, rms=%.4f\", "
                          "iter=%d, pool=%d, cd_Δ=%.4f%%",
                          rob.n_matched, rob.rms_arcsec, rob.n_iterations,
                          rob.n_pool, rob.cd_relative_change * 100);
        } else {
            logger_.infof("  [阶段] robust_refine: 回退 (fallback=%d), 使用 hi_order_rematch 结果",
                          (int)rob.fallback);
        }
        auto t_robust_end = std::chrono::steady_clock::now();
        logger_.infof("  [阶段] robust_refine: %.2f ms",
                      elapsed_ms(t_robust_start, t_robust_end));
    }

    // 9. 选择传给 extract_wcs_sip 的 U 向量
    const std::vector<StarPoint>& U_for_wcs =
        robust_refine_applied ? selection.U_full : U;

    // 10. WCS+SIP 从 TRANS 提取
    auto t_wcs_start = std::chrono::steady_clock::now();
    extract_wcs_sip(
        rep_result.trans, rep_result.ra0, rep_result.dec0,
        img_width, img_height, s0,
        U_for_wcs, W_final, rep_result.matched, result, &logger_);
    auto t_wcs_end = std::chrono::steady_clock::now();

    logger_.infof("  [阶段] extract_wcs_sip: %.2f ms, success=%d, n_pairs=%d, "
                  "rms_px=%.3f, rms_arcsec=%.3f, trans_order=%d, sip_order=%d",
                  elapsed_ms(t_wcs_start, t_wcs_end), (int)result->success, result->n_pairs,
                  result->rms_px, result->rms_arcsec,
                  result->trans_order, result->sip.order);

    logger_.infof("  最终: n_pairs=%d, rms_px=%.3f, rms_arcsec=%.3f, "
                  "trans_order=%d, sip_order=%d, success=%d",
                  result->n_pairs, result->rms_px, result->rms_arcsec,
                  result->trans_order, result->sip.order, (int)result->success);

    // v1.3: 缓存最终权威 inlier 数据 (供 WCS Gate v2 双层闭环)
    cache_last_inliers_(
        rep_result.matched, U_for_wcs,
        selection.gaia_ra, selection.gaia_dec,
        rep_result.trans, s0,
        rep_result.ra0, rep_result.dec0,
        img_width, img_height,
        robust_refine_applied);
}

// ===========================================================================
// 路径 A - solve_from_detections_v1
//
// 从外部 detections (FLOAT64 [N,6] star_det v1) 求解, 跳过 sdet_detect_ex。
// 算法与 solve_from_memory 一致, 仅跳过检测步骤。
// ===========================================================================
void IPVSolver::solve_from_detections_v1(
    const double* detections,
    int n_detections,
    int image_width, int image_height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    WcsFitResult* result
) {
    // 失败结果
    WcsFitResult fail_result{};
    fail_result.trans_order = 0;

    // 1. 初始化日志
    if (params.log_dir != nullptr && params.log_dir[0] != '\0') {
        std::string dir(params.log_dir);
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
            dir.pop_back();
        }
        logger_.init(dir + "/ipv_solver.log");
        init_triangle_logger(dir + "/ipv_triangle.log");
        init_itertrans_logger(dir + "/ipv_itertrans.log");
    }

    logger_.info("==== IPVSolver::solve_from_detections_v1 开始 (P02-002 路径 A) ====");
    logger_.infof("  detections=%d 颗, image=%dx%d", n_detections, image_width, image_height);
    logger_.infof("  ra0=%.6f deg, dec0=%.6f deg", ra0, dec0);
    logger_.infof("  focal_length=%.3f mm, pixel_size=%.3f um",
                  focal_length_mm, pixel_size_um);

    // 2. 选星 (路径 A: 从外部 detections 选星, 跳过 sdet_detect_ex)
    IPVSolverParams p_adapt = params;
    p_adapt.img_n_target = 60;  // 与 solve_from_memory 一致

    StarSelection selection;
    int ret = ipv_select_from_detections(
        detections, n_detections,
        image_width, image_height,
        ra0, dec0, focal_length_mm, pixel_size_um,
        p_adapt, selection, &logger_);

    if (ret != 0 || !selection.success) {
        logger_.error("ipv_select_from_detections 失败, 终止求解");
        *result = fail_result;
        return;
    }

    // 3. 选星后通用求解流程
    solve_post_select(selection, params, ra0, dec0, result);

    logger_.info("==== IPVSolver::solve_from_detections_v1 完成 (P02-002 路径 A) ====");
}

// ===========================================================================
// INTERNAL_DETECTION_SHARED_EXPORT (历史 路径 B) -
// solve_from_memory_with_callback
//
// 与 solve_from_memory 算法完全一致, 区别:
// - sdet_detect_ex 调用后, 选星前, 调用 callback 导出完整检测结果
// - callback 为 NULL 时行为与 solve_from_memory 完全一致
// ===========================================================================
void IPVSolver::solve_from_memory_with_callback(
    const float* pixels,
    int width, int height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    WcsFitResult* result
) {
    // 失败结果
    WcsFitResult fail_result{};
    fail_result.trans_order = 0;

    // 1. 初始化日志
    if (params.log_dir != nullptr && params.log_dir[0] != '\0') {
        std::string dir(params.log_dir);
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
            dir.pop_back();
        }
        logger_.init(dir + "/ipv_solver.log");
        init_triangle_logger(dir + "/ipv_triangle.log");
        init_itertrans_logger(dir + "/ipv_itertrans.log");
    }

    logger_.info("==== IPVSolver::solve_from_memory_with_callback 开始 (INTERNAL_DETECTION_SHARED_EXPORT) ====");
    logger_.infof("  pixels=%dx%d (内存数据)", width, height);
    logger_.infof("  ra0=%.6f deg, dec0=%.6f deg", ra0, dec0);
    logger_.infof("  focal_length=%.3f mm, pixel_size=%.3f um",
                  focal_length_mm, pixel_size_um);
    logger_.infof("  callback=%s", callback ? "ENABLED" : "NULL (兼容模式)");

    // 2. 选星 (INTERNAL_DETECTION_SHARED_EXPORT: 带 callback 的内存选星)
    IPVSolverParams p_adapt = params;
    p_adapt.img_n_target = 60;

    StarSelection selection;
    int ret = ipv_select_from_memory_with_callback(
        pixels, width, height,
        ra0, dec0, focal_length_mm, pixel_size_um,
        p_adapt, callback, user_data,
        selection, &logger_);

    if (ret != 0 || !selection.success) {
        logger_.error("ipv_select_from_memory_with_callback 失败, 终止求解");
        *result = fail_result;
        return;
    }

    // 3. 选星后通用求解流程
    solve_post_select(selection, params, ra0, dec0, result);

    logger_.info("==== IPVSolver::solve_from_memory_with_callback 完成 (INTERNAL_DETECTION_SHARED_EXPORT) ====");
}

// ============================================================================
// solve_from_memory_with_callback_f64 - FP64 内存求解 (double 图像)
// 与 float 版算法一致, double 图像直接检测, 不降级
// ============================================================================
void IPVSolver::solve_from_memory_with_callback_f64(
    const double* pixels,
    int width, int height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    WcsFitResult* result
) {
    // 失败结果
    WcsFitResult fail_result{};
    fail_result.trans_order = 0;

    // 1. 初始化日志
    if (params.log_dir != nullptr && params.log_dir[0] != '\0') {
        std::string dir(params.log_dir);
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
            dir.pop_back();
        }
        logger_.init(dir + "/ipv_solver.log");
        init_triangle_logger(dir + "/ipv_triangle.log");
        init_itertrans_logger(dir + "/ipv_itertrans.log");
    }

    logger_.info("==== IPVSolver::solve_from_memory_with_callback_f64 开始 (FP64, INTERNAL_DETECTION_SHARED_EXPORT) ====");
    logger_.infof("  pixels=%dx%d (double 内存数据, 不降级)", width, height);
    logger_.infof("  ra0=%.6f deg, dec0=%.6f deg", ra0, dec0);
    logger_.infof("  focal_length=%.3f mm, pixel_size=%.3f um",
                  focal_length_mm, pixel_size_um);

    // 2. 选星 (FP64: double 图像直接检测)
    IPVSolverParams p_adapt = params;
    p_adapt.img_n_target = 60;

    StarSelection selection;
    int ret = ipv_select_from_memory_with_callback_f64(
        pixels, width, height,
        ra0, dec0, focal_length_mm, pixel_size_um,
        p_adapt, callback, user_data,
        selection, &logger_);

    if (ret != 0 || !selection.success) {
        logger_.error("ipv_select_from_memory_with_callback_f64 失败, 终止求解");
        *result = fail_result;
        return;
    }

    // 3. 选星后通用求解流程
    solve_post_select(selection, params, ra0, dec0, result);

    logger_.info("==== IPVSolver::solve_from_memory_with_callback_f64 完成 ====");
}

// ===========================================================================
// v1.3: 权威 inlier 缓存实现
//
// cache_last_inliers_: 在每次 solve_* 成功末尾填充缓存
// get_last_inlier_count: 返回缓存中的 inlier 数量
// get_last_inliers: 输出 (N,9) double 数组 (字段定义见 ipv_solver.h)
//
// 内部预测计算 (与 extract_wcs_sip 的 RMS 计算保持一致):
// apply_trans(trans, U.x, U.y) -> (xi_asec, eta_asec) [TRANS: U(像素)→W(角秒)]
// pred_x_px = xi_asec / s0
// pred_y_px = eta_asec / s0
// residual_x_px = U.x - pred_x_px (像素)
// ===========================================================================

void IPVSolver::cache_last_inliers_(
    const std::vector<MatchPair>& matched,
    const std::vector<StarPoint>& U_snapshot,
    const std::vector<double>& gaia_ra,
    const std::vector<double>& gaia_dec,
    const Trans& trans,
    double s0,
    double ra0, double dec0,
    int img_width, int img_height,
    bool robust_applied) {

    last_inliers_.matched       = matched;
    last_inliers_.U_snapshot    = U_snapshot;
    last_inliers_.gaia_ra       = gaia_ra;
    last_inliers_.gaia_dec      = gaia_dec;
    last_inliers_.trans          = trans;
    last_inliers_.s0            = s0;
    last_inliers_.ra0           = ra0;
    last_inliers_.dec0          = dec0;
    last_inliers_.img_width     = img_width;
    last_inliers_.img_height    = img_height;
    last_inliers_.robust_applied = robust_applied;
    last_inliers_.valid         = !matched.empty() && s0 > 0.0;

    if (last_inliers_.valid) {
        logger_.infof("  [cache_last_inliers_] 缓存 %zu 对 inliers "
                      "(robust=%d, s0=%.4f\", U_snap.size=%zu, gaia.size=%zu, "
                      "trans.order=%d, ra0=%.6f, dec0=%.6f)",
                      matched.size(), (int)robust_applied, s0,
                      U_snapshot.size(), gaia_ra.size(),
                      trans.order, ra0, dec0);
    } else {
        logger_.warnf("  [cache_last_inliers_] 跳过缓存 (matched.empty=%d, s0=%.4f)",
                      (int)matched.empty(), s0);
    }
}

int IPVSolver::get_last_inlier_count() const {
    return last_inliers_.valid ? (int)last_inliers_.matched.size() : 0;
}

int IPVSolver::get_last_inliers(double* out_buffer, int max_count) const {
    if (!last_inliers_.valid) {
        return 0;
    }
    if (out_buffer == nullptr || max_count <= 0) {
        return -1;
    }

    const int n_total = (int)last_inliers_.matched.size();
    const int n_out   = std::min(n_total, max_count);
    const double s0   = last_inliers_.s0;
    const Trans& trans = last_inliers_.trans;
    const auto& U       = last_inliers_.U_snapshot;
    const auto& gaia_ra  = last_inliers_.gaia_ra;
    const auto& gaia_dec = last_inliers_.gaia_dec;
    const double ra0  = last_inliers_.ra0;
    const double dec0 = last_inliers_.dec0;

    for (int i = 0; i < n_out; ++i) {
        const auto& mp = last_inliers_.matched[i];
        double* row = out_buffer + i * 9;

        if (mp.u < 0 || mp.u >= (int)U.size() ||
            mp.w < 0 || mp.w >= (int)gaia_ra.size()) {
            // 索引越界, 写入 NaN (供调用方识别)
            for (int k = 0; k < 9; ++k) row[k] = std::nan("");
            continue;
        }

        const auto& u_star = U[mp.u];
        const double ra  = gaia_ra[mp.w];
        const double dec = gaia_dec[mp.w];

        // v3.1 修复: 残差 = (实际W - 预测W) / s0, 与 extract_wcs_sip 的 RMS 计算一致
        //
        // 1. 实际 W (角秒): 用 gnomonic 正向投影把 Gaia (ra,dec) 投到以 ra0/dec0 为中心的切平面
        // 2. 预测 W (角秒): apply_trans(trans, U) -> (xi_pred_asec, eta_pred_asec)
        // 3. 残差 (像素) = (实际 - 预测) / s0
        //
        // 坐标系约定 :
        // U = 像素坐标, 图像中心原点, Y 轴向上
        // W = 角秒坐标, 投影中心原点 (gnomonic xi/eta)
        // TRANS: U -> W
        // 所以 det_x_px = u_star.x, 但残差应在 W 空间计算
        bool valid = false;
        double xi_actual_asec = 0.0, eta_actual_asec = 0.0;
        gnomonic_forward_proj_solver(
            ra, dec, ra0, dec0,
            xi_actual_asec, eta_actual_asec, valid);

        // 内部 TRANS 预测: apply_trans(U) -> (xi_pred_asec, eta_pred_asec) [角秒]
        double xi_pred_asec = 0.0, eta_pred_asec = 0.0;
        apply_trans(trans, u_star.x, u_star.y, &xi_pred_asec, &eta_pred_asec);

        // 转回像素 (用于 A vs B 层对比: 内部预测 vs 外部 WCS 回投)
        double pred_x_px = xi_pred_asec  / s0;
        double pred_y_px = eta_pred_asec / s0;

        // 残差 (像素) = (实际W - 预测W) / s0
        // 与 extract_wcs_sip 的 RMS 计算一致 (匹配 RMS)
        double res_x = valid ? (xi_actual_asec  - xi_pred_asec)  / s0 : std::nan("");
        double res_y = valid ? (eta_actual_asec - eta_pred_asec) / s0 : std::nan("");
        double res_dist = std::sqrt(res_x * res_x + res_y * res_y);

        row[0] = u_star.x;          // det_x_px
        row[1] = u_star.y;          // det_y_px
        row[2] = ra;                 // gaia_ra_deg
        row[3] = dec;                // gaia_dec_deg
        row[4] = pred_x_px;          // pred_x_px (内部预测的 W 像素)
        row[5] = pred_y_px;          // pred_y_px
        row[6] = res_x;              // residual_x_px = (实际W - 预测W) / s0
        row[7] = res_y;              // residual_y_px
        row[8] = res_dist;           // residual_dist_px
    }

    return n_out;
}

} // namespace ipv

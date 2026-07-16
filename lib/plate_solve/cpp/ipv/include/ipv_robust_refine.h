#ifndef IPV_ROBUST_REFINE_H
#define IPV_ROBUST_REFINE_H

// ============================================================================
// ipv_robust_refine.h - V4.30 鲁棒扩增 WCS 精化模块
//
// 在 hi_order_rematch 之后、extract_wcs_sip 之前插入新阶段:
//   1. 网格配额采样选星 (100-300 颗, 空间均匀, 替代纯按星等取 60 颗)
//   2. 5 层防护 NN 匹配 (Lowe ratio + 空间一致性 + 发散检测)
//   3. IRLS 鲁棒拟合 (CD+SIP 联合, CD 带阻尼, Tukey biweight)
//   4. 失败回退到 hi_order_rematch 结果, 不破坏 99.87% 成功率
//
// 简化方案 (与现有 iter_trans 框架一致):
//   - 参数向量 = TRANS 系数 (order=3 时 20 个: x00..x03, y00..y03)
//   - 残差在角秒空间: r_i = apply_trans(U[i]) - W_gaia[i] (W_gaia 为 gnomonic xi/eta)
//   - CD 阻尼 → 对 TRANS 线性项 x10/x01/y10/y01 加阻尼
//   - 内部使用 Y-up 坐标系 (与 solver 内部一致), 由 extract_wcs_sip 完成 Y-flip
//
// 日期: 2026-07-09
// ============================================================================

#include <vector>
#include "ipv_types.h"
#include "ipv_itertrans.h"   // Trans, MatchPair
#include "ipv_log.h"

namespace ipv {

// ---------------------------------------------------------------------------
// 鲁棒精化参数 (默认值对齐 spec)
// ---------------------------------------------------------------------------
struct RobustRefineParams {
    // --- 网格采样 ---
    int    grid_g_short_narrow = 2;   // FOV < 1° 短边格数
    int    grid_g_short_medium = 3;   // FOV 1-3° 短边格数
    int    grid_g_short_wide   = 4;   // FOV > 3° 短边格数
    int    total_target_narrow = 1000; // 小视场目标星数 (全选不限制)
    int    total_target_normal = 180;  // 常规视场目标星数
    int    max_stars           = 300;  // 候选池上限
    int    min_stars           = 100;  // 候选池下限 (低于此值不强制填充)

    // --- IRLS 迭代 ---
    int    max_iterations      = 10;   // 主循环最大迭代次数
    double tukey_c             = 4.685; // Tukey biweight 常数
    double converge_dp_max     = 1e-9; // 参数变化收敛阈值
    double converge_rms_rel    = 0.001; // RMS 相对变化收敛阈值 (0.1%)

    // --- CD 阻尼 (相对变化阈值) ---
    double cd_free_threshold   = 0.01; // < 1% 无阻尼
    double cd_freeze_threshold = 0.03; // > 3% 冻结
    double cd_damp_base        = 1e-4; // 1% 时的阻尼
    double cd_damp_freeze      = 1e6;  // > 3% 时的阻尼

    // --- 容差收紧 ---
    double tol_factor_init     = 2.0;  // iter 0: 2×initial_RMS
    double tol_factor_mid      = 1.5;  // iter 1-2: 1.5×current_RMS
    double tol_factor_final    = 1.0;  // iter ≥3: 1.0×current_RMS
    double tol_floor_init      = 1.0;  // 角秒保底
    double tol_floor_mid       = 0.5;
    double tol_floor_final     = 0.3;

    // --- 防护 ---
    int    lowe_k_neighbors    = 8;     // 空间一致性 K 邻居
    double lowe_ratio          = 0.75;  // Lowe 比率阈值
    double lowe_abs_threshold  = 0.5;   // Lowe 绝对阈值 (角秒)
    double spatial_consistency_sigma = 3.0; // 空间一致性 sigma 阈值
    double spatial_weight_factor     = 0.1;  // 可疑点权重因子
    int    diverge_consecutive  = 2;          // RMS 连续上升次数
    double diverge_rms_increase = 0.10;       // RMS 上升 10% 触发发散
    double diverge_match_drop   = 0.50;       // 匹配数下降 50% 触发发散
    double final_rms_tolerance  = 1.5;        // final > 1.5×initial → 回退
    int    min_matched_final    = 30;         // matched < 30 → 回退
};

// ---------------------------------------------------------------------------
// 鲁棒精化结果
// ---------------------------------------------------------------------------
struct RobustRefineResult {
    Trans                   trans;         // 精化后 TRANS (若回退则 = 初始)
    std::vector<MatchPair>  matched;       // 精化后匹配对
    double                  rms_px = 0;    // 最终 RMS (像素)
    double                  rms_arcsec = 0;
    int                     n_matched = 0;
    int                     n_iterations = 0;
    bool                    fallback = false; // 是否回退
    bool                    success = false;

    // 诊断字段
    int                     n_pool = 0;       // 候选池星数
    int                     n_grid_cells = 0; // 网格格数
    double                  cd_relative_change = 0; // CD 最大相对变化
};

// ---------------------------------------------------------------------------
// 主入口: 鲁棒扩增 WCS 精化
//
// 输入:
//   initial_trans       - WCS0 (hi_order_rematch 结果, TRANS: U→W)
//   U_full              - 全部检测星点 (像素坐标, 原点图像中心, Y-up)
//   mag_full            - 全部检测星点 mag (与 U_full 一一对应)
//   gaia_ra/gaia_dec    - Gaia 星原始 (RA, Dec) 度
//   ra0, dec0           - 中心指向 (度, iterative_reproject 收敛后)
//   s0                  - 像素尺度 (角秒/像素)
//   initial_rms_arcsec  - 初始 RMS (角秒, 来自 hi_order_rematch)
//   fov_diag_deg        - FOV 对角线 (度)
//   img_width/height    - 图像尺寸
//   params              - 参数
//   logger              - 日志器 (可选)
//
// 输出: RobustRefineResult
//   成功: trans = 精化后, matched = 精化后匹配对, fallback=false
//   失败: trans = initial_trans (回退), fallback=true
// ---------------------------------------------------------------------------
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
    const RobustRefineParams& params = {},
    Logger* logger = nullptr
);

} // namespace ipv

#endif // IPV_ROBUST_REFINE_H

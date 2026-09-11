#ifndef IPV_WCS_H
#define IPV_WCS_H

#include <vector>
#include "ipv_types.h"
#include "ipv_sip.h"   // build_wcs 内部调用 fit_sip

namespace ipv {

// 从相似变换提取标准 WCS
// 输入:
// transform - 相似变换 (s, θ, tx, ty)，s 无量纲, θ 弧度, tx/ty 角秒
// 注: 该变换是对 W_flipped (W') 求解的, 即 W' = s·R(θ)·U + t
// s0 - 像素尺度 (角秒/像素)
// img_width - 图像宽度 (像素)
// img_height - 图像高度 (像素)
// ra0 - 初始指向 RA (度)
// dec0 - 初始指向 Dec (度)
// U - 图像侧星点 (角秒坐标, 原点图像中心, Y 轴向上)
// W_flipped - 星表侧星点 W' (已应用 flip_mode 翻转, 角秒坐标)
// RMS 计算时直接用 W' - transform(U)
// inliers - PROSAC 内点匹配对列表 (w_idx 同时对应 W 与 W', 索引一致)
// flip_mode - 镜像模式 (0=NONE, 1=FLIP_X, 2=FLIP_Y, 3=FLIP_XY)
// 决定 CD 矩阵的符号方向 (W = flip_mode(W'))
// 输出:
// WcsFitResult (cd, crval, crpix, sip, rms_px, rms_arcsec, n_pairs, success, trans_order)
// 注: 旧 build_wcs 仅为向后兼容保留, trans_order 固定为 1 (线性 SimTransform)
// 起统一求解请使用 extract_wcs_sip (从多项式 TRANS 提取)
WcsFitResult build_wcs(
    const SimTransform& transform,
    double s0,
    int img_width,
    int img_height,
    double ra0,
    double dec0,
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W_flipped,
    const std::vector<MatchPair>& inliers,
    int flip_mode
);

// ---------------------------------------------------------------------------
// 迭代式反演: 天球 (RA/Dec, 度) → 像素 (0-based FITS, Y-down)
// (WCS-003 owner 裁决 1 选 B: 消费方迭代式反演语义的生产参考实现,
//  替代 AP/BP 一步直加语义达成 F2 冻结门 <1e-4 px)
//
// 语义: 解 u 满足前向模型 F(u) = u + A(u) = CD⁻¹·(ξ,η)
//   (ξ,η) = TAN 正投影 (ra,dec 相对 CRVAL), A 为 Y-down 标准前向 SIP。
//   初值 = CD⁻¹·(ξ,η) + APx(扩展逆向, apx_order>0) / 一步 AP(ap_order>0)。
//   牛顿迭代 (J = I + ∂A/∂u, 2x2 解析导数), 收敛判据 |F(u)-UV|∞ < tol_px。
// 确定性拒绝 (确定性, 零 UB): 
//   reject_code=1 投影背面/输入非有限 (TAN 正投影 cosc≈0 或 NaN/Inf);
//   reject_code=2 发散 (超 max_iter 未收敛或步出合理像素域);
//   reject_code=3 牛顿矩阵奇异 (|det J|<1e-15, 畸变场奇点域)。
//   非确定状态零依赖: 纯算术固定顺序, 无并行/无时间/无随机。
// 返回:
//   converged=true 时 x,y 有效 (x = u + CRPIX1 - 1, y = v + CRPIX2 - 1);
//   converged=false 时 x,y 置 NaN, iterations=已执行迭代次数。
struct WcsIterativeResult {
    bool   converged;
    double x;            // 0-based FITS 像素 x (Y-down)
    double y;            // 0-based FITS 像素 y (Y-down)
    int    iterations;   // 实际迭代次数 (含拒绝前的次数)
    int    reject_code;  // 0=收敛, 1=背面/非有限, 2=发散/未收敛, 3=J 奇异
};

WcsIterativeResult wcs_sky_to_pixel_iterative(
    const WcsFitResult& wcs,
    double ra_deg,
    double dec_deg,
    double tol_px = 1e-9,
    int max_iter = 64
);

} // namespace ipv

#endif // IPV_WCS_H

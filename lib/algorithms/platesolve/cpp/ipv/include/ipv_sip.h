#ifndef IPV_SIP_H
#define IPV_SIP_H

// ============================================================================
// ipv_sip.h - IPV SIP (Simple Imaging Polynomial) 多项式畸变拟合接口
//
// 在线性相似变换 (s, θ, tx, ty) 基础上, 拟合 SIP A/B 多项式表达宽 FOV 镜头的
// 径向/切向畸变残差。与 WCS 一起输出, 提高大视场定标精度。
//
// 模型 ( 像素空间):
// linear_pred = s·R(θ)·U + t (与 PROSAC/Umeyama 求解的 tf 一致)
// 残差 r = W - linear_pred (W 已 flip, 与 tf 对应)
// r_x ≈ A_poly(xn, yn), r_y ≈ B_poly(xn, yn)
// 其中 xn/yn 是归一化后的图像绝对像素坐标, 用于避免数值不稳定。
//
// 日期: 2026-07-04
// ============================================================================

#include <vector>
#include "ipv_types.h"
#include "ipv_log.h"

namespace ipv {

// 拟合 SIP A/B 多项式系数
// 模型: 标准 WCS SIP
// x_pred = CD11*(ra-crval1)*cos(dec) + CD12*(dec-crval2) + CRPIX1 + A_poly(x-CD1,y-CD2)
// y_pred = CD21*(ra-crval1)*cos(dec) + CD22*(dec-crval2) + CRPIX2 + B_poly(x-CD1,y-CD2)
// 但在 IPV 像素空间下简化:
// U[u].x (像素) = s*R*W[w] (像素) + t + A_poly(dx, dy)
// 其中 dx = U[u].x (像素, 减去图像中心), dy = U[u].y (像素, 减去图像中心)
// A_poly = sum_ij A_ij * dx^i * dy^j (i+j <= order)
//
// 输入:
// U, W: 图像侧/星表侧星点 ( 像素坐标)
// inliers: 匹配内点对
// tf: 相似变换 (s, theta, tx, ty) — 来自 PROSAC/Umeyama
// s0: 像素尺度 (arcsec/pixel)
// img_width, img_height: 图像尺寸
// order: SIP 阶数 (默认 3)
// logger: 可选日志器
// 返回:
// SIPCoeffs (失败时 order=0)
SIPCoeffs fit_sip(
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& inliers,
    const SimTransform& tf,
    double s0,
    int img_width,
    int img_height,
    int order = 3,
    Logger* logger = nullptr
);

} // namespace ipv

#endif // IPV_SIP_H

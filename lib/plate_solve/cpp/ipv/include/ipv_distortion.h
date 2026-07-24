#ifndef IPV_DISTORTION_H
#define IPV_DISTORTION_H

// ============================================================================
// ipv_distortion.h - V4.11 CDA Phase B: 径向畸变估计
//
// 设计参考: ipv_cda_distortion_design.md §4
// 输入: Phase A 得到的初始 WCS₀ (s, θ, tx, ty) + 全帧 U + W
// 输出: 径向畸变模型 (k1, k2) + R² 质量评估
//
// 模型 (Brown-Conrady 简化形式):
//   归一化半径 r̃ = sqrt((x-cx)²/(W/2)² + (y-cy)²/(H/2)²)
//   径向位移 dr = r̃ × (k1·r̃² + k2·r̃⁴)
//   残差预测: dx = (x-cx) × dr / r̃, dy = (y-cy) × dr / r̃
//
// 拟合方法: 迭代重加权最小二乘 (IRLS) + Huber 权重
// ============================================================================

#include "ipv_types.h"
#include "ipv_log.h"
#include <vector>

namespace ipv {

// 径向畸变模型
struct DistortionModel {
    double k1 = 0.0;           // 主导项 (负=桶形, 正=枕形)
    double k2 = 0.0;           // 高阶修正
    double fit_rms_px = 0.0;   // 拟合残差 RMS (像素)
    double r_squared = 0.0;    // 模型可解释度
    int    n_pairs = 0;        // 匹配对数
    bool   valid = false;      // 是否有效 (n_pairs >= 10 且 |k1| < 0.1)
};

// 估计径向畸变
// 输入:
//   U_full: 全帧图像星点 (像素坐标, 原点图像中心, Y轴向上)
//   W:      星表星点 (像素坐标, TAN 投影后 / s0)
//   tf:     Phase A 的初始变换 (s, θ, tx, ty)
//   s0:     像素尺度 (角秒/像素)
//   cx, cy: 图像中心 (像素, 原点左上角, 即 img_w/2, img_h/2)
//            注: U 坐标已是 (det_x - cx, -(det_y - cy)), 所以 U 坐标系原点就是中心
//   img_width, img_height: 图像尺寸
//   tau_match: 近邻匹配半径 (像素, 默认 15)
//   logger:  日志器
DistortionModel estimate_radial_distortion(
    const std::vector<StarPoint>& U_full,
    const std::vector<StarPoint>& W,
    const SimTransform& tf,
    double s0,
    int img_width,
    int img_height,
    double tau_match = 15.0,
    Logger* logger = nullptr
);

// 去畸变: 对星点坐标应用畸变校正
//   x' = x - (x) × (k1·r̃² + k2·r̃⁴)
//   y' = y - (y) × (k1·r̃² + k2·r̃⁴)
// 注: U 坐标原点已在图像中心, 所以 (x-cx) → x, (y-cy) → y
std::vector<StarPoint> undistort_stars(
    const std::vector<StarPoint>& U,
    const DistortionModel& dist,
    int img_width,
    int img_height,
    Logger* logger = nullptr
);

} // namespace ipv

#endif // IPV_DISTORTION_H

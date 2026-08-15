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

} // namespace ipv

#endif // IPV_WCS_H

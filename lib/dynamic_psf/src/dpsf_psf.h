/* Moffat4/PSF 拟合数值契约锚点（不改算法，仅文档化）：
 * SCI-PSF-001 / ALG-STAR-PSF-*：I=B+A/(1+Q)⁴，Q=p1dx²+2p2dxdy+p3dy²，
 *  p1=cos²θ/(2sx²)+sin²θ/(2sy²), p2=sin2θ/(4sx²)−sin2θ/(4sy²), p3=sin²θ/(2sx²)+cos²θ/(2sy²)。
 * 常量：MOFFAT4_FWHM_FACTOR=1.230310=2√2·√(2^{1/4}−1)（β=4，α=√2σ，见 docs/science/PSF.md / dpsf_psf.cpp:13-18）。
 * 边界/守卫：gauss pivot <1e-30 判奇异（数值奇异守卫，dpsf_psf.cpp:45）；dx/dy 求导步长 h=max(|x|·1e-6,1e-8)（相对+绝对守卫，114）；
 *  sx/sy 下界 0.3 px（防平坦星退化，169-170,326）；FWHM>rect 判 NO_CONVERGENCE（拟合窗约束，333-341）；
 *  |B−bkg0|/max(bkg0,0.01)≤0.5（背景漂移约束，343-344）；Q<0 哨兵 1e10（无效几何守卫，89-91）。
 * 残差：10–90% trimmed mean |res| = residual_scale，robust_residual_sigma=residual_scale/0.7316728 仅 Gaussian 假设有效
 *  （E[trimmed mean |r|]=0.7316728·σ, kTrimMeanToSigma, docs/science/PSF.md PsfFitQuality / STAR_PSF_ALGORITHMS.md Postconditions）。
 * θ 消歧：4 候选 {θ,π/2−θ,π/2+θ,π−θ} 取 trimmed-mad 最小者（351-363）。
 * 通量：flux=2πA·sxsy/3（β=4 整平面解析积分，368）。
 * 迭代：LM tol 1e-8 / maxIter 200（dpsf_psf.cpp:98-182,314）。
 * 线程：OpenMP 按星并行，拟合局部无共享可变状态。
 */
#pragma once
#include "../include/dynamic_psf.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int moffat4_fit(const float* image, int width, int height,
                double cx, double cy,
                int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                DPSFFitResult* result);

// 双精度 ABI: double 版本 PSF 拟合
// 与 moffat4_fit 逻辑一致, 仅 image 数据类型从 float 改为 double。
// FP64 模式下采样像素值直接为 double, 不降级到 float32 (精度关键路径)。
int moffat4_fit_d(const double* image, int width, int height,
                  double cx, double cy,
                  int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                  DPSFFitResult* result);

#pragma once

void sdet_gaussian_filter_separable(const float* src, float* dst, int w, int h, double sigma);

// Young-van Vliet 递归 IIR 高斯滤波
// 标量实现, 参考 gaussHorizontalSse + gaussVerticalSse 的 M 归一化与 Triggs-Sdika 边界条件
void sdet_gaussian_blur_yvv(const float* src, float* dst, int w, int h, double sigma);
// FP64 变体 - double 图像, 无 float 降级
void sdet_gaussian_blur_yvv_d(const double* src, double* dst, int w, int h, double sigma);

void sdet_median_filter_3x3(const float* src, float* dst, int w, int h);
void sdet_median_filter_5x5(const float* src, float* dst, int w, int h);
void sdet_median_filter(const float* src, float* dst, int w, int h, int radius);

void sdet_dilate_box(const float* src, float* dst, int w, int h, int radius);
void sdet_erode_box(const float* src, float* dst, int w, int h, int radius);
void sdet_dilate_circle(const float* src, float* dst, int w, int h, int radius);
void sdet_erode_circle(const float* src, float* dst, int w, int h, int radius);

void sdet_truncate_and_rescale(float* data, int n, float min_val, float max_val);

void sdet_local_maxima_map(const float* src, float* dst, int w, int h, int radius, float limit);

float sdet_robust_median(const float* data, int n);
float sdet_robust_mad(const float* data, int n);
// FP64 变体
double sdet_robust_median_d(const double* data, int n);
double sdet_robust_mad_d(const double* data, int n);

void sdet_downsample(const float* src, int sw, int sh, float* dst, int dw, int dh);
void sdet_upsample_bilinear(const float* src, int sw, int sh, float* dst, int dw, int dh);
void sdet_atrous_b3v_filter(const float* src, float* dst, int w, int h, int scale);
void sdet_extract_lowfreq_atrous(const float* src, float* dst, int w, int h, int downsample_factor, int n_scales);

// void sdet_atrous_linear3_filter(const float* src, float* dst, int w, int h, int scale);
// void sdet_atrous_decompose_layer2(const float* src, float* dst, int w, int h, int n_layers);

void sdet_iterative_sigma_clip(const float* data, int n,
                               float clip_sigma, int max_rounds,
                               float* out_med, float* out_mad);

void sdet_dynamic_regional_background(const float* src, float* out_detail, int w, int h,
                                       int block_size, int block_overlap,
                                       int sub_block_size, int sub_block_overlap,
                                       float clip_sigma, int max_rounds,
                                       int fill_radius);

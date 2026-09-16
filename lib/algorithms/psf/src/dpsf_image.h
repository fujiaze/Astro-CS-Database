#pragma once

void gaussian_filter_separable(const float* src, float* dst, int w, int h, double sigma);

void median_filter_3x3(const float* src, float* dst, int w, int h);
void median_filter_5x5(const float* src, float* dst, int w, int h);
void median_filter(const float* src, float* dst, int w, int h, int radius);

void dilate_box(const float* src, float* dst, int w, int h, int radius);
void erode_box(const float* src, float* dst, int w, int h, int radius);
void dilate_circle(const float* src, float* dst, int w, int h, int radius);
void erode_circle(const float* src, float* dst, int w, int h, int radius);

void truncate_and_rescale(float* data, int n, float min_val, float max_val);

void local_maxima_map(const float* src, float* dst, int w, int h, int radius, float limit);

float robust_median(const float* data, int n);
float robust_mad(const float* data, int n);

void downsample(const float* src, int sw, int sh, float* dst, int dw, int dh);
void upsample_bilinear(const float* src, int sw, int sh, float* dst, int dw, int dh);
void atrous_b3v_filter(const float* src, float* dst, int w, int h, int scale);
void extract_lowfreq_atrous(const float* src, float* dst, int w, int h, int downsample_factor, int n_scales);

void iterative_sigma_clip(const float* data, int n,
                          float clip_sigma, int max_rounds,
                          float* out_med, float* out_mad);

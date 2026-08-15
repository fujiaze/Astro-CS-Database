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

// 双精度 ABI : double 版本 PSF 拟合
// 与 moffat4_fit 逻辑一致, 仅 image 数据类型从 float 改为 double。
// FP64 模式下采样像素值直接为 double, 不降级到 float32 (精度关键路径)。
int moffat4_fit_d(const double* image, int width, int height,
                  double cx, double cy,
                  int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                  DPSFFitResult* result);

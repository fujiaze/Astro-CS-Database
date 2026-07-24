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

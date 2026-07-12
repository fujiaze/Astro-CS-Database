// image_corrector.cpp - 图像全局scale校正
// 功能: 计算scale=median(F_syn/F_instr), 应用I_cal=I*scale
// 算法: 简化版测光校准, 去掉梯度拟合, 仅做全局乘性校正
// 参考: lib/photometric_calib/flux_calibrator/python/image_corrector.py

#include "image_corrector.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace pc {

ImageCorrector::ImageCorrector() {
    std::fprintf(stderr, "[image_corrector] 初始化: 全局scale校正\n");
}

// ============================================================================
// 计算scale因子: median(F_syn / F_instr)
// ============================================================================
double ImageCorrector::computeScale(const std::vector<StarMatch>& matches) {
    int n = (int)matches.size();
    if (n == 0) {
        std::fprintf(stderr, "[image_corrector] 匹配星数为0, scale退化为1.0\n");
        return 1.0;
    }

    // 计算F_syn/F_instr比值, 排除无效值
    std::vector<double> ratios;
    ratios.reserve(n);
    for (int i = 0; i < n; ++i) {
        if (matches[i].f_instr > 0.0 && matches[i].f_syn > 0.0) {
            double ratio = matches[i].f_syn / matches[i].f_instr;
            if (std::isfinite(ratio)) {
                ratios.push_back(ratio);
            }
        }
    }

    if (ratios.empty()) {
        std::fprintf(stderr, "[image_corrector] 无有效匹配星(F_instr/F_syn<=0), scale退化为1.0\n");
        return 1.0;
    }

    // 中位数
    std::sort(ratios.begin(), ratios.end());
    double scale = ratios[ratios.size() / 2];

    std::fprintf(stderr, "[image_corrector] scale计算: 匹配星=%d, 有效=%d, scale=%.6e\n",
                n, (int)ratios.size(), scale);
    return scale;
}

// ============================================================================
// 图像校正: I_cal = I * scale
// OpenMP并行加速
// ============================================================================
void ImageCorrector::correctImage(const float* pixels, int width, int height,
                                   double scale, float* out_pixels) {
    int total = width * height;
    std::fprintf(stderr, "[image_corrector] 图像校正: %dx%d, scale=%.6e, 并行%d线程\n",
                width, height, scale,
#ifdef _OPENMP
                omp_get_max_threads());
#else
                1);
#endif

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < total; ++i) {
        out_pixels[i] = pixels[i] * (float)scale;
    }

    std::fprintf(stderr, "[image_corrector] 校正完成\n");
}

} // namespace pc

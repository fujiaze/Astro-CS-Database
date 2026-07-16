// image_corrector.cpp - gradient_2d 模块图像校正与归一化实现
// 对应 Python: lib/photometric_calib/archive/old_monolithic/image_corrector.py
// 加性梯度已封存 (S_map=0), 仅做乘性流量定标

#include "image_corrector.h"
#include "log_macros.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace pc {

Gradient2DImageCorrector::Gradient2DImageCorrector() {}

// ----------------------------------------------------------------------------

std::vector<float> Gradient2DImageCorrector::evaluate_mult_map(
    const GradientSurface& mult_surface, int width, int height) {

    std::vector<float> M_map(width * height, 0.0f);
    double sx = (width > 0) ? (2.0 / width) : 0.0;
    double sy = (height > 0) ? (2.0 / height) : 0.0;

    // 行优先: 外层 y, 内层 x (OpenMP 并行外层)
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        double y_norm = y * sy - 1.0;
        for (int x = 0; x < width; ++x) {
            double x_norm = x * sx - 1.0;
            double r = GradientFitter::eval_surface(mult_surface, x_norm, y_norm);
            double m = std::pow(10.0, r);
            if (!std::isfinite(m) || m < MIN_M) m = MIN_M;
            M_map[y * width + x] = (float)m;
        }
    }

    // 统计日志
    double m_min = 1e30, m_max = -1e30, m_sum = 0.0;
    for (int i = 0; i < width * height; ++i) {
        double v = M_map[i];
        if (v < m_min) m_min = v;
        if (v > m_max) m_max = v;
        m_sum += v;
    }
    LOG_INFO("M_map eval: %dx%d, range=[%.4f, %.4f], mean=%.4f",
             width, height, m_min, m_max, m_sum / (width * height));
    return M_map;
}

// ----------------------------------------------------------------------------

std::vector<double> Gradient2DImageCorrector::evaluate_mult_points(
    const GradientSurface& mult_surface,
    const std::vector<double>& match_x,
    const std::vector<double>& match_y,
    int img_w, int img_h) {

    std::vector<double> M_vals(match_x.size());
    double sx = (img_w > 0) ? (2.0 / img_w) : 0.0;
    double sy = (img_h > 0) ? (2.0 / img_h) : 0.0;
    for (size_t i = 0; i < match_x.size(); ++i) {
        double x_norm = match_x[i] * sx - 1.0;
        double y_norm = match_y[i] * sy - 1.0;
        double r = GradientFitter::eval_surface(mult_surface, x_norm, y_norm);
        double m = std::pow(10.0, r);
        if (!std::isfinite(m) || m < MIN_M) m = MIN_M;
        M_vals[i] = m;
    }
    return M_vals;
}

// ----------------------------------------------------------------------------

double Gradient2DImageCorrector::correct_and_normalize(
    const float* pixels, int width, int height,
    const GradientSurface& mult_surface,
    const std::vector<double>& match_x,
    const std::vector<double>& match_y,
    const std::vector<double>& f_syn,
    const std::vector<double>& f_instr,
    float* out_pixels) {

    if (!pixels || !out_pixels || width <= 0 || height <= 0) {
        LOG_ERROR("correct_and_normalize: invalid params (pixels=%p out=%p w=%d h=%d)",
                  pixels, out_pixels, width, height);
        return 1.0;
    }

    // 1. 评估全图 M_map
    auto M_map = evaluate_mult_map(mult_surface, width, height);

    // 2. 图像校正: I_cal = I / max(M, 0.01)  (S=0 已封存)
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < width * height; ++i) {
        double m = M_map[i];
        if (m < MIN_M) m = MIN_M;
        out_pixels[i] = (float)((double)pixels[i] / m);
    }

    // 3. 通量归一化: scale = median(F_syn / (F_instr / M)) = median(F_syn * M / F_instr)
    size_t n = match_x.size();
    if (n == 0 || f_syn.size() != n || f_instr.size() != n) {
        LOG_INFO("no match stars for normalize (n=%zu), scale=1.0", n);
        return 1.0;
    }

    auto M_vals = evaluate_mult_points(mult_surface, match_x, match_y, width, height);

    std::vector<double> ratios;
    ratios.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (f_instr[i] <= 0.0 || f_syn[i] <= 0.0) continue;
        if (!std::isfinite(M_vals[i])) continue;
        double f_cal = f_instr[i] / M_vals[i];
        if (f_cal <= 0.0) continue;
        ratios.push_back(f_syn[i] / f_cal);
    }
    if (ratios.empty()) {
        LOG_INFO("no valid ratios for normalize, scale=1.0");
        return 1.0;
    }
    std::sort(ratios.begin(), ratios.end());
    double scale;
    size_t m = ratios.size();
    if (m % 2 == 0) scale = (ratios[m/2 - 1] + ratios[m/2]) * 0.5;
    else scale = ratios[m/2];

    // 4. 应用 scale: I_final = I_cal * scale
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < width * height; ++i) {
        out_pixels[i] = (float)((double)out_pixels[i] * scale);
    }

    LOG_INFO("correct_and_normalize: scale=%.6e, valid_stars=%zu/%zu",
             scale, ratios.size(), n);
    return scale;
}

} // namespace pc

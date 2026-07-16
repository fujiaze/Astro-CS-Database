#ifndef PC_GRADIENT_2D_IMAGE_CORRECTOR_H
#define PC_GRADIENT_2D_IMAGE_CORRECTOR_H

#include <vector>
#include "gradient_fitter.h"

namespace pc {

// 图像梯度校正与归一化器 (gradient_2d 模块专用)
// 对应 Python: lib/photometric_calib/archive/old_monolithic/image_corrector.py
//
// 算法 (加性梯度已封存, S_map=0):
//   1. 评估乘性梯度图: r(x,y) = 多项式曲面, M(x,y) = 10^r, 钳位下限 0.01
//   2. 图像校正: I_cal(x,y) = (I(x,y) - 0) / max(M(x,y), 0.01) = I / max(M, 0.01)
//   3. 通量归一化:
//      F_cal_i = F_instr_i / M(x_i, y_i)
//      scale = median(F_syn_i / F_cal_i)
//      I_final(x,y) = I_cal(x,y) * scale
class Gradient2DImageCorrector {
public:
    Gradient2DImageCorrector();

    // 评估乘性梯度图 M(x,y) = 10^r(x,y), 钳位下限 0.01
    // 返回值: 长度 height*width 的 float32 数组, 按行优先存储
    static std::vector<float> evaluate_mult_map(
        const GradientSurface& mult_surface, int width, int height);

    // 评估匹配星位置的 M 值
    // 返回值: 长度 n 的 M 值数组 (已钳位 >= 0.01)
    static std::vector<double> evaluate_mult_points(
        const GradientSurface& mult_surface,
        const std::vector<double>& match_x,
        const std::vector<double>& match_y,
        int img_w, int img_h);

    // 图像校正 + 通量归一化 (一站式)
    // 参数:
    //   pixels: 原图像素 float32 [H*W]
    //   width, height: 图像尺寸
    //   mult_surface: 乘性梯度曲面
    //   match_x, match_y: 匹配星像素坐标 (0-based)
    //   f_syn, f_instr: 匹配星的合成流量与仪器流量
    //   out_pixels: 输出像素 (调用者分配, float32 [H*W])
    // 返回: scale 因子 (无有效匹配星时返回 1.0)
    static double correct_and_normalize(
        const float* pixels, int width, int height,
        const GradientSurface& mult_surface,
        const std::vector<double>& match_x,
        const std::vector<double>& match_y,
        const std::vector<double>& f_syn,
        const std::vector<double>& f_instr,
        float* out_pixels);

    // 乘性梯度下限 (防止除零, 见算法文档 7.2)
    static constexpr double MIN_M = 0.01;
};

} // namespace pc

#endif // PC_GRADIENT_2D_IMAGE_CORRECTOR_H

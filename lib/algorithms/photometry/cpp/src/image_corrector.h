#ifndef PC_IMAGE_CORRECTOR_H
#define PC_IMAGE_CORRECTOR_H

#include <vector>
#include "star_matcher.h"

namespace pc {

// 图像校正器: 全局scale校正
// 算法:
// scale = median(F_syn[i] / F_instr[i]) (对所有匹配星)
// I_cal = I * scale
class ImageCorrector {
public:
    ImageCorrector();

    // 计算scale因子: median(F_syn / F_instr)
    // 排除 F_instr<=0 或 F_syn<=0
    // 无有效匹配星时返回1.0
    static double computeScale(const std::vector<StarMatch>& matches);

    // 图像校正: I_cal = I * scale
    // OpenMP并行加速 (16线程)
    // 参数:
    // pixels: 原图像素 float32 [H*W]
    // width,height: 图像尺寸
    // scale: 全局缩放因子
    // out_pixels: 输出像素 (调用者分配, float32 [H*W])
    static void correctImage(const float* pixels, int width, int height,
                             double scale, float* out_pixels);
};

} // namespace pc

#endif // PC_IMAGE_CORRECTOR_H

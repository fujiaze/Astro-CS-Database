#ifndef COSMETIC_CORRECTOR_H
#define COSMETIC_CORRECTOR_H

#include <cstdint>

#ifdef _WIN32
    #define CC_EXPORT __declspec(dllexport)
#else
    #define CC_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 5×5 中值滤波修复坏像素（OpenMP并行）
// data: float32 图像数据 [H*W]，会被原地修改
// bad_mask: uint8 坏像素掩码 [H*W]，1=坏像素，0=好像素
// H, W: 图像尺寸
// window: 滤波窗口大小（奇数 3..15，默认5；偶数/<3/>15 返回-1）；15×15 为 225，栈缓冲 256，上限保证无溢出
CC_EXPORT long long cc_correct_median(
    float* data,
    const uint8_t* bad_mask,
    int H, int W,
    int window
);

// 从Dark检测热像素（全局统计 + 连通区域过滤）
// dark_data: float32 Dark图像 [H*W]
// H, W: 图像尺寸
// sigma: 检测阈值（median + sigma * 1.4826 * MAD）
// max_structure_size: 最大结构大小（大于此值的连通区域被过滤，用于排除星点）
// out_mask: 输出 uint8 掩码 [H*W]，1=热像素，0=正常
// 返回: 检测到的热像素数
CC_EXPORT long long cc_detect_hot(
    const float* dark_data,
    int H, int W,
    double sigma,
    int max_structure_size,
    uint8_t* out_mask
);

// 从Bias检测冷像素（全局统计 + 连通区域过滤）
// bias_data: float32 Bias图像 [H*W]
// 其余参数同 cc_detect_hot
// 返回: 检测到的冷像素数
CC_EXPORT long long cc_detect_cold(
    const float* bias_data,
    int H, int W,
    double sigma,
    int max_structure_size,
    uint8_t* out_mask
);

// 获取最后错误信息
CC_EXPORT const char* cc_last_error();

#ifdef __cplusplus
}
#endif

#endif // COSMETIC_CORRECTOR_H

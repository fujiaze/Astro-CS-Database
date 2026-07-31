#ifndef ASTRO_CALIBRATION_H
#define ASTRO_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define AC_API __declspec(dllexport)
#else
#define AC_API __attribute__((visibility("default")))
#endif

/* ========== 常量 ========== */
#define AC_COMBINE_MEAN   0
#define AC_COMBINE_MEDIAN 1

#define AC_METHOD_MEDIAN  0
#define AC_METHOD_BILINEAR 1

#define AC_OK             0
#define AC_ERR_PARAM     -1
#define AC_ERR_MEMORY    -2
#define AC_ERR_INTERNAL  -3

/* ========== 主帧生成 ========== */

/* 生成 Master Bias：sigma-clip + median/mean 合并
 * stack: [n_frames * height * width] float32 行优先
 * out: [height * width] float32
 * combine: AC_COMBINE_MEAN 或 AC_COMBINE_MEDIAN
 */
AC_API int ac_generate_master_bias(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine);

/* 生成 Master Dark：sigma-clip + median/mean 合并（不减Bias，Dark已含Bias）
 * 同 ac_generate_master_bias
 */
AC_API int ac_generate_master_dark(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine);

/* 生成 Master Flat：减Bias + 逐帧归一化 + sigma-clip + mean + 再归一化
 * flat_stack: [n_frames * height * width]
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 */
AC_API int ac_generate_master_flat(
    const float* flat_stack, int n_frames, int width, int height,
    const float* master_bias,
    float* out,
    float sigma_low, float sigma_high, int max_iterations);

/* ========== 图像校准 ========== */

/* 校准单帧 Light
 * 无暗场优化: (Light - Dark) / Flat
 * 有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat
 *
 * light: [height * width]
 * master_dark: [height * width] 或 NULL
 * master_flat: [height * width] 或 NULL
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 * dark_optimization: 0=关闭, 1=开启
 * dark_scale_factor: K初始值（如 Light曝光/Dark曝光）
 * actual_k: 输出实际使用的K值（可选，NULL则不输出）
 */
AC_API int ac_calibrate_frame(
    const float* light, int width, int height,
    const float* master_dark, const float* master_flat, const float* master_bias,
    float* out,
    int dark_optimization, float dark_scale_factor,
    float* actual_k);

/* ========== 坏点修复 ========== */

/* 校正单帧图像的坏点
 * 检测方法：Dark全局统计检测热像素 + Bias全局统计检测冷像素
 *
 * data: [height * width] 校准后Light
 * master_dark: [height * width] 或 NULL
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 * hot_sigma: Dark热像素检测sigma倍数
 * cold_sigma: Bias冷像素检测sigma倍数
 * method: AC_METHOD_MEDIAN 或 AC_METHOD_BILINEAR
 * max_structure_size: 连通区域大小阈值（>=此值视为星点）
 * out_hot: 输出热像素数（可选）
 * out_cold: 输出冷像素数（可选）
 */
AC_API int ac_correct_frame(
    const float* data, int width, int height,
    const float* master_dark, const float* master_bias,
    float* out,
    float hot_sigma, float cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold);

/* ========== 工具 ========== */

/* 设置OpenMP线程数 */
AC_API void ac_set_num_threads(int n);

/* 获取版本号 */
AC_API const char* ac_version();

#ifdef __cplusplus
}
#endif

/* ========== C++ 接口（最优 Dark 系数估计）========== */
/* 纯 C++ 接口，使用 hiss::Stage1Diagnostics 结构化诊断，仅供 C++ 调用方使用。
 * 对应实现见 src/dark_optimizer.cpp。
 */
#ifdef __cplusplus
#include "hiss_format.h"  // hiss::Stage1Diagnostics (02_FROZEN §2.3)
namespace ac {
/* 最优 Dark 系数估计
 * 模型: L - B = c + k*(D - B)
 * 算法: 背景提取(sigma-clip) + 8x8 分区抽样 + 鲁棒线性回归(MAD 离群抑制, 5 轮迭代)
 * 失败处理: 输出结构化诊断后自动回退曝光时间比例 k_init，
 *           设置 diagnostics.fell_back=1, fallback_from="OPTIMAL", fallback_to="EXPOSURE_RATIO"
 * 返回: 最优 k 值；失败时返回 k_init
 */
float optimize_dark_k(const float* light, const float* bias, const float* dark,
                      const float* flat, int w, int h, float k_init,
                      hiss::Stage1Diagnostics& diagnostics);
}
#endif

#endif /* ASTRO_CALIBRATION_H */

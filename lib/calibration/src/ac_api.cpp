// ac_api.cpp
// C API 导出层 - 天文CCD校准 (Astro Calibration)
//
// 功能：将 namespace ac 中的核心算法包装为 extern "C" 接口，
//       供 Python ctypes 调用。
//
// 设计要点：
//   - 所有导出函数使用 extern "C"，避免名称修饰
//   - 参数校验后转发到 ac:: 命名空间的实现
//   - 不包含文件IO，仅操作内存数组
//   - 返回 AC_OK(0) 成功，负数表示错误

#include "../include/astro_calibration.h"
#include <omp.h>
#include <cstring>

// 从 master_generator.cpp
namespace ac {
    void generate_master(const float* stack, int n_frames, int w, int h,
                         float* out, float sigma_low, float sigma_high,
                         int max_iter, int combine);
    void generate_master_flat(const float* flat_stack, int n_frames, int w, int h,
                              const float* bias, float* out,
                              float sigma_low, float sigma_high, int max_iter);
}

// 从 calibrator.cpp
namespace ac {
    void calibrate(const float* light, int w, int h,
                   const float* dark, const float* flat, const float* bias,
                   float* out, int dark_opt, float k_init, float* actual_k);
}

// 从 cosmetic_corrector.cpp
namespace ac {
    void correct_frame(const float* data, int w, int h,
                       const float* dark, const float* bias,
                       float* out,
                       float hot_sigma, float cold_sigma,
                       int method, int max_size,
                       int* out_hot, int* out_cold);
}

// ======================== C API 导出 ========================

extern "C" {

AC_API int ac_generate_master_bias(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine) {
    if (!stack || !out || n_frames <= 0 || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    ac::generate_master(stack, n_frames, width, height, out,
                        sigma_low, sigma_high, max_iterations, combine);
    return AC_OK;
}

AC_API int ac_generate_master_dark(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine) {
    if (!stack || !out || n_frames <= 0 || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    // Dark 主帧生成与 Bias 完全相同（sigma-clip + median/mean）
    // Dark 不减 Bias（已含 Bias）
    ac::generate_master(stack, n_frames, width, height, out,
                        sigma_low, sigma_high, max_iterations, combine);
    return AC_OK;
}

AC_API int ac_generate_master_flat(
    const float* flat_stack, int n_frames, int width, int height,
    const float* master_bias,
    float* out,
    float sigma_low, float sigma_high, int max_iterations) {
    if (!flat_stack || !out || n_frames <= 0 || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    ac::generate_master_flat(flat_stack, n_frames, width, height,
                             master_bias, out,
                             sigma_low, sigma_high, max_iterations);
    return AC_OK;
}

AC_API int ac_calibrate_frame(
    const float* light, int width, int height,
    const float* master_dark, const float* master_flat, const float* master_bias,
    float* out,
    int dark_optimization, float dark_scale_factor,
    float* actual_k) {
    if (!light || !out || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    ac::calibrate(light, width, height,
                  master_dark, master_flat, master_bias,
                  out, dark_optimization, dark_scale_factor, actual_k);
    return AC_OK;
}

AC_API int ac_correct_frame(
    const float* data, int width, int height,
    const float* master_dark, const float* master_bias,
    float* out,
    float hot_sigma, float cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold) {
    if (!data || !out || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    ac::correct_frame(data, width, height,
                      master_dark, master_bias,
                      out,
                      hot_sigma, cold_sigma,
                      method, max_structure_size,
                      out_hot, out_cold);
    return AC_OK;
}

AC_API void ac_set_num_threads(int n) {
    if (n > 0) omp_set_num_threads(n);
}

AC_API const char* ac_version() {
    return "Astro Calibration C++ v1.0.0";
}

} // extern "C"

// ac_api.cpp
// C API 导出层 - 天文CCD校准 (Astro Calibration)
//
// 功能：将 namespace ac 中的核心算法包装为 extern "C" 接口，
// 供 Python ctypes 调用。
//
// 设计要点：
// - 所有导出函数使用 extern "C"，避免名称修饰
// - 参数校验后转发到 ac:: 命名空间的实现
// - 不包含文件IO，仅操作内存数组
// - 返回 AC_OK(0) 成功，负数表示错误

#include "../include/astro_calibration.h"
#include <omp.h>
#include <cstring>
#include <vector>

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
    // 双精度 ABI: double 版本校准, 精度关键路径不降级到 float32
    void calibrate_d(const double* light, int w, int h,
                     const double* dark, const double* flat, const double* bias,
                     double* out, int dark_opt, double k_init, double* actual_k);
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

// ======================== 双精度 ABI (FP64) ========================
// FP64 模式下全链路使用 double, 不降级到 float32
//
// ac_calibrate_frame_f64: 调用 ac::calibrate_d, 像素级算术在 double 上运行
// (精度关键路径, 真正双精度)。
// ac_generate_master_*_f64 / ac_correct_frame_f64: 统计/mask 操作,
// 内部将 double 输入转 float 调用 f32 实现, 输出转回 double。
// (orchestrator 的 run_stage_calibrate 不调用这些函数)

AC_API int ac_generate_master_bias_f64(
    const double* stack, int n_frames, int width, int height,
    double* out,
    double sigma_low, double sigma_high, int max_iterations,
    int combine) {
    if (!stack || !out || n_frames <= 0 || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    // 统计操作: 转 float 调用 f32 实现, 输出转回 double
    int64_t n_pix = static_cast<int64_t>(width) * height * n_frames;
    std::vector<float> stack_f32(n_pix);
    for (int64_t i = 0; i < n_pix; ++i) stack_f32[i] = static_cast<float>(stack[i]);
    std::vector<float> out_f32(static_cast<int64_t>(width) * height);
    ac::generate_master(stack_f32.data(), n_frames, width, height, out_f32.data(),
                        static_cast<float>(sigma_low), static_cast<float>(sigma_high),
                        max_iterations, combine);
    int64_t out_n = static_cast<int64_t>(width) * height;
    for (int64_t i = 0; i < out_n; ++i) out[i] = static_cast<double>(out_f32[i]);
    return AC_OK;
}

AC_API int ac_generate_master_dark_f64(
    const double* stack, int n_frames, int width, int height,
    double* out,
    double sigma_low, double sigma_high, int max_iterations,
    int combine) {
    if (!stack || !out || n_frames <= 0 || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    int64_t n_pix = static_cast<int64_t>(width) * height * n_frames;
    std::vector<float> stack_f32(n_pix);
    for (int64_t i = 0; i < n_pix; ++i) stack_f32[i] = static_cast<float>(stack[i]);
    std::vector<float> out_f32(static_cast<int64_t>(width) * height);
    ac::generate_master(stack_f32.data(), n_frames, width, height, out_f32.data(),
                        static_cast<float>(sigma_low), static_cast<float>(sigma_high),
                        max_iterations, combine);
    int64_t out_n = static_cast<int64_t>(width) * height;
    for (int64_t i = 0; i < out_n; ++i) out[i] = static_cast<double>(out_f32[i]);
    return AC_OK;
}

AC_API int ac_generate_master_flat_f64(
    const double* flat_stack, int n_frames, int width, int height,
    const double* master_bias,
    double* out,
    double sigma_low, double sigma_high, int max_iterations) {
    if (!flat_stack || !out || n_frames <= 0 || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    int64_t n_pix = static_cast<int64_t>(width) * height;
    std::vector<float> flat_f32(static_cast<int64_t>(n_pix) * n_frames);
    for (int64_t i = 0; i < static_cast<int64_t>(n_pix) * n_frames; ++i)
        flat_f32[i] = static_cast<float>(flat_stack[i]);
    std::vector<float> bias_f32;
    const float* bias_ptr = nullptr;
    if (master_bias) {
        bias_f32.resize(n_pix);
        for (int64_t i = 0; i < n_pix; ++i) bias_f32[i] = static_cast<float>(master_bias[i]);
        bias_ptr = bias_f32.data();
    }
    std::vector<float> out_f32(n_pix);
    ac::generate_master_flat(flat_f32.data(), n_frames, width, height,
                             bias_ptr, out_f32.data(),
                             static_cast<float>(sigma_low), static_cast<float>(sigma_high),
                             max_iterations);
    for (int64_t i = 0; i < n_pix; ++i) out[i] = static_cast<double>(out_f32[i]);
    return AC_OK;
}

// 精度关键路径: 直接调用 ac::calibrate_d, 像素级算术在 double 上运行 (不降级)
AC_API int ac_calibrate_frame_f64(
    const double* light, int width, int height,
    const double* master_dark, const double* master_flat, const double* master_bias,
    double* out,
    int dark_optimization, double dark_scale_factor,
    double* actual_k) {
    if (!light || !out || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    ac::calibrate_d(light, width, height,
                    master_dark, master_flat, master_bias,
                    out, dark_optimization, dark_scale_factor, actual_k);
    return AC_OK;
}

AC_API int ac_correct_frame_f64(
    const double* data, int width, int height,
    const double* master_dark, const double* master_bias,
    double* out,
    double hot_sigma, double cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold) {
    if (!data || !out || width <= 0 || height <= 0)
        return AC_ERR_PARAM;
    // 坏点修复 (mask 操作): 转 float 调用 f32 实现, 输出转回 double
    int64_t n_pix = static_cast<int64_t>(width) * height;
    std::vector<float> data_f32(n_pix);
    for (int64_t i = 0; i < n_pix; ++i) data_f32[i] = static_cast<float>(data[i]);
    std::vector<float> dark_f32, bias_f32;
    const float* dark_ptr = nullptr;
    const float* bias_ptr = nullptr;
    if (master_dark) {
        dark_f32.resize(n_pix);
        for (int64_t i = 0; i < n_pix; ++i) dark_f32[i] = static_cast<float>(master_dark[i]);
        dark_ptr = dark_f32.data();
    }
    if (master_bias) {
        bias_f32.resize(n_pix);
        for (int64_t i = 0; i < n_pix; ++i) bias_f32[i] = static_cast<float>(master_bias[i]);
        bias_ptr = bias_f32.data();
    }
    std::vector<float> out_f32(n_pix);
    ac::correct_frame(data_f32.data(), width, height,
                      dark_ptr, bias_ptr,
                      out_f32.data(),
                      static_cast<float>(hot_sigma), static_cast<float>(cold_sigma),
                      method, max_structure_size,
                      out_hot, out_cold);
    for (int64_t i = 0; i < n_pix; ++i) out[i] = static_cast<double>(out_f32[i]);
    return AC_OK;
}

} // extern "C"

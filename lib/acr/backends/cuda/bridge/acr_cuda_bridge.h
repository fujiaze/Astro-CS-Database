// lib/acr/backends/cuda/bridge/acr_cuda_bridge.h — CUDA 桥接 C ABI
//
// 背景（23 号计划 §3）：ACR 主构建使用 MSYS2 MinGW 工具链，nvcc 11.8 不支持
// MinGW host；本桥接 DLL 用 MSVC + nvcc 构建，通过纯 C ABI 暴露真实 GPU kernel，
// 由 MinGW 侧加载器（cuda_bridge_loader.cpp）LoadLibrary 动态调用——
// 与项目现有 DLL 模块架构一致，无 ABI 冲突。
//
// 构建（证据命令记录于 docs/cuda_bridge_build.md）：
//   vcvars64 → nvcc -arch=sm_86 --allow-unsupported-compiler \
//     -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -shared \
//     acr_cuda_bridge.cu -o acr_cuda_bridge.dll
//
// 错误约定：所有函数返回 0 表示成功；非 0 时 *last_error 指向静态错误串。
#ifndef ACR_CUDA_BRIDGE_H
#define ACR_CUDA_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(ACR_CUDA_BRIDGE_BUILD)
#  define ACR_CUDA_BRIDGE_API __declspec(dllexport)
#elif defined(_WIN32)
#  define ACR_CUDA_BRIDGE_API __declspec(dllimport)
#else
#  define ACR_CUDA_BRIDGE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ===== 设备探测 =====
// 初始化 CUDA 并枚举设备；返回设备数（无设备/驱动错误返回 0）。
ACR_CUDA_BRIDGE_API int acr_cuda_bridge_init(const char** last_error);
ACR_CUDA_BRIDGE_API int acr_cuda_bridge_device_count(void);
ACR_CUDA_BRIDGE_API const char* acr_cuda_bridge_device_name(int device);

// 25 号计划 §3.4：真实设备元数据（显存、SM/CU、计算能力）
ACR_CUDA_BRIDGE_API int acr_cuda_device_memory(
    int device, uint64_t* total_bytes, uint64_t* free_bytes,
    const char** last_error);
ACR_CUDA_BRIDGE_API int acr_cuda_device_compute(
    int device, int* sm_count, int* cc_major, int* cc_minor,
    const char** last_error);

// ===== Executor 句柄 =====
// recommended_chunk/min_chunk 用于预分配设备缓冲（AXPY/COPY/CONV 按块工作）。
ACR_CUDA_BRIDGE_API void* acr_cuda_executor_create(
    int device, size_t recommended_chunk,
    size_t min_chunk, const char** last_error);
ACR_CUDA_BRIDGE_API void acr_cuda_executor_destroy(void* handle);
ACR_CUDA_BRIDGE_API int acr_cuda_executor_available(void* handle);
ACR_CUDA_BRIDGE_API int acr_cuda_executor_sync(void* handle, const char** last_error);

// ===== 真实 GPU kernel 提交 =====
// 全部为同步语义：H2D → launch → D2H（y 在 host 上），elapsed_ns 为真实耗时。

// y[i] = alpha * x[i] + y[i]，i in [begin, end)
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_axpy(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x, float alpha,
    uint64_t* elapsed_ns, const char** last_error);

// y[i] = x[i]，i in [begin, end)
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_copy(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x,
    uint64_t* elapsed_ns, const char** last_error);

// 归约（FP64 累加）：partials[chunk_index * blocks + blockIdx] = sum(x[begin..end) 的 block 和)
// blocks_per_chunk 固定（如 256），merge 阶段累加全部 partials。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_reduce(
    void* handle,
    size_t begin, size_t end,
    const float* x,
    double* partials,
    size_t blocks_per_chunk,
    uint64_t chunk_index,
    uint64_t* elapsed_ns, const char** last_error);

// 3x3 直接卷积（y 为 w*h 连续行主序，kernel9 为 3x3 权重）
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_conv3x3(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x,
    size_t width, size_t height,
    const float* kernel9,
    uint64_t* elapsed_ns, const char** last_error);

#ifdef __cplusplus
}
#endif

#endif // ACR_CUDA_BRIDGE_H

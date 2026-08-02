// lib/acr/backends/cuda/cuda_backend.hpp — CUDA 设备管理 + kernel 执行
// Phase D：纯 CUDA backend（不依赖 alpaka，ADR-001 评估延后）。
//
// 设计（ADR-009 CPU-only build gate）：
//   1. 整个 header 用 #ifdef ACR_BUILD_CUDA 保护，CPU-only 构建不展开任何 CUDA 符号
//   2. CudaBackend singleton lazy init；无设备/驱动错误时 available()=false，runtime 不崩溃
//   3. cudaError 转 StatusCode（DeviceLost / KernelFailed / OutOfMemory）
//   4. initialize() 内注册 GPU 报告回调到 topology（generate_hardware_report 含 GPU 字段）
//   5. cuda_parallel_for 模板仅在 CUDA 编译单元（__CUDACC__）可见，用 <<<>>> 启动
//   6. 公共 acr.hpp 不 include 本头；本头仅供 cuda/ 内部 + 测试 + example 使用
#pragma once

#ifdef ACR_BUILD_CUDA

#include <astro/compute/acr.hpp>  // StatusCode

#include <cstddef>
#include <string>

#include <cuda_runtime.h>

namespace astro::compute::cuda {

// ===== CUDA 设备信息 =====
struct CudaDeviceInfo {
    int device_id{-1};
    std::string name;
    std::string uuid;            // GPU UUID（"GPU-xxxxxxxx-xxxx-xxxx-..."）
    int compute_major{0};
    int compute_minor{0};        // compute capability (major.minor)
    std::size_t total_memory{0};
    int driver_major{0};
    int driver_minor{0};         // 驱动版本 (major.minor)
    int sm_count{0};             // SM 多处理器数量
    std::size_t free_memory{0};
};

// ===== cudaError 转 StatusCode =====
StatusCode cuda_error_to_status(cudaError_t err) noexcept;

// ===== CudaBackend：singleton lazy init =====
// 无 CUDA 设备 / 驱动错误时 available()=false，调用者回退 CPU（不抛异常）。
class CudaBackend {
public:
    static CudaBackend& instance();

    // 初始化：枚举设备、选择 device 0、创建 stream、注册 GPU 报告回调。
    // 返回 Ok / DeviceLost / OutOfMemory / KernelFailed。
    // 幂等：多次调用安全（std::call_once 保护）。
    StatusCode initialize();

    bool available() const noexcept { return has_device_; }
    const CudaDeviceInfo& device_info() const noexcept { return info_; }
    cudaStream_t stream() const noexcept { return stream_; }
    int device_count() const noexcept { return device_count_; }

    // 同步默认 stream
    StatusCode sync() noexcept;

    // GPU 报告 JSON（注册到 hardware_report 的回调签名）
    static std::string gpu_report_json();

    ~CudaBackend();
    CudaBackend(const CudaBackend&) = delete;
    CudaBackend& operator=(const CudaBackend&) = delete;

private:
    CudaBackend();

    bool initialized_{false};
    bool has_device_{false};
    int device_count_{0};
    CudaDeviceInfo info_{};
    cudaStream_t stream_{nullptr};
};

// ===== cuda_parallel_for：1D grid/block 自动计算 + 启动 =====
// 仅在 CUDA 编译单元可用（依赖 <<<>>> 启动语法 + __global__ 转发 kernel）。
// Functor 需 __device__ 可调用，签名 void(std::size_t idx)。
template <class Functor>
inline StatusCode cuda_parallel_for(const char* /*name*/, std::size_t n,
                                    Functor functor,
                                    cudaStream_t stream = nullptr);

// ===== AXPY kernel 启动器 =====
// y[i] = a * x[i] + y[i]，y/x 为 device pointer，n 为元素数。
// 内部通过 cuda_parallel_for 启动，stream 默认用 CudaBackend::stream()。
StatusCode axpy(float* y, const float* x, float a, std::size_t n,
                cudaStream_t stream = nullptr) noexcept;

} // namespace astro::compute::cuda

// ===== 模板实现（仅 nvcc 编译单元可见）=====
#ifdef __CUDACC__

namespace astro::compute::cuda {

// __global__ 转发 kernel：把 functor 拷贝到 device 并按线程索引调用
template <class Functor>
__global__ void parallel_for_kernel(Functor f, std::size_t n) {
    const std::size_t idx =
        static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx < n) {
        f(idx);
    }
}

template <class Functor>
inline StatusCode cuda_parallel_for(const char* /*name*/, std::size_t n,
                                    Functor functor,
                                    cudaStream_t stream) {
    if (n == 0) return StatusCode::Ok;
    const int block = 256;
    const std::size_t grid = (n + block - 1) / block;
    parallel_for_kernel<<<static_cast<unsigned int>(grid), block, 0, stream>>>(
        functor, n);
    cudaError_t err = cudaGetLastError();
    return cuda_error_to_status(err);
}

} // namespace astro::compute::cuda

#endif // __CUDACC__

#endif // ACR_BUILD_CUDA

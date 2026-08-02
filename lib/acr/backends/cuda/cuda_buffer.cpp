// lib/acr/backends/cuda/cuda_buffer.cpp — 非模板辅助函数
// Phase D：CudaBuffer 模板类内联在 header，本 .cpp 仅提供非模板辅助。
#ifdef ACR_BUILD_CUDA

#include "cuda_buffer.hpp"

namespace astro::compute::cuda {

// 查询设备 free/total memory（CudaBuffer 分配前预估用）
StatusCode query_device_memory(std::size_t& free, std::size_t& total) noexcept {
    cudaError_t err = cudaMemGetInfo(&free, &total);
    return cuda_error_to_status(err);
}

} // namespace astro::compute::cuda

#endif // ACR_BUILD_CUDA

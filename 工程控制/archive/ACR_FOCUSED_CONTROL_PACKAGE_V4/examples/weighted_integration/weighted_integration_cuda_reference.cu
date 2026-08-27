// Reference only: adapt to the repository's CUDA bridge/device-view API.
// Do not create a host vector or upload the full frame stack per WorkToken.
#include <cuda_runtime.h>
#include <cstddef>

namespace acr_example {

__global__ void weighted_integration_kernel(const float* __restrict__ frames,
                                            const float* __restrict__ weights,
                                            std::size_t frame_count,
                                            std::size_t pixel_count,
                                            std::size_t begin,
                                            std::size_t end,
                                            float* __restrict__ output) {
    const std::size_t p = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (p >= end) return;
    double numerator = 0.0;
    double denominator = 0.0;
    for (std::size_t f = 0; f < frame_count; ++f) {
        const double w = static_cast<double>(weights[f]);
        numerator += w * static_cast<double>(frames[f * pixel_count + p]);
        denominator += w;
    }
    output[p] = static_cast<float>(numerator / denominator);
}

// Frozen launcher semantics:
// 1. frames/weights/output are resolved device views owned by ResidencyManager.
// 2. stream is an internal CudaExecutor slot, never exposed to business code.
// 3. [begin,end) is the WorkToken-owned output range.
// 4. submit asynchronously and complete via CUDA event.
inline cudaError_t launch_weighted_integration_resident(
    const float* d_frames,
    const float* d_weights,
    std::size_t frame_count,
    std::size_t pixel_count,
    std::size_t begin,
    std::size_t end,
    float* d_output,
    cudaStream_t stream) {
    constexpr unsigned block = 256;
    const unsigned grid = static_cast<unsigned>((end - begin + block - 1) / block);
    weighted_integration_kernel<<<grid, block, 0, stream>>>(
        d_frames, d_weights, frame_count, pixel_count,
        begin, end, d_output);
    return cudaGetLastError();
}

} // namespace acr_example

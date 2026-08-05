// lib/acr/backends/cuda/bridge/acr_cuda_bridge_kernels.cu — CUDA kernels + launch 包装
//
// 仅包含 __global__ kernel 与 thin launch 包装（无 STL），供 nvcc 编译。
// host 逻辑（mutex/缓冲管理/extern "C" ABI）在 acr_cuda_bridge_host.cpp。
#include <cuda_runtime.h>

extern "C" {

// ===== Kernels =====
__global__ void acr_axpy_kernel(float* y, const float* x, float alpha,
                                size_t begin, size_t n) {
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (i < begin + n) y[i] = alpha * x[i] + y[i];
}

__global__ void acr_copy_kernel(float* y, const float* x,
                                size_t begin, size_t n) {
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (i < begin + n) y[i] = x[i];
}

__global__ void acr_reduce_kernel(const float* x, double* partials,
                                  size_t begin, size_t n,
                                  size_t chunk_index, size_t blocks_per_chunk) {
    extern __shared__ double sdata[];  // FP64 局部累加（24 号计划 §5.1）
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    sdata[threadIdx.x] = (i < begin + n) ? static_cast<double>(x[i]) : 0.0;
    __syncthreads();
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < static_cast<unsigned>(stride)) {
            sdata[threadIdx.x] += sdata[threadIdx.x + stride];
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) {
        // 写入本地 block 槽位（0..blocks_per_chunk-1）；
        // chunk 区域偏移由 host 侧 D2H 拷贝（partials + chunk_index*blocks）处理
        (void)chunk_index;
        (void)blocks_per_chunk;
        atomicAdd(&partials[blockIdx.x], sdata[0]);
    }
}

__global__ void acr_conv3x3_kernel(float* y, const float* x,
                                   size_t begin, size_t n,
                                   size_t width, size_t height,
                                   const float* k) {
    const size_t p = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (p >= begin + n) return;
    const int px = static_cast<int>(p % width);
    const int py = static_cast<int>(p / width);
    float acc = 0.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int nx = px + dx;
            const int ny = py + dy;
            if (nx < 0 || ny < 0 ||
                nx >= static_cast<int>(width) ||
                ny >= static_cast<int>(height)) {
                continue;
            }
            const size_t src = static_cast<size_t>(ny) * width + nx;
            acc += x[src] * k[(dy + 1) * 3 + (dx + 1)];
        }
    }
    y[p] = acc;
}

constexpr int kThreads = 256;

inline int grid_size(size_t n) {
    return static_cast<int>((n + kThreads - 1) / kThreads);
}

// ===== Launch 包装（host callable，由 host .cpp 调用）=====
void acr_launch_axpy(float* y, const float* x, float alpha,
                     size_t begin, size_t n, cudaStream_t stream) {
    acr_axpy_kernel<<<grid_size(n), kThreads, 0, stream>>>(y, x, alpha, begin, n);
}

void acr_launch_copy(float* y, const float* x,
                     size_t begin, size_t n, cudaStream_t stream) {
    acr_copy_kernel<<<grid_size(n), kThreads, 0, stream>>>(y, x, begin, n);
}

void acr_launch_reduce(const float* x, double* partials,
                       size_t begin, size_t n,
                       size_t chunk_index, size_t blocks_per_chunk,
                       cudaStream_t stream) {
    acr_reduce_kernel<<<static_cast<int>(blocks_per_chunk), kThreads,
                        kThreads * sizeof(double), stream>>>(
        x, partials, begin, n, chunk_index, blocks_per_chunk);
}

void acr_launch_conv3x3(float* y, const float* x,
                        size_t begin, size_t n,
                        size_t width, size_t height,
                        const float* k, cudaStream_t stream) {
    acr_conv3x3_kernel<<<grid_size(n), kThreads, 0, stream>>>(
        y, x, begin, n, width, height, k);
}

} // extern "C"

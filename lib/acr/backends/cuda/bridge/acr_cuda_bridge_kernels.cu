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

// 25 号计划 §2.2：分块卷积使用全局输出索引读图、chunk-local 索引写输出。
// begin 为全局像素偏移（读图坐标 = begin + idx），输出写入 y[idx]（chunk 局部），
// host 侧把 d_y 拷贝回 y + begin。x 始终为完整输入图像（整图 H2D）。
__global__ void acr_conv3x3_kernel(float* y, const float* x,
                                   size_t begin, size_t n,
                                   size_t width, size_t height,
                                   const float* k) {
    const size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    const size_t p = begin + idx;   // 全局输出像素
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
    y[idx] = acc;  // chunk-local 输出槽位
}

// ===== 聚焦版（08 号计划 §3）：目标合成内核 =====
// Dense pixel accumulate：FP32 输入 + FP64 累加器
__global__ void acr_dense_accumulate_fp64acc_kernel(float* y, const float* x,
                                                     size_t begin, size_t n) {
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (i < begin + n) {
        const double acc = static_cast<double>(y[i]) + static_cast<double>(x[i]);
        y[i] = static_cast<float>(acc);
    }
}

// Drizzle-like scatter：确定性 hash 桶 + FP64 原子累加
__device__ __forceinline__ size_t acr_hash_bin(size_t i, size_t bins) {
    size_t h = i;
    h ^= h >> 17; h *= 0xed5ad4bbU; h ^= h >> 11;
    return h % bins;
}

__global__ void acr_drizzle_scatter_kernel(const float* x, double* partials,
                                            size_t begin, size_t n,
                                            size_t bins) {
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (i < begin + n) {
        const size_t b = acr_hash_bin(i, bins);
        atomicAdd(&partials[b], static_cast<double>(x[i]));
    }
}

// Resident chain：y[i] = x[i]+1 → z[i] = y[i]*2（两个 kernel，显存中间值）
__global__ void acr_chain_k1_kernel(float* y, const float* x,
                                    size_t begin, size_t n) {
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (i < begin + n) y[i] = x[i] + 1.0f;
}

__global__ void acr_chain_k2_kernel(float* z, const float* y,
                                    size_t begin, size_t n) {
    const size_t i = begin + blockIdx.x * blockDim.x + threadIdx.x;
    if (i < begin + n) z[i] = y[i] * 2.0f;
}

// 空 kernel：launch/event/sync 固定开销
__global__ void acr_empty_kernel(size_t /*begin*/, size_t /*n*/) {}

// ===== ACR 架构冻结（07 号计划 C）：加权积分（FP64 累加）=====
// output[p] = Σ_f weight[f]*frame[f,p] / Σ_f weight[f]
// frames 为整帧驻留（d_frames，frame-major）；weights 为驻留权重（d_weights）。
// 输出按 chunk-local 索引写 d_out[idx]，host 侧拷贝回 output + begin
// （与分块卷积同一全局偏移 + chunk-local 写约定）。
__global__ void acr_weighted_integration_kernel(
    const float* __restrict__ frames,
    const float* __restrict__ weights,
    size_t frame_count,
    size_t pixel_count,
    size_t begin,
    size_t n,
    float* __restrict__ output) {
    const size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    const size_t p = begin + idx;
    double numerator = 0.0;
    double denominator = 0.0;
    for (size_t f = 0; f < frame_count; ++f) {
        const double w = static_cast<double>(weights[f]);
        numerator += w * static_cast<double>(frames[f * pixel_count + p]);
        denominator += w;
    }
    output[idx] = static_cast<float>(numerator / denominator);
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

void acr_launch_dense_accumulate_fp64acc(float* y, const float* x,
                                         size_t begin, size_t n,
                                         cudaStream_t stream) {
    acr_dense_accumulate_fp64acc_kernel<<<grid_size(n), kThreads, 0, stream>>>(
        y, x, begin, n);
}

void acr_launch_drizzle_scatter(const float* x, double* partials,
                                size_t begin, size_t n, size_t bins,
                                cudaStream_t stream) {
    acr_drizzle_scatter_kernel<<<grid_size(n), kThreads, 0, stream>>>(
        x, partials, begin, n, bins);
}

void acr_launch_chain(float* y, float* z, const float* x,
                      size_t begin, size_t n, cudaStream_t stream) {
    acr_chain_k1_kernel<<<grid_size(n), kThreads, 0, stream>>>(y, x, begin, n);
    acr_chain_k2_kernel<<<grid_size(n), kThreads, 0, stream>>>(z, y, begin, n);
}

void acr_launch_empty(size_t begin, size_t n, cudaStream_t stream) {
    acr_empty_kernel<<<grid_size(n), kThreads, 0, stream>>>(begin, n);
}

void acr_launch_weighted_integration(const float* frames,
                                     const float* weights,
                                     size_t frame_count,
                                     size_t pixel_count,
                                     size_t begin, size_t n,
                                     float* output,
                                     cudaStream_t stream) {
    acr_weighted_integration_kernel<<<grid_size(n), kThreads, 0, stream>>>(
        frames, weights, frame_count, pixel_count, begin, n, output);
}

} // extern "C"

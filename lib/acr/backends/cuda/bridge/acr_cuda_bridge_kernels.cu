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

// ===== Phase2 mosaic_reject（synthetic.mosaic_reject.fp64acc）=====
// 与 lib/phase2 CPU reference（p2_reject_stack + p2_integrate_pixel）同语义：
//   - 每像素收集有效样本（finite && support>0）；
//   - 迭代 sigma-clip：median + MAD(1.4826×median|Δ|)，low/high 边界；
//   - 样本不足（< min_samples）fallback=全接受（single-coverage 稳定语义）；
//   - 输出 = 接受样本的 SNR²×support 加权均值（0 = 无有效/全拒）。
// 计算全程 FP64（与 CPU reference 数值一致）；输入/输出为 FP32 buffer。
__device__ __forceinline__ void acr_sort_asc(double* v, int n) {
    for (int i = 1; i < n; ++i) {
        const double key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            --j;
        }
        v[j + 1] = key;
    }
}

__device__ __forceinline__ double acr_median_sorted(const double* s, int n) {
    if (n % 2 == 1) return s[n / 2];
    return 0.5 * (s[n / 2 - 1] + s[n / 2]);
}

__global__ void acr_mosaic_reject_kernel(
    const float* __restrict__ frames,
    const float* __restrict__ support,
    const float* __restrict__ frame_snr,
    size_t frame_count,
    size_t pixel_count,
    size_t begin,
    size_t n,
    float sigma_low,
    float sigma_high,
    int max_iterations,
    int min_samples,
    float* __restrict__ output,
    float* __restrict__ out_support,
    float* __restrict__ out_reject_count,
    float* __restrict__ out_valid_count) {
    const size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    if (frame_count > 64) {
        output[idx] = 0.0f;
        if (out_support) out_support[idx] = 0.0f;
        if (out_reject_count) out_reject_count[idx] = 0.0f;
        if (out_valid_count) out_valid_count[idx] = 0.0f;
        return;
    }
    const size_t p = begin + idx;

    double vals[64], w[64], sup[64];
    int nv = 0;
    for (size_t f = 0; f < frame_count; ++f) {
        const float v = frames[f * pixel_count + p];
        const float s = support ? support[f * pixel_count + p] : 1.0f;
        if (v != v || s != s) continue;                 // NaN
        if (v > 1e30f || v < -1e30f) continue;          // Inf
        if (s <= 0.0f) continue;
        vals[nv] = static_cast<double>(v);
        sup[nv] = static_cast<double>(s);
        const double snr = frame_snr ? static_cast<double>(frame_snr[f]) : 1.0;
        w[nv] = static_cast<double>(s) * snr * snr;
        ++nv;
    }
    if (nv == 0) {
        output[idx] = 0.0f;
        if (out_support) out_support[idx] = 0.0f;
        if (out_reject_count) out_reject_count[idx] = 0.0f;
        if (out_valid_count) out_valid_count[idx] = (float)nv;
        return;
    }

    bool acc[64];
    for (int i = 0; i < nv; ++i) acc[i] = true;

    if (nv >= min_samples) {
        for (int it = 0; it < max_iterations; ++it) {
            double cur[64], sorted[64];
            int nc = 0;
            for (int i = 0; i < nv; ++i) {
                if (acc[i]) { cur[nc] = vals[i]; ++nc; }
            }
            if (nc < 2) break;
            for (int j = 0; j < nc; ++j) sorted[j] = cur[j];
            acr_sort_asc(sorted, nc);
            const double m = acr_median_sorted(sorted, nc);
            double dev[64];
            for (int j = 0; j < nc; ++j) dev[j] = fabs(cur[j] - m);
            acr_sort_asc(dev, nc);
            const double s = 1.4826 * acr_median_sorted(dev, nc);
            if (s <= 1e-12) break;
            bool changed = false;
            const double lo = static_cast<double>(sigma_low);
            const double hi = static_cast<double>(sigma_high);
            for (int i = 0; i < nv; ++i) {
                if (!acc[i]) continue;
                const double z = (vals[i] - m) / s;
                if (z < lo || z > hi) { acc[i] = false; changed = true; }
            }
            if (!changed) break;
        }
    }

    double wsum = 0.0, vs = 0.0, sup_out = 0.0;
    int used = 0;
    for (int i = 0; i < nv; ++i) {
        if (!acc[i]) continue;
        vs += w[i] * vals[i];
        wsum += w[i];
        if (sup[i] > sup_out) sup_out = sup[i];
        ++used;
    }
    const int rejected = nv - used;
    output[idx] = (used > 0 && wsum > 0.0)
                      ? static_cast<float>(vs / wsum)
                      : 0.0f;
    if (out_support) out_support[idx] = (used > 0) ? static_cast<float>(sup_out) : 0.0f;
    if (out_reject_count) out_reject_count[idx] = (float)rejected;
    if (out_valid_count) out_valid_count[idx] = (float)nv;
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

void acr_launch_mosaic_reject(const float* frames, const float* support,
                              const float* frame_snr, size_t frame_count,
                              size_t pixel_count, size_t begin, size_t n,
                              float sigma_low, float sigma_high,
                              int max_iterations, int min_samples,
                              float* output, float* out_support,
                              float* out_reject_count, float* out_valid_count,
                              cudaStream_t stream) {
    acr_mosaic_reject_kernel<<<grid_size(n), kThreads, 0, stream>>>(
        frames, support, frame_snr, frame_count, pixel_count, begin, n,
        sigma_low, sigma_high, max_iterations, min_samples, output,
        out_support, out_reject_count, out_valid_count);
}

} // extern "C"

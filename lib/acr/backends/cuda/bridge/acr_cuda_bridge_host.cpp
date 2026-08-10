// lib/acr/backends/cuda/bridge/acr_cuda_bridge_host.cpp — 桥接 host 实现
//
// 由 nvcc（MSVC host）编译：extern "C" ABI + 句柄/mutex + cuda 内存管理，
// kernel 实现在 acr_cuda_bridge_kernels.cu。
#include "acr_cuda_bridge.h"

#include <cuda_runtime.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

extern "C" {
void acr_launch_axpy(float* y, const float* x, float alpha,
                     size_t begin, size_t n, cudaStream_t stream);
void acr_launch_copy(float* y, const float* x,
                     size_t begin, size_t n, cudaStream_t stream);
void acr_launch_reduce(const float* x, double* partials,
                       size_t begin, size_t n,
                       size_t chunk_index, size_t blocks_per_chunk,
                       cudaStream_t stream);
void acr_launch_conv3x3(float* y, const float* x,
                        size_t begin, size_t n,
                        size_t width, size_t height,
                        const float* k, cudaStream_t stream);
// 聚焦版（08 号计划 §3）：目标合成内核 launch
void acr_launch_dense_accumulate_fp64acc(float* y, const float* x,
                                         size_t begin, size_t n,
                                         cudaStream_t stream);
void acr_launch_drizzle_scatter(const float* x, double* partials,
                                size_t begin, size_t n, size_t bins,
                                cudaStream_t stream);
void acr_launch_chain(float* y, float* z, const float* x,
                      size_t begin, size_t n, cudaStream_t stream);
void acr_launch_empty(size_t begin, size_t n, cudaStream_t stream);
void acr_launch_weighted_integration(const float* frames,
                                     const float* weights,
                                     size_t frame_count,
                                     size_t pixel_count,
                                     size_t begin, size_t n,
                                     float* output,
                                     cudaStream_t stream);
// Phase2 mosaic_reject launch
void acr_launch_mosaic_reject(const float* frames, const float* support,
                              const float* frame_snr, size_t frame_count,
                              size_t pixel_count, size_t begin, size_t n,
                              float sigma_low, float sigma_high,
                              int max_iterations, int min_samples,
                              float* output, cudaStream_t stream);
}

namespace {

thread_local std::string tls_error;
const char* set_error(cudaError_t err) {
    tls_error = cudaGetErrorString(err);
    return tls_error.c_str();
}
const char* set_error_msg(const char* msg) {
    tls_error = msg;
    return tls_error.c_str();
}

struct CudaExecutorHandle {
    int device{0};
    cudaStream_t stream{nullptr};
    // ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §5）：每 GPU 一个 executor，
    // 内部 1..3 个 stream 通道；streams[0] 与 stream 同指针（向后兼容）。
    cudaStream_t streams[3]{nullptr, nullptr, nullptr};
    int stream_count{1};
    std::size_t next_stream{0};
    std::mutex mtx;
    // 25 号计划 §2.1：每个设备缓冲区独立容量记账，
    // 禁止 d_x/d_y 或 d_partials/d_kernel 共享一个计数器
    float* d_x{nullptr};
    size_t d_x_capacity{0};
    float* d_y{nullptr};
    size_t d_y_capacity{0};
    double* d_partials{nullptr};
    size_t d_partials_capacity{0};
    float* d_kernel{nullptr};
    size_t d_kernel_capacity{0};
    float* d_image{nullptr};   // 卷积整图输入（独立缓冲）
    size_t d_image_capacity{0};
    // 聚焦版：chain 中间值 + drizzle 输出桶（独立容量）
    float* d_z{nullptr};
    size_t d_z_capacity{0};
    double* d_bins{nullptr};
    size_t d_bins_capacity{0};
    // 加权积分：weights（slot 1 persistent）与输出
    float* d_w{nullptr};
    size_t d_w_capacity{0};
    float* d_out{nullptr};
    size_t d_out_capacity{0};
    // persistent 槽位真实上传次数（slot 0/1；验收 resident-reuse）
    uint64_t upload_count[2]{0, 0};
    // 纯传输暂存（H2D/D2H 测量）
    void* d_staging{nullptr};
    size_t d_staging_bytes{0};
};

constexpr int kReduceBlocks = 256;

cudaError_t ensure_buffer(float** buf, size_t& current, size_t needed) {
    if (*buf != nullptr && current >= needed) return cudaSuccess;
    if (*buf != nullptr) cudaFree(*buf);
    *buf = nullptr;
    current = 0;
    cudaError_t err = cudaMalloc(buf, needed * sizeof(float));
    if (err == cudaSuccess) current = needed;
    return err;
}

cudaError_t ensure_buffer(double** buf, size_t& current, size_t needed) {
    if (*buf != nullptr && current >= needed) return cudaSuccess;
    if (*buf != nullptr) cudaFree(*buf);
    *buf = nullptr;
    current = 0;
    cudaError_t err = cudaMalloc(buf, needed * sizeof(double));
    if (err == cudaSuccess) current = needed;
    return err;
}

uint64_t measure_elapsed_ns(cudaEvent_t start, cudaEvent_t end) {
    float ms = 0.0f;
    cudaEventElapsedTime(&ms, start, end);
    return static_cast<uint64_t>(ms * 1e6);
}

// 同步提交公共流程
template <class F>
int submit_impl(CudaExecutorHandle* h, F&& body,
                uint64_t* elapsed_ns, const char** last_error) {
    cudaError_t err = cudaSetDevice(h->device);
    if (err != cudaSuccess) goto fail;
    {
        cudaEvent_t ev_start, ev_end;
        cudaEventCreate(&ev_start);
        cudaEventCreate(&ev_end);
        // ACR 架构冻结：stream 轮转（内部通道共享 GPU 队列/预算，非多设备）
        if (h->stream_count > 0) {
            h->stream = h->streams[
                h->next_stream % static_cast<std::size_t>(h->stream_count)];
            ++h->next_stream;
        }
        cudaEventRecord(ev_start, h->stream);
        err = body();
        cudaEventRecord(ev_end, h->stream);
        cudaEventSynchronize(ev_end);
        if (err == cudaSuccess && elapsed_ns) {
            *elapsed_ns = measure_elapsed_ns(ev_start, ev_end);
        }
        cudaEventDestroy(ev_start);
        cudaEventDestroy(ev_end);
    }
    if (err != cudaSuccess) goto fail;
    if (last_error) *last_error = nullptr;
    return 0;
fail:
    if (last_error) *last_error = set_error(err);
    return 1;
}

} // anonymous namespace

// ===== 设备探测 =====
extern "C" int acr_cuda_bridge_init(const char** last_error) {
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return 0;
    }
    if (last_error) *last_error = nullptr;
    return count;
}

extern "C" int acr_cuda_bridge_device_count(void) {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess) return 0;
    return count;
}

extern "C" const char* acr_cuda_bridge_device_name(int device) {
    thread_local std::string name;
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, device) != cudaSuccess) {
        name = "unknown";
        return name.c_str();
    }
    name = prop.name;
    return name.c_str();
}

extern "C" int acr_cuda_device_memory(int device, uint64_t* total_bytes,
                                      uint64_t* free_bytes,
                                      const char** last_error) {
    cudaError_t err = cudaSetDevice(device);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return 1;
    }
    size_t total = 0, free = 0;
    err = cudaMemGetInfo(&free, &total);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return 1;
    }
    if (total_bytes) *total_bytes = total;
    if (free_bytes) *free_bytes = free;
    if (last_error) *last_error = nullptr;
    return 0;
}

extern "C" int acr_cuda_device_compute(int device, int* sm_count,
                                       int* cc_major, int* cc_minor,
                                       const char** last_error) {
    cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return 1;
    }
    if (sm_count) *sm_count = prop.multiProcessorCount;
    if (cc_major) *cc_major = prop.major;
    if (cc_minor) *cc_minor = prop.minor;
    if (last_error) *last_error = nullptr;
    return 0;
}

// ===== Executor =====
extern "C" void* acr_cuda_executor_create(int device, size_t /*rec*/,
                                          size_t /*min*/, const char** last_error) {
    cudaError_t err = cudaSetDevice(device);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return nullptr;
    }
    auto* h = new CudaExecutorHandle();
    h->device = device;
    err = cudaStreamCreate(&h->stream);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        delete h;
        return nullptr;
    }
    h->streams[0] = h->stream;
    h->stream_count = 1;
    if (last_error) *last_error = nullptr;
    return h;
}

extern "C" void acr_cuda_executor_destroy(void* handle) {
    if (handle == nullptr) return;
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    if (h->d_x) cudaFree(h->d_x);
    if (h->d_y) cudaFree(h->d_y);
    if (h->d_partials) cudaFree(h->d_partials);
    if (h->d_kernel) cudaFree(h->d_kernel);
    if (h->d_image) cudaFree(h->d_image);
    if (h->d_z) cudaFree(h->d_z);
    if (h->d_bins) cudaFree(h->d_bins);
    if (h->d_w) cudaFree(h->d_w);
    if (h->d_out) cudaFree(h->d_out);
    if (h->d_staging) cudaFree(h->d_staging);
    for (int i = 0; i < h->stream_count; ++i) {
        if (h->streams[i]) cudaStreamDestroy(h->streams[i]);
    }
    h->stream = nullptr;
    delete h;
}

extern "C" int acr_cuda_executor_available(void* handle) {
    if (handle == nullptr) return 0;
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    return (cudaSetDevice(h->device) == cudaSuccess) ? 1 : 0;
}

extern "C" int acr_cuda_executor_sync(void* handle, const char** last_error) {
    if (handle == nullptr) {
        if (last_error) *last_error = set_error_msg("null handle");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    cudaError_t err = cudaSuccess;
    for (int i = 0; i < h->stream_count; ++i) {
        if (h->streams[i]) {
            cudaError_t e = cudaStreamSynchronize(h->streams[i]);
            if (e != cudaSuccess && err == cudaSuccess) err = e;
        }
    }
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return 1;
    }
    if (last_error) *last_error = nullptr;
    return 0;
}

// ===== AXPY =====
extern "C" int acr_cuda_executor_submit_axpy(void* handle,
                                             size_t begin, size_t end,
                                             float* y, const float* x,
                                             float alpha,
                                             uint64_t* elapsed_ns,
                                             const char** last_error) {
    if (handle == nullptr || y == nullptr || x == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->d_y_capacity, n);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemcpyAsync(h->d_y, y + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_axpy(h->d_y, h->d_x, alpha, 0, n, h->stream);
        cudaMemcpyAsync(y + begin, h->d_y, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== COPY =====
extern "C" int acr_cuda_executor_submit_copy(void* handle,
                                             size_t begin, size_t end,
                                             float* y, const float* x,
                                             uint64_t* elapsed_ns,
                                             const char** last_error) {
    if (handle == nullptr || y == nullptr || x == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->d_y_capacity, n);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_copy(h->d_y, h->d_x, 0, n, h->stream);
        cudaMemcpyAsync(y + begin, h->d_y, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== REDUCE =====
extern "C" int acr_cuda_executor_submit_reduce(void* handle,
                                               size_t begin, size_t end,
                                               const float* x,
                                               double* partials,
                                               size_t blocks_per_chunk,  // 槽位跨度（≥实际块数）
                                               uint64_t chunk_index,
                                               uint64_t* elapsed_ns,
                                               const char** last_error) {
    if (handle == nullptr || x == nullptr || partials == nullptr ||
        begin >= end || blocks_per_chunk == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    // 25 号计划：grid 块数 = ceil(n / 256)（覆盖整个 chunk，不再固定 256）
    const size_t blocks = (n + 255) / 256;
    if (blocks == 0 || blocks_per_chunk < blocks) {
        if (last_error) *last_error = set_error_msg("reduce span too small");
        return 1;
    }
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_partials, h->d_partials_capacity, blocks);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemsetAsync(h->d_partials, 0, blocks * sizeof(double), h->stream);
        acr_launch_reduce(h->d_x, h->d_partials, 0, n,
                          static_cast<size_t>(chunk_index), blocks_per_chunk, h->stream);
        cudaMemcpyAsync(partials + chunk_index * blocks_per_chunk, h->d_partials,
                        blocks * sizeof(double),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== 3x3 卷积 =====
extern "C" int acr_cuda_executor_submit_conv3x3(void* handle,
                                                size_t begin, size_t end,
                                                float* y, const float* x,
                                                size_t width, size_t height,
                                                const float* kernel9,
                                                uint64_t* elapsed_ns,
                                                const char** last_error) {
    if (handle == nullptr || y == nullptr || x == nullptr ||
        begin >= end || width == 0 || height == 0 || kernel9 == nullptr) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    const size_t image = width * height;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_image, h->d_image_capacity, image);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->d_y_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_kernel, h->d_kernel_capacity, 9);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_image, x, image * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemcpyAsync(h->d_kernel, kernel9, 9 * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_conv3x3(h->d_y, h->d_image, begin, n, width, height,
                           h->d_kernel, h->stream);
        cudaMemcpyAsync(y + begin, h->d_y, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== 聚焦版（08 号计划 §3）：目标合成 Operation =====

// Dense pixel accumulate（FP32 输入 + FP64 累加器）
extern "C" int acr_cuda_executor_submit_dense_accumulate_fp64acc(
    void* handle, size_t begin, size_t end,
    float* y, const float* x,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || y == nullptr || x == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->d_y_capacity, n);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemcpyAsync(h->d_y, y + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_dense_accumulate_fp64acc(h->d_y, h->d_x, 0, n, h->stream);
        cudaMemcpyAsync(y + begin, h->d_y, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// Drizzle-like scatter/accumulate（FP64 原子累计）
extern "C" int acr_cuda_executor_submit_drizzle_scatter(
    void* handle, size_t begin, size_t end,
    const float* x, double* partials, size_t bins,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || x == nullptr || partials == nullptr ||
        begin >= end || bins == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_bins, h->d_bins_capacity, bins);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemsetAsync(h->d_bins, 0, bins * sizeof(double), h->stream);
        acr_launch_drizzle_scatter(h->d_x, h->d_bins, 0, n, bins, h->stream);
        cudaMemcpyAsync(partials, h->d_bins, bins * sizeof(double),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// Resident chain：一次上传、两个 kernel、一次下载
extern "C" int acr_cuda_executor_submit_chain(
    void* handle, size_t begin, size_t end,
    float* z, const float* x,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || z == nullptr || x == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->d_y_capacity, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_z, h->d_z_capacity, n);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_chain(h->d_y, h->d_z, h->d_x, 0, n, h->stream);
        cudaMemcpyAsync(z + begin, h->d_z, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// Launch/event/sync 固定开销
extern "C" int acr_cuda_executor_submit_launch_event(
    void* handle, size_t begin, size_t end,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        acr_launch_empty(0, n, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// 纯 H2D 传输（host_bytes 字节 → 设备暂存）
extern "C" int acr_cuda_executor_transfer_h2d(
    void* handle, size_t host_bytes, const void* host,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || host == nullptr || host_bytes == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    return submit_impl(h, [&]() -> cudaError_t {
        if (h->d_staging == nullptr || h->d_staging_bytes < host_bytes) {
            if (h->d_staging) cudaFree(h->d_staging);
            h->d_staging = nullptr;
            h->d_staging_bytes = 0;
            cudaError_t err = cudaMalloc(&h->d_staging, host_bytes);
            if (err != cudaSuccess) return err;
            h->d_staging_bytes = host_bytes;
        }
        cudaMemcpyAsync(h->d_staging, host, host_bytes,
                        cudaMemcpyHostToDevice, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// 纯 D2H 传输（设备暂存 → host）
extern "C" int acr_cuda_executor_transfer_d2h(
    void* handle, size_t device_bytes, void* host,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || host == nullptr || device_bytes == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    return submit_impl(h, [&]() -> cudaError_t {
        if (h->d_staging == nullptr || h->d_staging_bytes < device_bytes) {
            if (h->d_staging) cudaFree(h->d_staging);
            h->d_staging = nullptr;
            h->d_staging_bytes = 0;
            cudaError_t err = cudaMalloc(&h->d_staging, device_bytes);
            if (err != cudaSuccess) return err;
            h->d_staging_bytes = device_bytes;
        }
        cudaMemcpyAsync(host, h->d_staging, device_bytes,
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== 聚焦版 v2：resident 持久上传与提交 =====
extern "C" int acr_cuda_executor_upload_persistent(
    void* handle, size_t begin, size_t end,
    const float* x,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || x == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    // 持久上传：整帧 [begin, end) 保存到 d_x[begin..end)，
    // 供后续 resident 提交以 d_x + begin 复用（共享输入只上传一次）
    const int rc = submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, end);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
    if (rc == 0) ++h->upload_count[0];
    return rc;
}

extern "C" int acr_cuda_executor_submit_dense_accumulate_resident(
    void* handle, size_t begin, size_t end,
    float* y,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || y == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_y, h->d_y_capacity, end);
        if (err != cudaSuccess) return err;
        // d_x 已整帧驻留（upload_persistent）；y 初值从 host 传入
        cudaMemcpyAsync(h->d_y + begin, y + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);  // y 初值上传
        acr_launch_dense_accumulate_fp64acc(h->d_y + begin, h->d_x + begin,
                                            0, n, h->stream);
        cudaMemcpyAsync(y + begin, h->d_y + begin, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

extern "C" int acr_cuda_executor_submit_reduce_resident(
    void* handle, size_t begin, size_t end,
    double* partials, size_t blocks_per_chunk, uint64_t chunk_index,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || partials == nullptr || begin >= end ||
        blocks_per_chunk == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    const size_t blocks = (n + 255) / 256;
    if (blocks == 0 || blocks_per_chunk < blocks) {
        if (last_error) *last_error = set_error_msg("reduce span too small");
        return 1;
    }
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_partials, h->d_partials_capacity, blocks);
        if (err != cudaSuccess) return err;
        cudaMemsetAsync(h->d_partials, 0, blocks * sizeof(double), h->stream);
        acr_launch_reduce(h->d_x + begin, h->d_partials, 0, n,
                          static_cast<size_t>(chunk_index), blocks_per_chunk, h->stream);
        cudaMemcpyAsync(partials + chunk_index * blocks_per_chunk, h->d_partials,
                        blocks * sizeof(double),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

extern "C" int acr_cuda_executor_submit_drizzle_scatter_resident(
    void* handle, size_t begin, size_t end,
    double* partials, size_t bins,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || partials == nullptr || begin >= end || bins == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_bins, h->d_bins_capacity, bins);
        if (err != cudaSuccess) return err;
        cudaMemsetAsync(h->d_bins, 0, bins * sizeof(double), h->stream);
        // Mixed 正确性：kernel 必须用全局像素索引 hash（与 CPU launcher 一致）。
        // d_x 是完整驻留缓冲，kernel i = begin + idx 访问 d_x[i] 并 hash(i)。
        acr_launch_drizzle_scatter(h->d_x, h->d_bins, begin, n, bins,
                                   h->stream);
        cudaMemcpyAsync(partials, h->d_bins, bins * sizeof(double),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

extern "C" int acr_cuda_executor_submit_chain_resident(
    void* handle, size_t begin, size_t end,
    float* z,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || z == nullptr || begin >= end) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_y, h->d_y_capacity, end);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_z, h->d_z_capacity, end);
        if (err != cudaSuccess) return err;
        // d_x 已驻留；两个 kernel 全程显存；只下载最终 z
        acr_launch_chain(h->d_y + begin, h->d_z + begin, h->d_x + begin,
                         0, n, h->stream);
        cudaMemcpyAsync(z + begin, h->d_z + begin, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== ACR 架构冻结（07 号计划 C）：加权积分 =====
// 上传并保留到指定 persistent 槽位（slot 0 = d_x；slot 1 = d_w）。
extern "C" int acr_cuda_executor_upload_persistent_slot(
    void* handle, int slot, size_t begin, size_t end,
    const float* x,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || x == nullptr || begin >= end ||
        (slot != 0 && slot != 1)) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    const int rc = submit_impl(h, [&]() -> cudaError_t {
        float** buf = (slot == 0) ? &h->d_x : &h->d_w;
        size_t* cap = (slot == 0) ? &h->d_x_capacity : &h->d_w_capacity;
        cudaError_t err = ensure_buffer(buf, *cap, end);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(*buf, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
    if (rc == 0) ++h->upload_count[slot];
    return rc;
}

// 加权积分 host roundtrip：整帧 H2D + kernel + 输出范围 D2H
extern "C" int acr_cuda_executor_submit_weighted_integration(
    void* handle, size_t begin, size_t end,
    float* output,
    const float* frames, const float* weights,
    size_t frame_count, size_t pixel_count,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || output == nullptr || frames == nullptr ||
        weights == nullptr || begin >= end ||
        frame_count == 0 || pixel_count == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    const size_t total = frame_count * pixel_count;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, total);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_w, h->d_w_capacity, frame_count);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_out, h->d_out_capacity, end);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, frames, total * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemcpyAsync(h->d_w, weights, frame_count * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_weighted_integration(h->d_x, h->d_w,
                                        frame_count, pixel_count,
                                        begin, n, h->d_out, h->stream);
        cudaMemcpyAsync(output + begin, h->d_out, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// Phase2 mosaic_reject：H2D frames/support/frame_snr → kernel → D2H output
extern "C" int acr_cuda_executor_submit_mosaic_reject(
    void* handle, size_t begin, size_t end,
    float* output, const float* frames, const float* support,
    const float* frame_snr, size_t frame_count, size_t pixel_count,
    float sigma_low, float sigma_high, int max_iterations, int min_samples,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || output == nullptr || frames == nullptr ||
        begin >= end ||
        frame_count == 0 || pixel_count == 0 || frame_count > 64) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    const size_t total = frame_count * pixel_count;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->d_x_capacity, total);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_w, h->d_w_capacity, total);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_kernel, h->d_kernel_capacity, frame_count);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_out, h->d_out_capacity, end);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, frames, total * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        if (support != nullptr) {
            err = ensure_buffer(&h->d_w, h->d_w_capacity, total);
            if (err != cudaSuccess) return err;
            cudaMemcpyAsync(h->d_w, support, total * sizeof(float),
                            cudaMemcpyHostToDevice, h->stream);
        }
        if (frame_snr != nullptr) {
            err = ensure_buffer(&h->d_kernel, h->d_kernel_capacity,
                                frame_count);
            if (err != cudaSuccess) return err;
            cudaMemcpyAsync(h->d_kernel, frame_snr,
                            frame_count * sizeof(float),
                            cudaMemcpyHostToDevice, h->stream);
        }
        acr_launch_mosaic_reject(h->d_x,
                                 support ? h->d_w : nullptr,
                                 frame_snr ? h->d_kernel : nullptr,
                                 frame_count, pixel_count,
                                 begin, n, sigma_low, sigma_high,
                                 max_iterations, min_samples,
                                 h->d_out, h->stream);
        cudaMemcpyAsync(output + begin, h->d_out, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// 加权积分 resident：frames/weights 已驻留（slot 0/1），只 launch + D2H
extern "C" int acr_cuda_executor_submit_weighted_integration_resident(
    void* handle, size_t begin, size_t end,
    float* output,
    size_t frame_count, size_t pixel_count,
    uint64_t* elapsed_ns, const char** last_error) {
    if (handle == nullptr || output == nullptr || begin >= end ||
        frame_count == 0 || pixel_count == 0) {
        if (last_error) *last_error = set_error_msg("invalid args");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    const size_t n = end - begin;
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_out, h->d_out_capacity, end);
        if (err != cudaSuccess) return err;
        // d_x（frames）/d_w（weights）已整帧驻留；只 launch + D2H 输出范围
        acr_launch_weighted_integration(h->d_x, h->d_w,
                                        frame_count, pixel_count,
                                        begin, n, h->d_out, h->stream);
        cudaMemcpyAsync(output + begin, h->d_out, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

// ===== ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §5）：GPU 内部通道 =====
// 每 GPU 只有一个 executor；stream 是 executor 内部通道（1..3），
// 共享同一 GPU 队列、显存预算与成本模型。禁止把多个 stream 报告为多张 GPU。
extern "C" int acr_cuda_executor_configure_streams(
    void* handle, int stream_count, const char** last_error) {
    if (handle == nullptr || stream_count < 1 || stream_count > 3) {
        if (last_error) *last_error = set_error_msg("stream_count must be 1..3");
        return 1;
    }
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    cudaError_t err = cudaSetDevice(h->device);
    if (err != cudaSuccess) {
        if (last_error) *last_error = set_error(err);
        return 1;
    }
    for (int i = 1; i < stream_count; ++i) {
        if (h->streams[i] == nullptr) {
            err = cudaStreamCreate(&h->streams[i]);
            if (err != cudaSuccess) {
                if (last_error) *last_error = set_error(err);
                return 1;
            }
        }
    }
    h->stream_count = stream_count;
    h->stream = h->streams[0];
    if (last_error) *last_error = nullptr;
    return 0;
}

extern "C" int acr_cuda_executor_stream_count(void* handle) {
    if (handle == nullptr) return 0;
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    return h->stream_count;
}

extern "C" int acr_cuda_executor_upload_count(void* handle, int slot) {
    if (handle == nullptr || (slot != 0 && slot != 1)) return 0;
    auto* h = static_cast<CudaExecutorHandle*>(handle);
    std::lock_guard<std::mutex> lk(h->mtx);
    return static_cast<int>(h->upload_count[slot]);
}

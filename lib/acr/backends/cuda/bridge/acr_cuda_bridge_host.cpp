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
void acr_launch_reduce(const float* x, float* partials,
                       size_t begin, size_t n,
                       size_t chunk_index, size_t blocks_per_chunk,
                       cudaStream_t stream);
void acr_launch_conv3x3(float* y, const float* x,
                        size_t begin, size_t n,
                        size_t width, size_t height,
                        const float* k, cudaStream_t stream);
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
    std::mutex mtx;
    float* d_x{nullptr};
    float* d_y{nullptr};
    float* d_partials{nullptr};
    float* d_kernel{nullptr};
    size_t scratch_count{0};
    size_t partials_count{0};
    size_t image_count{0};
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
    if (h->stream) cudaStreamDestroy(h->stream);
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
    cudaError_t err = cudaStreamSynchronize(h->stream);
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
        cudaError_t err = ensure_buffer(&h->d_x, h->scratch_count, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->scratch_count, n);
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
        cudaError_t err = ensure_buffer(&h->d_x, h->scratch_count, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->scratch_count, n);
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
                                               float* partials,
                                               size_t blocks_per_chunk,
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
    return submit_impl(h, [&]() -> cudaError_t {
        cudaError_t err = ensure_buffer(&h->d_x, h->scratch_count, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_partials, h->partials_count, blocks_per_chunk);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x + begin, n * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemsetAsync(h->d_partials, 0, blocks_per_chunk * sizeof(float), h->stream);
        acr_launch_reduce(h->d_x, h->d_partials, 0, n,
                          static_cast<size_t>(chunk_index), blocks_per_chunk, h->stream);
        cudaMemcpyAsync(partials + chunk_index * blocks_per_chunk, h->d_partials,
                        blocks_per_chunk * sizeof(float),
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
        cudaError_t err = ensure_buffer(&h->d_x, h->image_count, image);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_y, h->scratch_count, n);
        if (err != cudaSuccess) return err;
        err = ensure_buffer(&h->d_kernel, h->partials_count, 9);
        if (err != cudaSuccess) return err;
        cudaMemcpyAsync(h->d_x, x, image * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        cudaMemcpyAsync(h->d_kernel, kernel9, 9 * sizeof(float),
                        cudaMemcpyHostToDevice, h->stream);
        acr_launch_conv3x3(h->d_y, h->d_x, 0, n, width, height,
                           h->d_kernel, h->stream);
        cudaMemcpyAsync(y + begin, h->d_y, n * sizeof(float),
                        cudaMemcpyDeviceToHost, h->stream);
        return cudaStreamSynchronize(h->stream);
    }, elapsed_ns, last_error);
}

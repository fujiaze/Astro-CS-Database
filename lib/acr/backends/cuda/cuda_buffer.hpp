// lib/acr/backends/cuda/cuda_buffer.hpp — device memory RAII + 事件计时
// Phase D：CudaBuffer<T> + cuda_event。
//
// 设计：
// 1. 整个 header 用 #ifdef ACR_BUILD_CUDA 保护（ADR-009）
// 2. CudaBuffer<T>：cudaMalloc/cudaFree RAII，移动语义，禁止拷贝
// 3. copy_h2d / copy_d2h：异步拷贝 + stream sync（默认 stream）
// 4. cuda_event：CUDA event 计时（record/sync/elapsed_since），供 Qualification 用
// 5. 模板类内联实现，避免模板实例化复杂度
#pragma once

#ifdef ACR_BUILD_CUDA

#include <astro/compute/acr.hpp>  // StatusCode

#include <cstddef>

#include <cuda_runtime.h>

namespace astro::compute::cuda {

// ===== CudaBuffer<T>：device memory RAII =====
template <class T>
class CudaBuffer {
public:
    CudaBuffer() = default;

    // 分配 count 个元素（cudaMalloc）。分配失败时 valid()=false，不抛异常。
    explicit CudaBuffer(std::size_t count) : count_(count) {
        if (count > 0) {
            cudaError_t err = cudaMalloc(&data_, count * sizeof(T));
            if (err != cudaSuccess) {
                data_ = nullptr;
                count_ = 0;
            }
        }
    }

    ~CudaBuffer() {
        if (data_ != nullptr) {
            cudaFree(data_);
            data_ = nullptr;
        }
    }

    CudaBuffer(CudaBuffer&& other) noexcept
        : data_(other.data_), count_(other.count_) {
        other.data_ = nullptr;
        other.count_ = 0;
    }

    CudaBuffer& operator=(CudaBuffer&& other) noexcept {
        if (this != &other) {
            if (data_ != nullptr) {
                cudaFree(data_);
            }
            data_ = other.data_;
            count_ = other.count_;
            other.data_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }

    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    std::size_t count() const noexcept { return count_; }
    std::size_t bytes() const noexcept { return count_ * sizeof(T); }
    bool valid() const noexcept { return data_ != nullptr; }

    // 异步 H2D 拷贝 + stream sync
    StatusCode copy_h2d(const T* host, std::size_t n,
                        cudaStream_t stream = nullptr) noexcept {
        if (n == 0) return StatusCode::Ok;
        if (host == nullptr || data_ == nullptr) return StatusCode::InvalidArgument;
        if (n > count_) return StatusCode::OutOfBounds;
        cudaError_t err = cudaMemcpyAsync(data_, host, n * sizeof(T),
                                          cudaMemcpyHostToDevice, stream);
        if (err != cudaSuccess) return cuda_error_to_status(err);
        if (stream != nullptr) {
            err = cudaStreamSynchronize(stream);
        } else {
            err = cudaDeviceSynchronize();
        }
        return cuda_error_to_status(err);
    }

    // 异步 D2H 拷贝 + stream sync
    StatusCode copy_d2h(T* host, std::size_t n,
                        cudaStream_t stream = nullptr) noexcept {
        if (n == 0) return StatusCode::Ok;
        if (host == nullptr || data_ == nullptr) return StatusCode::InvalidArgument;
        if (n > count_) return StatusCode::OutOfBounds;
        cudaError_t err = cudaMemcpyAsync(host, data_, n * sizeof(T),
                                          cudaMemcpyDeviceToHost, stream);
        if (err != cudaSuccess) return cuda_error_to_status(err);
        if (stream != nullptr) {
            err = cudaStreamSynchronize(stream);
        } else {
            err = cudaDeviceSynchronize();
        }
        return cuda_error_to_status(err);
    }

private:
    T* data_{nullptr};
    std::size_t count_{0};
};

// ===== cuda_event：CUDA event 计时 =====
// 供 Qualification 微基准测量 kernel 耗时（record → kernel → record → sync → elapsed）。
class cuda_event {
public:
    cuda_event() {
        cudaEventCreate(&ev_);
    }
    ~cuda_event() {
        if (ev_ != nullptr) {
            cudaEventDestroy(ev_);
            ev_ = nullptr;
        }
    }
    cuda_event(const cuda_event&) = delete;
    cuda_event& operator=(const cuda_event&) = delete;
    cuda_event(cuda_event&& other) noexcept : ev_(other.ev_) {
        other.ev_ = nullptr;
    }
    cuda_event& operator=(cuda_event&& other) noexcept {
        if (this != &other) {
            if (ev_ != nullptr) cudaEventDestroy(ev_);
            ev_ = other.ev_;
            other.ev_ = nullptr;
        }
        return *this;
    }

    void record(cudaStream_t stream = nullptr) noexcept {
        cudaEventRecord(ev_, stream);
    }
    void sync() noexcept {
        cudaEventSynchronize(ev_);
    }
    // 两个 event 之间的耗时（ms）：*this 较后，start 较前
    float elapsed_since(const cuda_event& start) const noexcept {
        float ms = 0.0f;
        cudaEventElapsedTime(&ms, start.ev_, ev_);
        return ms;
    }
    cudaEvent_t raw() const noexcept { return ev_; }

private:
    cudaEvent_t ev_{nullptr};
};

// ===== 非模板辅助函数（cuda_buffer.cpp 提供）=====
// 查询设备 free/total memory（供 CudaBuffer 分配前预估）
StatusCode query_device_memory(std::size_t& free, std::size_t& total) noexcept;

} // namespace astro::compute::cuda

#endif // ACR_BUILD_CUDA

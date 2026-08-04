// lib/acr/backends/cuda/bridge/cuda_bridge_api.hpp — MinGW 侧桥接 API
//
// 由 cuda_bridge_loader.cpp 填充函数指针（LoadLibrary/GetProcAddress），
// 由 classic_kernels.cpp 的 CUDA launcher 调用。纯 C 函数指针，无 ABI 冲突。
#pragma once

#include <cstddef>
#include <cstdint>

namespace astro::compute::cuda::bridge {

struct BridgeApi {
    int (*init)(const char**){nullptr};
    int (*device_count)(){nullptr};
    const char* (*device_name)(int){nullptr};
    void* (*executor_create)(int, std::size_t, std::size_t, const char**){nullptr};
    void (*executor_destroy)(void*){nullptr};
    int (*executor_available)(void*){nullptr};
    int (*executor_sync)(void*, const char**){nullptr};
    int (*submit_axpy)(void*, std::size_t, std::size_t,
                       float*, const float*, float,
                       std::uint64_t*, const char**){nullptr};
    int (*submit_copy)(void*, std::size_t, std::size_t,
                       float*, const float*,
                       std::uint64_t*, const char**){nullptr};
    int (*submit_reduce)(void*, std::size_t, std::size_t,
                         const float*, float*, std::size_t,
                         std::uint64_t, std::uint64_t*, const char**){nullptr};
    int (*submit_conv3x3)(void*, std::size_t, std::size_t,
                          float*, const float*, std::size_t, std::size_t,
                          const float*, std::uint64_t*, const char**){nullptr};

    bool loaded() const noexcept { return init != nullptr; }
};

// 全局桥接 API（loader 填充；未加载时 loaded()==false）
BridgeApi& api() noexcept;

// executor 提交时的线程本地句柄/耗时（launcher 与 executor 之间传递）
void set_tls_handle(void* handle) noexcept;
void* get_tls_handle() noexcept;
void set_tls_elapsed(std::uint64_t ns) noexcept;
std::uint64_t get_tls_elapsed() noexcept;

} // namespace astro::compute::cuda::bridge

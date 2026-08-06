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
    int (*device_memory)(int, std::uint64_t*, std::uint64_t*, const char**){nullptr};
    int (*device_compute)(int, int*, int*, int*, const char**){nullptr};
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
                         const float*, double*, std::size_t,
                         std::uint64_t, std::uint64_t*, const char**){nullptr};
    int (*submit_conv3x3)(void*, std::size_t, std::size_t,
                          float*, const float*, std::size_t, std::size_t,
                          const float*, std::uint64_t*, const char**){nullptr};
    // 聚焦版（08 号计划 §3）：目标合成 Operation
    int (*submit_dense_accumulate_fp64acc)(void*, std::size_t, std::size_t,
                                           float*, const float*,
                                           std::uint64_t*, const char**){nullptr};
    int (*submit_drizzle_scatter)(void*, std::size_t, std::size_t,
                                  const float*, double*, std::size_t,
                                  std::uint64_t*, const char**){nullptr};
    int (*submit_chain)(void*, std::size_t, std::size_t,
                        float*, const float*,
                        std::uint64_t*, const char**){nullptr};
    int (*submit_launch_event)(void*, std::size_t, std::size_t,
                               std::uint64_t*, const char**){nullptr};
    int (*transfer_h2d)(void*, std::size_t, const void*,
                        std::uint64_t*, const char**){nullptr};
    int (*transfer_d2h)(void*, std::size_t, void*,
                        std::uint64_t*, const char**){nullptr};
    // 聚焦版 v2：resident 持久上传与提交
    int (*upload_persistent)(void*, std::size_t, std::size_t,
                             const float*, std::uint64_t*, const char**){nullptr};
    int (*submit_dense_accumulate_resident)(void*, std::size_t, std::size_t,
                                            float*, std::uint64_t*, const char**){nullptr};
    int (*submit_reduce_resident)(void*, std::size_t, std::size_t,
                                  double*, std::size_t, std::uint64_t,
                                  std::uint64_t*, const char**){nullptr};
    int (*submit_drizzle_scatter_resident)(void*, std::size_t, std::size_t,
                                           double*, std::size_t,
                                           std::uint64_t*, const char**){nullptr};
    int (*submit_chain_resident)(void*, std::size_t, std::size_t,
                                 float*, std::uint64_t*, const char**){nullptr};

    bool loaded() const noexcept { return init != nullptr; }
};

// 全局桥接 API（loader 填充；未加载时 loaded()==false）
BridgeApi& api() noexcept;

// 触发一次桥接 DLL 加载（LoadLibrary + GetProcAddress；幂等）。
// 任何需要 GPU 的组件（benchmark/executor 注册）在使用 api() 前应调用。
void ensure_bridge_loaded();

// executor 提交时的线程本地句柄/耗时（launcher 与 executor 之间传递）
void set_tls_handle(void* handle) noexcept;
void* get_tls_handle() noexcept;
void set_tls_elapsed(std::uint64_t ns) noexcept;
std::uint64_t get_tls_elapsed() noexcept;

} // namespace astro::compute::cuda::bridge

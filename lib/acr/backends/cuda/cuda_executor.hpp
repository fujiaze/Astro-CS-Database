// lib/acr/backends/cuda/cuda_executor.hpp — CUDA 设备执行器（F-fix 8）
//
// 实现真实 GPU kernel 提交，让 Dispatcher 能把工作块真正派发到 GPU。
// 满足 22_FIX_REVIEW_CORRECTION_PLAN §F-fix 8：
// "至少一个真实GPU完成部分工作块；每块恰好一次；实际设备统计由completion event生成"
//
// 设计：
// 1. CudaExecutor 继承 scheduler::DeviceExecutor
// 2. submit 在 GPU 上执行 axpy kernel（y=a*x+y），用 token 的 begin/end 作为范围
// 3. 通过 cuda_parallel_for 真实启动 <<<>>> kernel
// 4. submit 是同步的（kernel 执行完才返回），sync 对齐 stream
// 5. available 由 CudaBackend::available 决定（无设备时 false，调用者回退 CPU）
#pragma once

#ifdef ACR_BUILD_CUDA

#include "scheduler/device_executor.hpp"
#include "scheduler/shared_work_pool.hpp"  // WorkToken

#include "cuda_backend.hpp"   // CudaBackend, axpy
#include "cuda_buffer.hpp"     // CudaBuffer

#include <atomic>
#include <cstddef>
#include <string>

namespace astro::compute::cuda {

// ===== CudaExecutor：真实 GPU 执行器 =====
class CudaExecutor : public scheduler::DeviceExecutor {
public:
    explicit CudaExecutor(int device_id = 0,
                          std::size_t recommended_chunk = 65536,
                          std::size_t min_chunk = 256);
    ~CudaExecutor() override;

    // DeviceExecutor 接口
    std::string device_id() const override { return device_id_str_; }
    std::string backend_type() const override { return "cuda"; }
    bool available() const override;
    scheduler::QueueState queue_state() const override;
    std::size_t recommended_chunk() const override { return recommended_chunk_; }
    std::size_t min_effective_chunk() const override { return min_chunk_; }

    scheduler::SubmitResult submit(const scheduler::WorkToken& token,
                                     const scheduler::KernelInvocation& invocation) override;
    void sync() override;

    std::string name() const override;

private:
    int device_id_;
    std::string device_id_str_;
    bool available_{false};
    std::size_t recommended_chunk_;
    std::size_t min_chunk_;
    std::atomic<std::size_t> pending_count_{0};
    CudaBuffer<float> d_buffer_;   // GPU 工作缓冲区（axpy 的 y/x）
    CudaBuffer<float> d_x_buffer_; // GPU x 缓冲区
    cudaStream_t stream_{nullptr};
    bool initialized_{false};

    StatusCode ensure_initialized();
};

// 工厂函数：检测 CUDA 设备并追加 CudaExecutor 到 registry
// 供 ExecutorRegistry::create_auto 调用（ACR_BUILD_CUDA=ON 时链接）
// 无设备时返回而不追加（调用者继续使用 CPU executor）
void append_cuda_executors(scheduler::ExecutorRegistry& registry);

} // namespace astro::compute::cuda

#endif // ACR_BUILD_CUDA

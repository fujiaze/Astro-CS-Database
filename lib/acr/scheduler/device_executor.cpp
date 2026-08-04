// lib/acr/scheduler/device_executor.cpp — 设备执行器实现（F-fix 6）
//
// CpuExecutor：直接调用 kernel function 执行工作块
// ExecutorRegistry：管理设备列表
#include "device_executor.hpp"

#include <atomic>
#include <chrono>
#include <memory>

// F-fix 8: forward declare append_cuda_executors（实现在 backends/cuda/cuda_executor.cpp）
// 必须在 astro::compute::cuda 命名空间，与定义一致；不可声明在 scheduler 命名空间，
// 否则链接器找不到符号（LNK2019）。
#ifdef ACR_BUILD_CUDA
namespace astro::compute::cuda {
void append_cuda_executors(scheduler::ExecutorRegistry& registry);
} // namespace astro::compute::cuda
#endif

namespace astro::compute::scheduler {

// ===== CpuExecutor =====
CpuExecutor::CpuExecutor(const std::string& id,
                         std::size_t recommended_chunk,
                         std::size_t min_chunk)
    : id_(id)
    , recommended_chunk_(recommended_chunk)
    , min_chunk_(min_chunk) {}

QueueState CpuExecutor::queue_state() const {
    QueueState qs;
    qs.depth = pending_count_.load(std::memory_order_relaxed);
    qs.load = (qs.depth > 0) ? 1.0 : 0.0;
    qs.busy = (qs.depth > 0);
    return qs;
}

SubmitResult CpuExecutor::submit(const WorkToken& token,
                                   const KernelInvocation& invocation) {
    SubmitResult result;
    if (!token.valid() || !invocation.fn) {
        result.status = SubmitStatus::Rejected;
        result.error = "invalid token or null kernel";
        return result;
    }
    if (!available_) {
        result.status = SubmitStatus::Rejected;
        result.error = "cpu executor not available";
        return result;
    }

    pending_count_.fetch_add(1, std::memory_order_relaxed);

    auto start = std::chrono::high_resolution_clock::now();
    try {
        invocation.fn(token.id, token.begin, token.end, invocation.user_data);
        result.status = SubmitStatus::Ok;
        result.items_done = token.size();
    } catch (...) {
        result.status = SubmitStatus::Failed;
        result.error = "kernel exception";
    }
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - start).count();

    pending_count_.fetch_sub(1, std::memory_order_relaxed);
    return result;
}

// ===== ExecutorRegistry =====
ExecutorRegistry::ExecutorRegistry() = default;

void ExecutorRegistry::register_executor(std::unique_ptr<DeviceExecutor> exec) {
    if (exec) {
        executors_.push_back(std::move(exec));
    }
}

std::vector<DeviceExecutor*> ExecutorRegistry::available_executors() const {
    std::vector<DeviceExecutor*> result;
    for (const auto& exec : executors_) {
        if (exec && exec->available()) {
            result.push_back(exec.get());
        }
    }
    return result;
}

DeviceExecutor* ExecutorRegistry::find(const std::string& device_id) const {
    for (const auto& exec : executors_) {
        if (exec && exec->device_id() == device_id) {
            return exec.get();
        }
    }
    return nullptr;
}

ExecutorRegistry ExecutorRegistry::create_cpu_only() {
    ExecutorRegistry registry;
    registry.register_executor(std::make_unique<CpuExecutor>("cpu", 65536, 256));
    return registry;
}

ExecutorRegistry ExecutorRegistry::create_auto() {
    ExecutorRegistry registry;
    registry.register_executor(std::make_unique<CpuExecutor>("cpu", 65536, 256));
#ifdef ACR_BUILD_CUDA
    // F-fix 8: ACR_BUILD_CUDA=ON 时追加 CudaExecutor（真实 GPU 执行器）
    // 无 CUDA 设备时 append_cuda_executors 内部跳过，调用者继续使用 CPU executor
    // 注意：append_cuda_executors 定义在 astro::compute::cuda 命名空间，
    // 必须用全限定名声明，否则会被解析到当前 scheduler 命名空间（链接器找不到符号）
    astro::compute::cuda::append_cuda_executors(registry);
#endif
    return registry;
}

} // namespace astro::compute::scheduler

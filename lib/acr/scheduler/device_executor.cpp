// lib/acr/scheduler/device_executor.cpp — 设备执行器实现（23 号计划 §3）
//
// CpuExecutor：通过 KernelRegistry 的 CPU launcher 执行 KernelInvocation，
// SubmitHandle 记录真实 device/items/bytes/duration。
#include "device_executor.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "../cost/cost_estimator.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace astro::compute::scheduler {

// CUDA 桥接执行器追加函数（23 号计划 §3）。
// 默认 weak no-op：CPU-only 构建无 CUDA 桥接；
// 启用 ACR_CUDA_BRIDGE 时由 backends/cuda/cuda_bridge_loader.cpp 提供强定义。
#if defined(_MSC_VER)
// MSVC 无 __attribute__((weak))：ASan 独立构建只编译本文件（不含 loader），
// 此处给出普通空定义；MinGW 构建仍用 weak（允许 loader 强定义覆盖）。
void try_append_cuda_bridge_executors(ExecutorRegistry&) {}
#else
__attribute__((weak)) void try_append_cuda_bridge_executors(ExecutorRegistry&) {}
#endif

namespace {

// 从 invocation traits 估算单元素字节数（真实完成字节报告）
inline std::size_t bytes_per_item(const KernelInvocation& inv) noexcept {
    return inv.traits.bytes_read_per_item + inv.traits.bytes_written_per_item;
}

} // anonymous namespace

// ===== CpuExecutor =====
CpuExecutor::CpuExecutor(const std::string& id,
                         std::size_t recommended_chunk,
                         std::size_t min_chunk,
                         const KernelRegistry* registry)
    : id_str_(id)
    , recommended_chunk_(recommended_chunk)
    , min_chunk_(min_chunk)
    , registry_(registry) {
    id_ = (id == "cpu") ? kHwCpuDeviceId : cost::backend_to_device_id(id);
    if (id_ == kHwInvalidDeviceId) id_ = kHwCpuDeviceId;
}

const KernelRegistry* CpuExecutor::registry() const {
    return registry_ ? registry_ : &global_kernel_registry();
}

bool CpuExecutor::supports(OperationId op) const {
    return registry()->supports(op, "cpu");
}

QueueState CpuExecutor::queue_state() const {
    QueueState qs;
    qs.depth = pending_count_.load(std::memory_order_relaxed);
    qs.load = (qs.depth > 0) ? 1.0 : 0.0;
    qs.busy = (qs.depth > 0);
    return qs;
}

SubmitHandle CpuExecutor::submit(const WorkToken& token,
                                 const KernelInvocation& invocation) {
    SubmitHandle result;
    result.device = id_;
    result.op_id = std::string(invocation.id);
    result.attempt = token.attempt;

    if (!token.valid()) {
        result.status = SubmitStatus::Rejected;
        result.error = "invalid token";
        return result;
    }
    if (!available_) {
        result.status = SubmitStatus::Rejected;
        result.error = "cpu executor not available";
        return result;
    }
    const KernelRegistration* reg = registry()->find(invocation.id);
    if (reg == nullptr || reg->cpu == nullptr) {
        // 设备 launcher 缺失：拒绝（调用方必须回退并如实报告）
        result.status = SubmitStatus::Rejected;
        result.error = "operation not registered for cpu: " + std::string(invocation.id);
        return result;
    }
    // 24 号计划 §5.2：提交前统一契约校验（buffer/scalar/domain/numeric）
    const std::string contract_err = validate_invocation(*reg, invocation, "cpu");
    if (!contract_err.empty()) {
        result.status = SubmitStatus::Rejected;
        result.error = "invocation contract violation: " + contract_err;
        return result;
    }

    pending_count_.fetch_add(1, std::memory_order_relaxed);
    const auto start = std::chrono::high_resolution_clock::now();
    try {
        reg->cpu(invocation, nullptr);
        result.status = SubmitStatus::Ok;
        result.items_done = token.size();
        result.bytes_done = token.size() * bytes_per_item(invocation);
    } catch (const std::exception& e) {
        result.status = SubmitStatus::Failed;
        result.error = std::string("cpu kernel exception: ") + e.what();
    } catch (...) {
        result.status = SubmitStatus::Failed;
        result.error = "cpu kernel unknown exception";
    }
    const auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
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
    registry.register_executor(
        std::make_unique<CpuExecutor>("cpu", 65536, 256));
    return registry;
}

ExecutorRegistry ExecutorRegistry::create_auto() {
    ExecutorRegistry registry;
    registry.register_executor(
        std::make_unique<CpuExecutor>("cpu", 65536, 256));
    // 23 号计划 §3：GPU 不可用时不创建 executor（运行时探测，不得仅凭编译宏）。
    // CUDA 桥接加载由 backends/cuda/cuda_bridge_loader 实现：
    //   - 探测 acr_cuda_bridge.dll（MSVC+nvcc 构建）与真实设备；
    //   - 无 DLL / 无设备 → 不注册 CudaExecutor，继续使用 CPU executor。
    try_append_cuda_bridge_executors(registry);
    return registry;
}

} // namespace astro::compute::scheduler

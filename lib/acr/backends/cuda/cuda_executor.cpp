// lib/acr/backends/cuda/cuda_executor.cpp — CudaExecutor 实现（F-fix 8）
//
// 真实 GPU kernel 执行：submit 通过 cuda_parallel_for 启动 axpy kernel，
// 证明 GPU 真实完成工作块（非占位回退）。
#ifdef ACR_BUILD_CUDA

#include "cuda_executor.hpp"

#include <chrono>
#include <cstring>
#include <vector>

namespace astro::compute::cuda {

CudaExecutor::CudaExecutor(int device_id,
                           std::size_t recommended_chunk,
                           std::size_t min_chunk,
                           const KernelRegistry* registry)
    : device_id_(device_id)
    , device_id_str_("cuda:" + std::to_string(device_id))
    , recommended_chunk_(recommended_chunk)
    , min_chunk_(min_chunk)
    , registry_(registry) {
    ensure_initialized();
}

const KernelRegistry* CudaExecutor::registry() const {
    return registry_ ? registry_ : &global_kernel_registry();
}

CudaExecutor::~CudaExecutor() {
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }
}

StatusCode CudaExecutor::ensure_initialized() {
    if (initialized_) return StatusCode::Ok;

    // 初始化 CudaBackend（幂等，std::call_once 保护）
    StatusCode s = CudaBackend::instance().initialize();
    if (s != StatusCode::Ok || !CudaBackend::instance().available()) {
        available_ = false;
        return s;
    }

    // 创建独立 stream（不与 CudaBackend 默认 stream 冲突）
    cudaError_t err = cudaStreamCreate(&stream_);
    if (err != cudaSuccess) {
        available_ = false;
        return cuda_error_to_status(err);
    }

    // 预分配 GPU 工作缓冲区（recommended_chunk 个 float）
    d_buffer_ = CudaBuffer<float>(recommended_chunk_);
    d_x_buffer_ = CudaBuffer<float>(recommended_chunk_);
    if (!d_buffer_.valid() || !d_x_buffer_.valid()) {
        available_ = false;
        return StatusCode::OutOfMemory;
    }

    // 初始化 x 缓冲区为 1.0（y = a*x + y，x=1 时 y += a）
    std::vector<float> ones(recommended_chunk_, 1.0f);
    StatusCode hs = d_x_buffer_.copy_h2d(ones.data(), recommended_chunk_, stream_);
    if (hs != StatusCode::Ok) {
        available_ = false;
        return hs;
    }

    available_ = true;
    initialized_ = true;
    return StatusCode::Ok;
}

bool CudaExecutor::available() const {
    return available_;
}

bool CudaExecutor::supports(astro::compute::OperationId op) const {
    return registry()->supports(op, "cuda");
}

scheduler::QueueState CudaExecutor::queue_state() const {
    scheduler::QueueState qs;
    qs.depth = pending_count_.load(std::memory_order_relaxed);
    qs.load = (qs.depth > 0) ? 1.0 : 0.0;
    qs.busy = (qs.depth > 0);
    return qs;
}

scheduler::SubmitHandle CudaExecutor::submit(const scheduler::WorkToken& token,
                                               const astro::compute::KernelInvocation& invocation) {
    scheduler::SubmitHandle result;
    result.device = static_cast<astro::compute::DeviceId>(device_id_ + 1);
    result.op_id = std::string(invocation.id);
    result.attempt = token.attempt;

    if (!token.valid()) {
        result.status = scheduler::SubmitStatus::Rejected;
        result.error = "invalid token";
        return result;
    }
    if (!available_) {
        result.status = scheduler::SubmitStatus::Rejected;
        result.error = "cuda executor not available";
        return result;
    }

    const KernelRegistration* reg = registry()->find(invocation.id);
    if (reg == nullptr || !reg->cuda.has_value()) {
        // 设备 launcher 缺失：拒绝（调用方必须回退并如实报告）
        result.status = scheduler::SubmitStatus::Rejected;
        result.error = "operation not registered for cuda: " + std::string(invocation.id);
        return result;
    }
    // 24 §5.2：提交前统一契约校验
    const std::string contract_err =
        validate_invocation(*reg, invocation, "cuda");
    if (!contract_err.empty()) {
        result.status = scheduler::SubmitStatus::Rejected;
        result.error = "invocation contract violation: " + contract_err;
        return result;
    }

    pending_count_.fetch_add(1, std::memory_order_relaxed);
    const auto start = std::chrono::high_resolution_clock::now();
    try {
        // 真实 GPU kernel 执行：注册的 cuda launcher（含显存传输与 kernel 启动）
        (*reg->cuda)(invocation, nullptr);
        result.status = scheduler::SubmitStatus::Ok;
        result.items_done = token.size();
        result.bytes_done =
            token.size() * (invocation.traits.bytes_read_per_item +
                            invocation.traits.bytes_written_per_item);
    } catch (const std::exception& e) {
        result.status = scheduler::SubmitStatus::Failed;
        result.error = std::string("cuda kernel exception: ") + e.what();
    } catch (...) {
        result.status = scheduler::SubmitStatus::Failed;
        result.error = "cuda kernel unknown exception";
    }
    const auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    pending_count_.fetch_sub(1, std::memory_order_relaxed);
    return result;
}

void CudaExecutor::sync() {
    if (stream_ != nullptr) {
        cudaStreamSynchronize(stream_);
    }
}

std::string CudaExecutor::name() const {
    if (CudaBackend::instance().available()) {
        return CudaBackend::instance().device_info().name;
    }
    return device_id_str_;
}

// ===== 工厂函数：追加 CudaExecutor 到 registry =====
void append_cuda_executors(scheduler::ExecutorRegistry& registry) {
    // 初始化 CudaBackend 检测设备
    StatusCode s = CudaBackend::instance().initialize();
    if (s != StatusCode::Ok || !CudaBackend::instance().available()) {
        // 无 CUDA 设备：不追加，调用者继续使用 CPU executor
        return;
    }

    int dev_count = CudaBackend::instance().device_count();
    // 为每个设备创建一个 CudaExecutor
    // 当前实现只创建 device 0（多 GPU 扩展预留）
    for (int i = 0; i < dev_count && i < 1; ++i) {
        auto exec = std::make_unique<CudaExecutor>(i, 65536, 256);
        if (exec->available()) {
            registry.register_executor(std::move(exec));
        }
    }
}

} // namespace astro::compute::cuda

#endif // ACR_BUILD_CUDA

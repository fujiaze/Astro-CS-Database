// lib/acr/scheduler/device_executor.hpp — 统一设备执行器接口（F-fix 6）
//
// F-fix 6: 建立真实 DeviceExecutor，让 CPU 和每个 GPU 各自成为真实 worker。
//
// 设计（控制包 22_FIX_REVIEW_CORRECTION_PLAN §F-fix 6）：
//   1. 抽象接口不泄漏第三方类型（CUDA/HIP/SYCL 不可见于公共头）
//   2. CPU executor：使用 oneTBB/parallel_batch 执行
//   3. CUDA executor：真实 kernel 提交、event 完成、错误回传
//   4. Dispatcher 不直接假定 cuda:0，实际设备 ID 来自 executor
//   5. 每个 executor 有独立的队列状态和推荐块大小
//
// 核心接口：
//   - DeviceExecutor::submit(token, invocation) 提交工作块到设备
//   - DeviceExecutor::queue_state() 报告队列深度和负载
//   - DeviceExecutor::available() 设备是否可用
#pragma once

#include "shared_work_pool.hpp"  // WorkToken

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace astro::compute::scheduler {

// ===== KernelInvocation：工作块执行回调 =====
// 封装 chunk kernel function + user_data，跨设备通用
struct KernelInvocation {
    // kernel 回调签名：(block_id, begin, end, user_data)
    using KernelFn = void(*)(std::size_t, std::size_t, std::size_t, void*);
    KernelFn fn{nullptr};
    void* user_data{nullptr};
};

// ===== QueueState：设备队列状态 =====
struct QueueState {
    std::size_t depth{0};           // 当前队列深度（待完成块数）
    double load{0.0};               // 负载比例 [0, 1]
    bool busy{false};                // 是否繁忙
};

// ===== SubmitResult：提交结果 =====
enum class SubmitStatus : std::uint8_t {
    Ok = 0,           // 成功提交并执行
    Queued = 1,       // 已入队（异步设备）
    Failed = 2,       // 执行失败
    Rejected = 3,     // 设备拒绝（不可用/队列满）
};

struct SubmitResult {
    SubmitStatus status{SubmitStatus::Failed};
    std::string error;               // 失败原因
    std::uint64_t elapsed_ns{0};     // 执行耗时（纳秒）
    std::size_t items_done{0};       // 完成的 item 数
};

// ===== DeviceExecutor：抽象设备执行器 =====
class DeviceExecutor {
public:
    virtual ~DeviceExecutor() = default;

    // 设备标识
    virtual std::string device_id() const = 0;
    // 设备类型："cpu" / "cuda" / ...
    virtual std::string backend_type() const = 0;
    // 设备是否可用
    virtual bool available() const = 0;
    // 队列状态
    virtual QueueState queue_state() const = 0;
    // 推荐块大小（由 CostEstimator 或硬件画像驱动）
    virtual std::size_t recommended_chunk() const = 0;
    // 最小有效块大小
    virtual std::size_t min_effective_chunk() const = 0;

    // 提交工作块执行
    // 同步设备（CPU）：直接执行并返回结果
    // 异步设备（GPU）：提交到队列，后续通过 event/sync 获取结果
    virtual SubmitResult submit(const WorkToken& token,
                                 const KernelInvocation& invocation) = 0;

    // 等待所有已提交工作完成（异步设备用）
    virtual void sync() {}

    // 设备名称（诊断用）
    virtual std::string name() const { return device_id(); }
};

// ===== CpuExecutor：CPU 执行器 =====
// 使用 parallel_batch 或直接调用 kernel function 执行工作块
class CpuExecutor : public DeviceExecutor {
public:
    CpuExecutor(const std::string& id = "cpu",
                std::size_t recommended_chunk = 65536,
                std::size_t min_chunk = 256);

    std::string device_id() const override { return id_; }
    std::string backend_type() const override { return "cpu"; }
    bool available() const override { return available_; }
    QueueState queue_state() const override;
    std::size_t recommended_chunk() const override { return recommended_chunk_; }
    std::size_t min_effective_chunk() const override { return min_chunk_; }

    SubmitResult submit(const WorkToken& token,
                         const KernelInvocation& invocation) override;

    void set_available(bool avail) { available_ = avail; }
    void set_recommended_chunk(std::size_t chunk) { recommended_chunk_ = chunk; }

private:
    std::string id_;
    bool available_{true};
    std::size_t recommended_chunk_;
    std::size_t min_chunk_;
    std::atomic<std::size_t> pending_count_{0};
};

// ===== ExecutorRegistry：设备执行器注册表 =====
// 管理所有可用的 DeviceExecutor 实例
class ExecutorRegistry {
public:
    ExecutorRegistry();

    // 注册执行器
    void register_executor(std::unique_ptr<DeviceExecutor> exec);

    // 获取所有可用执行器
    std::vector<DeviceExecutor*> available_executors() const;

    // 按 device_id 查找
    DeviceExecutor* find(const std::string& device_id) const;

    // 所有执行器数量
    std::size_t size() const { return executors_.size(); }

    // 默认 CPU-only 注册
    static ExecutorRegistry create_cpu_only();

    // 自动检测（CUDA 可用时注册 GPU executor）
    static ExecutorRegistry create_auto();

private:
    std::vector<std::unique_ptr<DeviceExecutor>> executors_;
};

} // namespace astro::compute::scheduler

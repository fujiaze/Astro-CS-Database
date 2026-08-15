// lib/acr/scheduler/device_executor.hpp — 真实设备执行器接口（23 §3）
//
// 设计（23_SECOND_FIX_REVIEW_CORRECTION_PLAN.md §3 + 07_COST_MODEL §10）：
// 1. 抽象接口不泄漏第三方类型（CUDA/HIP/SYCL 不可见于公共头）；
// 2. CPU executor：oneTBB/ISA，通过 KernelRegistry 的 CPU launcher 执行；
// 3. CUDA executor：真实 kernel 提交、stream/event、错误回传；
// 4. Dispatcher 只把 invocation 交给 supports(OperationId) 为 true 的 executor；
// 5. 每个 executor 有独立队列状态、推荐块大小（来自自身 DeviceCost）；
// 6. SubmitHandle 记录真实 device ID、items、bytes、duration、fallback ——
// actual 统计只能由 executor completion 产生，不从推荐值伪造。
#pragma once

#include "astro/compute/hardware_profile.hpp"
#include "astro/compute/kernel_registry.hpp"
#include "shared_work_pool.hpp"  // WorkToken

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== QueueState：设备队列状态 =====
struct QueueState {
    std::size_t depth{0};           // 当前队列深度（待完成块数）
    double load{0.0};               // 负载比例 [0, 1]
    bool busy{false};               // 是否繁忙
};

// ===== 提交结果状态 =====
enum class SubmitStatus : std::uint8_t {
    Ok = 0,           // 成功执行
    Failed = 1,       // 执行失败（错误已回传）
    Rejected = 2,     // 设备拒绝（不可用/op 不支持/参数无效）
};

// ===== SubmitHandle：一次真实提交的完成记录 =====
// actual 统计来源：executor 真实执行后回填，禁止用预测值填充。
struct SubmitHandle {
    SubmitStatus status{SubmitStatus::Failed};
    std::string error;               // 失败原因
    DeviceId device{kHwInvalidDeviceId};   // 真实执行设备
    std::string op_id;               // 执行的 OperationId
    std::size_t items_done{0};       // 真实完成元素数
    std::size_t bytes_done{0};       // 真实完成字节数（buffer 绑定累计）
    std::uint64_t elapsed_ns{0};     // 真实耗时（含传输+计算+同步）
    bool fallback{false};            // 是否因设备不可用/失败回退执行
    std::uint32_t attempt{0};        // token 的 attempt（诊断）
};

// ===== DeviceExecutor：抽象设备执行器 =====
class DeviceExecutor {
public:
    virtual ~DeviceExecutor() = default;

    // 设备标识（整数 DeviceId：0=CPU，1..=GPU 0..N-1）
    virtual DeviceId id() const = 0;
    // 设备类型："cpu" / "cuda" / "hip" / ...
    virtual std::string backend_type() const = 0;
    // 设备是否可用（运行时探测，不得仅凭编译宏判断）
    virtual bool available() const = 0;
    // 是否支持该 OperationId（通过 KernelRegistry 查询）
    virtual bool supports(OperationId op) const = 0;
    // 队列状态
    virtual QueueState queue_state() const = 0;
    // 推荐块大小（由 CostEstimator/硬件画像驱动，每设备独立）
    virtual std::size_t recommended_chunk() const = 0;
    // 最小有效块大小
    virtual std::size_t min_effective_chunk() const = 0;

    // 聚焦版 v3（08 §3）：真实驻留执行接口。
    // prefetch_input：上传 host 输入到设备并保留（真实一次传输）；
    // input_resident：查询该 host 输入是否已在设备显存。
    // 默认返回 false（无真实 device buffer 缓存的 executor 不支持）。
    virtual bool prefetch_input(const void* /*host*/, std::size_t /*bytes*/) {
        return false;
    }
    virtual bool input_resident(const void* /*host*/) const { return false; }
    // ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §3）：一次预取多个输入
    // （如加权积分的 frames + weights）。默认逐输入调用 prefetch_input；
    // 需要组合上传的 executor（如 CUDA 桥接整帧+权重一次上传）可重写。
    virtual bool prefetch_inputs(
        const std::vector<const void*>& hosts,
        const std::vector<std::size_t>& bytes) {
        if (hosts.size() != bytes.size()) return false;
        bool all_ok = true;
        for (std::size_t i = 0; i < hosts.size(); ++i) {
            if (!prefetch_input(hosts[i], bytes[i])) all_ok = false;
        }
        return all_ok;
    }

    // ACR 基座收尾（02_GENERATION_COHERENCE.md）：同 host 指针原地修改 +
    // generation++ 时，强制本 executor 的驻留视图失效。默认 no-op；
    // CUDA 桥接 executor 必须真实清理 host 驻留映射/slot 状态，使下一次
    // prefetch 强制真实上传（不得跳过 H2D 使用旧 device 数据）。
    virtual void invalidate_input(const void* /*host*/) {}

    // ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §5）：每 GPU 只有一个 executor，
    // 内部可容纳的 in-flight 槽位数（stream 数）。CPU 默认 1（多 worker 由
    // Dispatcher 管理）；CUDA executor 返回其内部 stream 槽位数。
    virtual std::size_t max_in_flight() const { return 1; }
    // 配置 executor 内部 stream 通道数（GPU 专用；默认返回 false=不支持）。
    virtual bool set_streams(std::size_t /*count*/) { return false; }
    // persistent 槽位真实上传次数（slot 0 = frames/d_x，slot 1 = weights/d_w）。
    // resident-reuse 验收：同一帧栈多次调用时 frames 上传必须保持 1。
    virtual std::uint64_t slot_upload_count(int /*slot*/) const { return 0; }

    // 提交一个工作块执行。
    // 同步设备（CPU）：直接执行并返回真实完成统计；
    // 异步设备（GPU）：提交到队列，通过 event/sync 获取真实结果。
    virtual SubmitHandle submit(const WorkToken& token,
                                const KernelInvocation& invocation) = 0;

    // 等待所有已提交工作完成（异步设备用）
    virtual void sync() {}

    // 设备名称（诊断用）
    virtual std::string name() const { return device_id(); }
    // 规范字符串标识："cpu" / "cuda:0"（诊断/报告用）
    virtual std::string device_id() const = 0;
};

// ===== CpuExecutor：CPU 执行器（KernelRegistry CPU launcher）=====
class CpuExecutor : public DeviceExecutor {
public:
    explicit CpuExecutor(const std::string& id = "cpu",
                         std::size_t recommended_chunk = 65536,
                         std::size_t min_chunk = 256,
                         const KernelRegistry* registry = nullptr);

    DeviceId id() const override { return id_; }
    std::string device_id() const override { return id_str_; }
    std::string backend_type() const override { return "cpu"; }
    bool available() const override { return available_; }
    bool supports(OperationId op) const override;
    QueueState queue_state() const override;
    std::size_t recommended_chunk() const override { return recommended_chunk_; }
    std::size_t min_effective_chunk() const override { return min_chunk_; }

    SubmitHandle submit(const WorkToken& token,
                        const KernelInvocation& invocation) override;

    void set_available(bool avail) { available_ = avail; }
    void set_recommended_chunk(std::size_t chunk) { recommended_chunk_ = chunk; }

private:
    const KernelRegistry* registry() const;

    DeviceId id_{kHwCpuDeviceId};
    std::string id_str_;
    bool available_{true};
    std::size_t recommended_chunk_;
    std::size_t min_chunk_;
    const KernelRegistry* registry_{nullptr};
    std::atomic<std::size_t> pending_count_{0};
};

// ===== ExecutorRegistry：设备执行器注册表 =====
class ExecutorRegistry {
public:
    ExecutorRegistry();

    void register_executor(std::unique_ptr<DeviceExecutor> exec);
    std::vector<DeviceExecutor*> available_executors() const;
    DeviceExecutor* find(const std::string& device_id) const;
    std::size_t size() const { return executors_.size(); }

    // 默认 CPU-only 注册（真实执行，非占位）
    static ExecutorRegistry create_cpu_only();
    // 自动检测：CUDA 桥接可用时注册 GPU executor（运行时探测）
    static ExecutorRegistry create_auto();

private:
    std::vector<std::unique_ptr<DeviceExecutor>> executors_;
};

} // namespace astro::compute::scheduler

// lib/acr/scheduler/dispatcher.hpp — CPU+GPU 混合调度器
// Phase F：工作保持调度（work-conserving dispatcher）。
//
// 设计（控制包 07_WORK_CONSERVING_DISPATCHER_SPEC.md）：
//   1. 不重叠 chunk：用 CoverageBitmap 保证完整不重复
//   2. 首选设备忙时使用空闲合格设备：工作保持
//   3. 失败回退：设备失败时，未执行的 chunk 回退到 CPU，已执行的不重放
//   4. 数据驻留成本：考虑传输成本，小数据优先 CPU
//   5. Dispatcher 是 MixedRunner + QueueAwareEstimator + FallbackPolicy 的组合
//   6. Phase F3 增强：新增 cost-aware 调度方法（接受 CostEstimate + CurrentState）
//   7. 公共头不暴露第三方类型
//   8. Commit F：接入 CpuController（95% 软目标）+ MemoryBudgetController（RAM/VRAM 预算）
//      + guided 尾部收缩（completion > 70% 时 chunk_size 动态缩小）
#pragma once

#include "current_state.hpp"
#include "fallback.hpp"
#include "mixed_runner.hpp"
#include "partitioner.hpp"
#include "queue_aware.hpp"
#include "reduction_merger.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute {
struct TaskDescriptor;
namespace cost { struct CostEstimate; }
namespace utilization { class CpuController; class MemoryBudgetController; }
}

namespace astro::compute::scheduler {

// ===== Dispatcher 配置 =====
struct DispatcherConfig {
    std::vector<DeviceState> devices;            // 可用设备列表
    FallbackStrategy fallback_strategy{FallbackStrategy::ToCpu};
    // 小数据阈值：bytes 总数小于此值时优先 CPU
    std::size_t small_data_threshold_bytes{1u << 20};  // 1 MB
    // Commit F：utilization + guided 配置
    double cpu_target_ratio{0.95};       // CPU 利用率软目标（95%）
    bool enable_utilization{true};        // 是否启用 utilization 反压
    bool enable_guided_tail{true};        // 是否启用 guided 尾部收缩
    double guided_tail_threshold{0.7};   // completion_ratio > 此值时开始收缩
    std::size_t min_effective_chunk{256}; // guided 收缩下限
};

// ===== Cost-aware 分发结果（Phase F3）=====
struct CostAwareResult {
    MixedRunResult run_result;          // 复用 MixedRunResult 统计
    std::string actual_primary_backend; // 实际执行主力 backend
    std::size_t total_chunks{0};
    std::size_t chunks_on_cpu{0};
    std::size_t chunks_on_gpu{0};
    std::size_t chunks_fallback{0};
    bool used_cost_estimator{false};    // 是否使用了 CostEstimator
    std::string current_state_json;     // 最终 CurrentState 快照
    // Commit F：utilization + guided 报告
    double cpu_actual_ratio{0.0};       // 最后一次采样的 CPU 实际利用率
    bool cpu_actual_valid{false};       // actual_ratio 是否有效
    std::string mem_action;            // 内存预算建议动作（none/shrink/stop/fail）
    bool guided_tail_used{false};       // 是否触发了 guided 尾部收缩
    std::size_t guided_min_chunk{0};   // guided 收缩到的最小块
};

// ===== Dispatcher =====
// 工作保持调度器：整合 MixedRunner + QueueAware + FallbackPolicy
// Phase F3 增强：新增 cost-aware 方法（接受 CostEstimate + CurrentState）
class Dispatcher {
public:
    Dispatcher();
    ~Dispatcher();

    void configure(const DispatcherConfig& cfg);

    // 分发 range 任务（自动拆分 + 调度）
    MixedRunResult dispatch_range(std::size_t begin, std::size_t end,
                                  std::size_t chunk_size,
                                  ChunkKernelFn fn, void* user_data);

    // 分发预拆分的 chunks
    MixedRunResult dispatch_chunks(const std::vector<RangeChunk>& chunks,
                                   ChunkKernelFn fn, void* user_data);

    // 路由决策：根据 task 估算选最优 backend
    // 工作保持：首选设备忙时使用空闲合格设备
    std::string pick_backend(const TaskEstimate& task) const;

    // 当设备失败时的回退决策
    FallbackDecision handle_failure(const std::string& failed_backend,
                                    const CoverageBitmap& bitmap) const;

    // ===== Phase F3：Cost-aware 工作保持调度 =====
    // 根据 CostEstimate 决定 chunk_size 和 backend 分配。
    // 无 GPU 或画像不可用时退化为纯 CPU（与 dispatch_range 等价）。
    // task: 任务描述（提供 work_size/precision/traits）
    // estimate: CostEstimator 的估算结果（提供 per_device 成本和推荐块）
    // fn: per-chunk kernel（参数：chunk_idx, begin, end, user_data）
    // user_data: 传递给 fn 的用户数据
    CostAwareResult dispatch_range_cost_aware(
        const TaskDescriptor& task,
        const cost::CostEstimate& estimate,
        ChunkKernelFn fn, void* user_data);

    // ===== CurrentState 访问（cost-aware 调度后可用）=====
    const CurrentState& current_state() const noexcept;
    CurrentState& current_state() noexcept;

    // 内部组件访问（用于测试）
    const MixedRunner& runner() const noexcept;
    const QueueAwareEstimator& estimator() const noexcept;
    const FallbackPolicy& fallback_policy() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::scheduler

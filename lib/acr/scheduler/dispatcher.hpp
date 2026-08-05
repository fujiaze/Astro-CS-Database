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
#include "device_executor.hpp"
#include "fallback.hpp"
#include "mixed_runner.hpp"
#include "partitioner.hpp"
#include "queue_aware.hpp"
#include "reduction_merger.hpp"
#include "../utilization/memory_budget.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute {
struct TaskDescriptor;
namespace cost { struct CostEstimate; }
namespace utilization {
class CpuController;
class MemoryBudgetController;
struct MemoryBudgetConfig;
struct CpuControlDecision;
struct MemoryBudget;
}
}

namespace astro::compute::scheduler {

// ===== Dispatcher 配置 =====
struct DispatcherConfig {
    std::vector<DeviceState> devices;            // 可用设备列表
    FallbackStrategy fallback_strategy{FallbackStrategy::ToCpu};
    // 小数据阈值：bytes 总数小于此值时优先 CPU
    std::size_t small_data_threshold_bytes{1u << 20};  // 1 MB
    // Commit F-fix 4：utilization 配置
    double cpu_target_ratio{0.95};       // CPU 利用率软目标（95%）
    bool enable_utilization{true};        // 是否启用 utilization 反压
    // F-fix 1：固定尾段实验（审计要求改名为 fixed_tail_chunking）
    // 注意：这不是动态 guided scheduling，仅是固定比例尾段缩块实验
    bool enable_fixed_tail_chunking{false};     // 默认关闭（审计要求）
    double fixed_tail_threshold{0.7};           // 固定阈值（仅实验用）
    std::size_t min_effective_chunk{256};       // 缩块下限
    // 23 号计划 §5：CPU 控制时间窗（100-500ms；0=使用默认 200ms）
    std::uint32_t control_window_ms{0};
    // 23 号计划 §4：invocation 路径 CPU executor 的 worker 数（0=auto min(8,hw)）
    std::size_t invocation_cpu_workers{0};
    // 24 号计划 §2：生产调度按 Eligible Device Set 筛选（feasible/最小有效规模/收益）。
    // 测试专用：置 true 时绕过筛选（仅用于 Mixed 调度验证，生产必须保持 false）。
    bool force_all_supported_executors{false};
    // 25 号计划 §7：正式 MemoryBudget 配置注入（RAM/VRAM 容量上限）。
    // 显式提供时由 configure 注入；未提供时使用默认配置（ram/vram 0.95）。
    utilization::MemoryBudgetConfig memory_budget{};
    bool memory_budget_explicit{false};
    // F-fix 6 + F-fix 7：设备执行器注册表（可选）
    // 如果提供且包含多个可用 executor，Dispatcher 会通过 execute_via_executors 执行：
    //   - 每个空闲 executor 按自身推荐块大小领取工作块
    //   - 设备忙时不等待，其他空闲设备继续领取
    //   - actual_primary_backend 从真实完成统计生成
    // 为空或仅 CPU executor 时退化为旧路径（向后兼容）
    // 禁止用户提供 CPU/GPU 比例：分配由 executor.available() + claim_next_dynamic 决定
    std::shared_ptr<ExecutorRegistry> executors;

    // ===== 23 号计划 §5：采样注入缝隙（测试/诊断用）=====
    // 生产路径为 null，使用真实控制器（CpuController::sample_and_decide /
    // MemoryBudgetController::sample）。测试可注入确定性的采样序列，
    // 驱动 gate close/recover 与内存动作，验证闭环行为。
    std::function<utilization::CpuControlDecision()> cpu_sampler_override;
    std::function<utilization::MemoryBudget()> memory_sampler_override;
};

// ===== Coverage 统计（从真实执行事件生成）=====
struct CoverageStats {
    std::size_t total{0};
    std::size_t pending{0};
    std::size_t claimed{0};
    std::size_t done{0};
    std::size_t failed{0};
};

// ===== F-fix 4: 资源闭环控制统计 =====
// 记录执行过程中的 CPU 利用率采样序列、内存预算动作序列和控制动作统计。
// 用于生成 50/80/95/100% 持续负载报告（验收：不能用人工样本代替）。
struct ResourceControlStats {
    // ---- CPU 利用率采样序列 ----
    std::vector<double> cpu_actual_samples;       // 每次采样的 actual_ratio
    std::vector<std::uint64_t> cpu_sample_ts_ns;  // 采样时间戳(ns)
    double cpu_target{0.0};                       // 目标利用率
    bool cpu_valid{false};                         // actual 是否有效

    // ---- CPU 控制动作统计 ----
    std::size_t yield_count{0};                    // 错峰让步次数
    std::size_t batch_shrink_count{0};             // 批次缩小次数
    bool submit_gate_triggered{false};             // submit gate 是否触发
    std::vector<std::string> control_actions;      // 已执行控制动作序列（诊断/证据）
    std::uint32_t workers_registered{0};           // 注册参与 worker 数
    std::uint32_t workers_active{0};               // 结束时活跃 worker 数

    // ---- F-fix 9: 可恢复 submit gate 统计 ----
    std::size_t gate_close_count{0};               // gate 关闭次数（含重关闭）
    std::size_t gate_recover_count{0};             // gate 恢复次数
    bool gate_aborted{false};                      // gate 超时放弃剩余工作

    // ---- 内存预算采样序列 ----
    std::vector<std::string> mem_actions;          // 每次采样的动作序列
    std::vector<std::uint64_t> mem_used_ram_samples;  // 每次采样的 used_ram
    std::uint64_t mem_limit_ram{0};                // RAM 限额
    std::string final_mem_action{"none"};         // 最终动作
    // 25 号计划 §7：claim 前内存峰值估算与预算动作原始记录
    // （输入/输出/临时/双缓冲/传输 staging/partial/merge 峰值 + 对应动作）
    std::vector<std::uint64_t> mem_peak_estimates;  // 每次 claim 前估算的峰值字节
    std::vector<std::string> mem_peak_actions;      // 每次 claim 前估算触发的动作
    std::vector<std::string> mem_vram_actions;      // GPU VRAM 预算动作序列
    std::uint64_t mem_peak_max{0};                  // 峰值估算最大值（证据用）

    // ---- F-fix 3: 动态 guided 块大小序列 ----
    // 每次 claim_next_dynamic 返回的块大小，用于验证尾部收缩
    std::vector<std::size_t> dynamic_chunk_sizes;
    bool dynamic_mode_used{false};                // 是否使用了动态 guided 模式
};

// ===== Cost-aware 分发结果（Phase F3 + F-fix 1）=====
struct CostAwareResult {
    MixedRunResult run_result;          // 复用 MixedRunResult 统计
    // F-fix 1：预测与实际分开报告
    std::string predicted_primary_backend;  // CostEstimator 预测的最优设备
    std::string actual_primary_backend;     // 实际执行主力 backend（由真实完成统计生成）
    std::vector<std::string> actual_devices_used;  // 实际参与执行的所有设备
    std::size_t total_chunks{0};
    std::size_t chunks_on_cpu{0};
    std::size_t chunks_on_gpu{0};
    std::size_t chunks_fallback{0};
    bool used_cost_estimator{false};    // 是否使用了 CostEstimator
    std::string current_state_json;     // 最终 CurrentState 快照
    // F-fix 1：coverage 从真实执行导入
    CoverageStats coverage;
    // F-fix 4：utilization + memory 报告（保留兼容字段）
    double cpu_actual_ratio{0.0};       // 最后一次采样的 CPU 实际利用率
    bool cpu_actual_valid{false};       // actual_ratio 是否有效
    std::string mem_action;            // 内存预算建议动作（none/shrink/stop/fail）
    // F-fix 4：完整资源控制统计
    ResourceControlStats resource_control;
    // F-fix 1：固定尾段实验标记（不是动态 guided）
    bool fixed_tail_chunking_used{false};  // 是否使用了固定尾段缩块实验
    std::size_t fixed_tail_min_chunk{0};   // 固定尾段缩到的最小块

    // 24 号计划 §3：每设备真实完成统计（仅由 completion 产生）
    struct PerDeviceStats {
        std::string device_id;              // "cpu" / "cuda:0"
        std::string backend;                // "cpu" / "cuda"
        std::size_t items_done{0};          // 完成元素数
        std::size_t bytes_read{0};          // 实际读取字节
        std::size_t bytes_written{0};       // 实际写入字节
        std::size_t blocks_done{0};         // 完成块数
        std::uint64_t active_duration_ns{0};    // 有效执行时间
        std::uint64_t queue_wait_ns{0};         // 队列等待
        std::uint64_t transfer_duration_ns{0};  // 传输耗时
        std::size_t fallback_count{0};          // 回退次数
        std::size_t error_count{0};             // 错误/失败次数
    };
    std::vector<PerDeviceStats> per_device_stats;
};

// ===== Dispatcher =====
// 工作保持调度器：整合 MixedRunner + QueueAware + FallbackPolicy
// Phase F3 增强：新增 cost-aware 方法（接受 CostEstimate + CurrentState）
class Dispatcher {
public:
    Dispatcher();
    ~Dispatcher();

    void configure(const DispatcherConfig& cfg);

    // 注册缓存释放 hook（MemoryBudget ReleaseCache 动作时调用）
    void set_cache_release_hook(std::function<void()> hook);

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

    // ===== 23 号计划 §3/§4：可加速 KernelInvocation 派发 =====
    // 把 KernelInvocation 交给支持其 OperationId 的 DeviceExecutor；
    // 每个 executor 按自身 CostEstimate（recommended_chunk/队列/剩余工作）
    // 独立计算 requested_items 并动态领取；CPU 与 GPU 可同时领取，
    // 设备忙时不阻塞其他空闲设备；禁止用户提供 CPU/GPU 比例。
    // 无 executor 支持该 op 时如实失败（不伪装 CPU/GPU 执行）。
    // 注意：本路径只接受 KernelRegistry 注册过的可加速 kernel；
    // 旧 lambda/ChunkKernelFn 走 dispatch_range_cost_aware（CPU-only）。
    CostAwareResult dispatch_invocation(
        const TaskDescriptor& task,
        const cost::CostEstimate& estimate,
        const KernelInvocation& invocation);

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

// lib/acr/scheduler/dispatcher.hpp — CPU+GPU 混合调度器
// Phase F：工作保持调度（work-conserving dispatcher）。
//
// 设计（控制包 07_WORK_CONSERVING_DISPATCHER_SPEC.md）：
//   1. 不重叠 chunk：用 CoverageBitmap 保证完整不重复
//   2. 首选设备忙时使用空闲合格设备：工作保持
//   3. 失败回退：设备失败时，未执行的 chunk 回退到 CPU，已执行的不重放
//   4. 数据驻留成本：考虑传输成本，小数据优先 CPU
//   5. Dispatcher 是 MixedRunner + QueueAwareEstimator + FallbackPolicy 的组合
//   6. 公共头不暴露第三方类型
#pragma once

#include "fallback.hpp"
#include "mixed_runner.hpp"
#include "partitioner.hpp"
#include "queue_aware.hpp"
#include "reduction_merger.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== Dispatcher 配置 =====
struct DispatcherConfig {
    std::string preferred_backend{"cpu"};        // 路由首选
    std::vector<DeviceState> devices;            // 可用设备列表
    FallbackStrategy fallback_strategy{FallbackStrategy::ToCpu};
    // 小数据阈值：bytes 总数小于此值时优先 CPU
    std::size_t small_data_threshold_bytes{1u << 20};  // 1 MB
};

// ===== Dispatcher =====
// 工作保持调度器：整合 MixedRunner + QueueAware + FallbackPolicy
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

    // 内部组件访问（用于测试）
    const MixedRunner& runner() const noexcept;
    const QueueAwareEstimator& estimator() const noexcept;
    const FallbackPolicy& fallback_policy() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::scheduler

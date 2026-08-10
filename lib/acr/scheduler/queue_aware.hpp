// lib/acr/scheduler/queue_aware.hpp — 队列感知估算
// Phase F：finish = queue_wait + transfer + compute + merge
//
// 设计：
//   1. 估算设备 finish 时间用于工作保持调度决策
//   2. 数据驻留成本：考虑 H2D/D2H 传输时间，小数据优先 CPU
//   3. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== 设备状态（用于 finish 估算）=====
struct DeviceState {
    std::string backend;             // "cpu" / "cuda:0" / ...
    std::uint64_t queue_load_ns{0};  // 当前队列等待时间（已排队任务预估）
    std::uint64_t last_finish_ns{0}; // 上次任务完成时间戳
    double bandwidth_gbps{0.0};      // GB/s（CPU 为内存带宽，GPU 为 PCIe 带宽）
    bool available{true};
};

// ===== 任务参数（用于估算）=====
struct TaskEstimate {
    std::size_t bytes_per_chunk{0};  // 每个 chunk 传输字节数
    std::size_t chunk_count{0};
    double compute_per_chunk_ns{0.0};  // 每个 chunk 计算时间（纳秒）
    double merge_per_chunk_ns{0.0};    // 每个 chunk 合并时间
};

// ===== QueueAwareEstimator =====
class QueueAwareEstimator {
public:
    QueueAwareEstimator();
    ~QueueAwareEstimator();

    // 估算某个 backend 执行 task 的总 finish 时间（纳秒）
    // finish = queue_wait + transfer + compute + merge
    std::uint64_t estimate_finish(const DeviceState& dev,
                                  const TaskEstimate& task) const noexcept;

    // 选择 finish 时间最短的可用设备（工作保持）
    // 返回 backend 字符串；空字符串表示无可用设备
    std::string pick_best_device(const std::vector<DeviceState>& devices,
                                 const TaskEstimate& task) const;

    // 数据驻留成本估算：传输时间 vs 计算时间
    // 若传输 > 计算的 0.5 倍，建议 CPU（小数据优先 CPU）
    bool should_prefer_cpu(const DeviceState& cpu_dev,
                           const DeviceState& gpu_dev,
                           const TaskEstimate& task) const noexcept;
};

} // namespace astro::compute::scheduler

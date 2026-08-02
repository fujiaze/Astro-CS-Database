// lib/acr/scheduler/queue_aware.cpp — 队列感知估算实现
#include "queue_aware.hpp"

#include <algorithm>
#include <limits>

namespace astro::compute::scheduler {

QueueAwareEstimator::QueueAwareEstimator() = default;
QueueAwareEstimator::~QueueAwareEstimator() = default;

std::uint64_t QueueAwareEstimator::estimate_finish(const DeviceState& dev,
                                                    const TaskEstimate& task) const noexcept {
    if (!dev.available) return std::numeric_limits<std::uint64_t>::max();
    if (task.chunk_count == 0) return dev.queue_load_ns;
    // transfer = bytes_total / bandwidth
    double bytes_total = static_cast<double>(task.bytes_per_chunk * task.chunk_count);
    double transfer_s = 0.0;
    if (dev.bandwidth_gbps > 0.0) {
        transfer_s = (bytes_total / 1e9) / dev.bandwidth_gbps;
    }
    std::uint64_t transfer_ns = static_cast<std::uint64_t>(transfer_s * 1e9);
    // CPU backend 传输为 0（数据已在内存）
    if (dev.backend == "cpu") transfer_ns = 0;
    std::uint64_t compute_ns = static_cast<std::uint64_t>(task.compute_per_chunk_ns * task.chunk_count);
    std::uint64_t merge_ns = static_cast<std::uint64_t>(task.merge_per_chunk_ns * task.chunk_count);
    return dev.queue_load_ns + transfer_ns + compute_ns + merge_ns;
}

std::string QueueAwareEstimator::pick_best_device(const std::vector<DeviceState>& devices,
                                                   const TaskEstimate& task) const {
    std::string best;
    std::uint64_t best_finish = std::numeric_limits<std::uint64_t>::max();
    for (const auto& d : devices) {
        if (!d.available) continue;
        std::uint64_t f = estimate_finish(d, task);
        if (f < best_finish) {
            best_finish = f;
            best = d.backend;
        }
    }
    return best;
}

bool QueueAwareEstimator::should_prefer_cpu(const DeviceState& cpu_dev,
                                            const DeviceState& gpu_dev,
                                            const TaskEstimate& task) const noexcept {
    // 计算数据驻留成本：GPU 传输 vs CPU 计算时间
    if (task.chunk_count == 0) return true;
    double bytes_total = static_cast<double>(task.bytes_per_chunk * task.chunk_count);
    double gpu_transfer_s = 0.0;
    if (gpu_dev.bandwidth_gbps > 0.0) {
        gpu_transfer_s = (bytes_total / 1e9) / gpu_dev.bandwidth_gbps;
    }
    double gpu_transfer_ns = gpu_transfer_s * 1e9;
    double compute_ns = task.compute_per_chunk_ns * task.chunk_count;
    // 若传输 > 计算的 0.5 倍，CPU 更优（小数据优先 CPU）
    return gpu_transfer_ns > compute_ns * 0.5;
}

} // namespace astro::compute::scheduler

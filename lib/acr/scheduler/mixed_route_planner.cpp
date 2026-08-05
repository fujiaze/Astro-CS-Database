// lib/acr/scheduler/mixed_route_planner.cpp — MixedRoutePlanner 实现
#include "mixed_route_planner.hpp"

#include "../qualification/focused/operation_profile.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace astro::compute::scheduler {

struct MixedRoutePlanner::Impl {
    const astro::compute::qualification::focused::OperationProfile* profile{
        nullptr};
};

MixedRoutePlanner::MixedRoutePlanner()
    : impl_(std::make_unique<Impl>()) {}
MixedRoutePlanner::~MixedRoutePlanner() = default;

void MixedRoutePlanner::set_profile(
    const astro::compute::qualification::focused::OperationProfile* p) noexcept {
    impl_->profile = p;
}

const astro::compute::qualification::focused::OperationProfile*
MixedRoutePlanner::profile() const noexcept {
    return impl_->profile;
}

MixedRoutePlan MixedRoutePlanner::plan(const std::string& operation_id,
                                       std::size_t remaining,
                                       bool data_resident) const {
    MixedRoutePlan r;
    if (impl_->profile == nullptr) {
        r.reason = "no-operation-profile";
        return r;
    }
    const auto* op = impl_->profile->find(operation_id);
    if (op == nullptr || !op->qualified) {
        r.reason = "operation-not-qualified";
        return r;
    }
    r.profile_available = true;
    r.cpu_ns_per_item = op->cpu.ns_per_item;
    r.gpu_ns_per_item = op->gpu.ns_per_item;
    r.gpu_fixed_ns = op->gpu.fixed_us * 1000.0 +
                     op->gpu.launch_us * 1000.0;
    r.cpu_chunk_items = op->cpu.recommended_chunk_items;
    r.gpu_chunk_items = op->gpu.recommended_chunk_items;
    // 尾段自动缩块（05 §5）：remaining 较小时候选块按剩余/活跃设备数缩小，
    // 但保持最小高效块。
    if (remaining > 0) {
        if (r.cpu_chunk_items > remaining / 2) {
            r.cpu_chunk_items =
                std::max(op->cpu.minimum_chunk_items, remaining / 2);
        }
        if (r.gpu_chunk_items > remaining / 2) {
            r.gpu_chunk_items =
                std::max(op->gpu.minimum_chunk_items, remaining / 2);
        }
    }
    // host/resident 不同 GPU 阈值（05 §4：resident 阈值更低）
    r.gpu_min_host_items = op->gpu.min_profitable_items_host;
    r.gpu_min_resident_items = op->gpu.min_profitable_items_resident;
    if (data_resident) {
        r.gpu_chunk_items = std::min(
            r.gpu_chunk_items,
            std::max<std::size_t>(op->gpu.minimum_chunk_items,
                                  r.gpu_min_resident_items));
        r.reason = "profile-resident";
    } else {
        r.reason = "profile-host";
    }
    return r;
}

bool MixedRoutePlanner::should_claim(const MixedRoutePlan& plan,
                                     const std::string& device_backend,
                                     std::size_t remaining,
                                     std::size_t queue_depth,
                                     double device_measured_ns_per_item) {
    if (!plan.profile_available || remaining == 0) {
        return plan.profile_available && remaining > 0;
    }
    const bool is_cpu = (device_backend == "cpu");
    // 该设备预计完成"下一块"的耗时
    const std::size_t chunk = is_cpu ? plan.cpu_chunk_items
                                     : plan.gpu_chunk_items;
    if (chunk == 0) return false;
    const double per_item = (device_measured_ns_per_item > 0.0)
        ? device_measured_ns_per_item
        : (is_cpu ? plan.cpu_ns_per_item : plan.gpu_ns_per_item);
    double device_next_ns = per_item * static_cast<double>(chunk);
    if (!is_cpu) {
        device_next_ns += plan.gpu_fixed_ns +
                          static_cast<double>(queue_depth) *
                              plan.gpu_ns_per_item *
                              static_cast<double>(chunk);
    }
    // 最快设备清空剩余工作的时间（用 CPU 每 item 作为保守下限；
    // 实际由两设备速率对比决定）
    const double fastest_ns_per_item =
        std::min(plan.cpu_ns_per_item, plan.gpu_ns_per_item);
    const double fastest_remaining_ns =
        fastest_ns_per_item * static_cast<double>(remaining);
    // 边际收益门：该设备下一块预计完成不晚于最快设备清空剩余（+20% 松弛）
    return device_next_ns <= fastest_remaining_ns * 1.2;
}

} // namespace astro::compute::scheduler

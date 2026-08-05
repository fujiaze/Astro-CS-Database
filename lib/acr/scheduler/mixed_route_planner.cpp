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
                                     double device_measured_ns_per_item,
                                     double other_measured_ns_per_item,
                                     bool device_has_measured_work) {
    if (!plan.profile_available || remaining == 0) {
        return plan.profile_available && remaining > 0;
    }
    // 尚未执行过任何块的设备：允许领取首块以建立真实实测
    // （无实测时 Profile 乐观速率会误判，导致某设备被过早排除）
    if (!device_has_measured_work) return true;
    const bool is_cpu = (device_backend == "cpu");
    // 该设备预计完成"下一块"的耗时
    const std::size_t chunk = is_cpu ? plan.cpu_chunk_items
                                     : plan.gpu_chunk_items;
    if (chunk == 0) return false;
    // 设备速率：本设备必有实测（上面已保证）；
    // 另一设备无实测时用 Profile 值乘以保守系数（dispatch 端到端高于理想速率）
    const double profile_ns =
        is_cpu ? plan.cpu_ns_per_item : plan.gpu_ns_per_item;
    const double other_profile_ns =
        is_cpu ? plan.gpu_ns_per_item : plan.cpu_ns_per_item;
    const double per_item = device_measured_ns_per_item;
    const double other_per_item = (other_measured_ns_per_item > 0.0)
        ? other_measured_ns_per_item : other_profile_ns * 10.0;
    // 两台设备完成剩余工作的总时间（实测/Profile 速率）
    const double fastest_ns_per_item =
        std::min(per_item, other_per_item);
    const double slowest_ns_per_item =
        std::max(per_item, other_per_item);
    const double fast_remaining_ns =
        fastest_ns_per_item * static_cast<double>(remaining);
    const double slow_remaining_ns =
        slowest_ns_per_item * static_cast<double>(remaining);
    // 边际收益门（05 号规范 §4）：慢设备参与会形成尾部时停止其新 claim。
    // 若该设备是最快的（含唯一设备清尾），始终允许继续。
    const bool device_is_fastest =
        (per_item <= other_per_item);
    if (device_is_fastest) return true;
    // 慢设备：仅当速率与最快设备接近（×1.05）时参与 Mixed；
    // 显著慢的设备停止新 claim（08 号计划 §7：只有实测 Mixed 有边际收益
    // 才允许 Mixed，否则 Auto 自然退化为最快设备）
    return slow_remaining_ns <= fast_remaining_ns * 1.05;
}

} // namespace astro::compute::scheduler

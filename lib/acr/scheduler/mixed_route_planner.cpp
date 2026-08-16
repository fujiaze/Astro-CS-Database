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
    r.cpu_fixed_ns = op->cpu.fixed_us * 1000.0;
    r.gpu_ns_per_item = op->gpu.ns_per_item;
    r.gpu_fixed_ns = op->gpu.fixed_us * 1000.0 +
                     op->gpu.launch_us * 1000.0;
    r.cpu_chunk_items = op->cpu.recommended_chunk_items;
    r.gpu_chunk_items = op->gpu.recommended_chunk_items;
    // 尾段自动缩块：块不超过剩余工作，保持最小高效块。
    // 不强制 "剩余一半"（避免 GPU-only 场景把整帧任务拆成多次 kernel，
    // 徒增 launch/D2H 开销）；混合场景由 Dispatcher 的 makespan claim 与
    // 首轮公平门决定设备参与。
    if (remaining > 0) {
        if (r.cpu_chunk_items > remaining) {
            r.cpu_chunk_items = remaining;
        }
        if (r.gpu_chunk_items > remaining) {
            r.gpu_chunk_items = remaining;
        }
    }
    // host/resident 不同 GPU 阈值（05 §4：resident 阈值更低）
    r.gpu_min_host_items = op->gpu.min_profitable_items_host
                               ? op->gpu.min_profitable_items_host.value() : 0;
    r.gpu_min_resident_items = op->gpu.min_profitable_items_resident
                                   ? op->gpu.min_profitable_items_resident.value()
                                   : 0;
    if (data_resident) {
        if (!op->gpu.resident_path_eligible) {
            r.profile_available = false;
            r.reason = "resident-path-not-eligible";
            return r;
        }
        // 收益阈值只决定 GPU 是否参与（前置门），不修改实测推荐块

        r.reason = "profile-resident";
    } else {
        if (!op->gpu.host_path_eligible) {
            r.profile_available = false;
            r.reason = "host-path-not-eligible";
            return r;
        }
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
                                     bool allow_first_block) {
    if (!plan.profile_available || remaining == 0) {
        return plan.profile_available && remaining > 0;
    }
    // ForcedMixed：允许未执行设备领取首块（仅正确性测试）。
    // Auto：禁止“每设备先领一块”，无实测时用保守 Profile。
    const bool has_measured = device_measured_ns_per_item > 0.0;
    if (!has_measured && allow_first_block) return true;
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
    const double per_item = has_measured
        ? device_measured_ns_per_item : profile_ns * 10.0;
    const double other_per_item = (other_measured_ns_per_item > 0.0)
        ? other_measured_ns_per_item : other_profile_ns * 10.0;
    // makespan 模型（08 §2 / ）：
    // 无该设备下一块：makespan0 = 另一设备完成剩余
    // 有该设备下一块：该设备完成 chunk，另一设备完成剩余
    // makespan1 = max(block_ns, other × (remaining - chunk))
    // 若 makespan1 < makespan0 → 允许 claim（缩短总完工时间）。
    // 异速设备（如 GPU 快很多、CPU 处理一小块并提前完成）也能 Mixed。
    const double block_ns =
        per_item * static_cast<double>(chunk) +
        (is_cpu ? plan.cpu_fixed_ns : plan.gpu_fixed_ns);
    const double without_ns =
        other_per_item * static_cast<double>(remaining);
    const double rem_after =
        (remaining > chunk) ? static_cast<double>(remaining - chunk) : 0.0;
    const double other_after_ns = other_per_item * rem_after;
    const double with_ns = std::max(block_ns, other_after_ns);
    if (with_ns < without_ns) return true;
    // 该设备是最快的（含唯一设备清尾）：必须允许它清空剩余
    return per_item <= other_per_item;
}

} // namespace astro::compute::scheduler

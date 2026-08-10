// lib/acr/scheduler/mixed_route_planner.hpp — 聚焦 Mixed 路由规划器
//
// 08 号计划 §5 / 05 号规范：基于 OperationProfile 决定
//   - CPU/GPU 独立块大小；
//   - host 与 resident 输入的不同 GPU 最小收益规模；
//   - 边际收益门：设备只有在预计能缩短总完成时间时才继续 claim；
//   - 尾段停止慢设备新 claim（防止尾部拖累）。
//
// 只使用当前 Operation 的 Profile；无合格 Profile 时安全回退 CPU。
#pragma once

#include "device_executor.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace astro::compute::qualification::focused {
struct OperationProfile;
} // namespace astro::compute::qualification::focused

namespace astro::compute::scheduler {

// ===== 单次规划结果 =====
struct MixedRoutePlan {
    std::size_t cpu_chunk_items{0};      // CPU 推荐块
    std::size_t gpu_chunk_items{0};      // GPU 推荐块
    std::size_t gpu_min_host_items{0};   // host 输入最小 GPU 收益规模
    std::size_t gpu_min_resident_items{0}; // resident 输入最小 GPU 收益规模
    double cpu_ns_per_item{0.0};
    double gpu_ns_per_item{0.0};
    double gpu_fixed_ns{0.0};            // launch + 传输固定开销
    double cpu_fixed_ns{0.0};            // CPU 调度/线程固定开销（worker 预判用）
    bool profile_available{false};       // 是否有该 Operation 的合格 Profile
    std::string reason;                  // 规划依据（诊断）
};

// ===== MixedRoutePlanner =====
// 线程安全：持有只读 OperationProfile 引用（生命周期由调用方保证）。
class MixedRoutePlanner {
public:
    MixedRoutePlanner();
    ~MixedRoutePlanner();

    // 设置当前 OperationProfile（nullptr=无 Profile，走保守 CPU fallback）
    void set_profile(const astro::compute::qualification::focused::
                         OperationProfile* profile) noexcept;
    const astro::compute::qualification::focused::OperationProfile*
    profile() const noexcept;

    // 对指定 operation + 剩余工作 + 数据驻留状态生成规划
    // remaining: 池中剩余未开始工作项
    // data_resident: 输入是否已在目标设备显存（仅影响 GPU 阈值）
    MixedRoutePlan plan(const std::string& operation_id,
                        std::size_t remaining,
                        bool data_resident) const;

    // 尾段门：设备 d 是否应继续 claim。
    // 规则：若设备 d 预计完成"下一块"的时间不早于当前最快设备清空剩余
    // 工作的时间，则停止 d 的新 claim（避免慢设备制造尾部）。
    // return false = 停止该设备本轮 claim。
    static bool should_claim(const MixedRoutePlan& plan,
                             const std::string& device_backend,
                             std::size_t remaining,
                             std::size_t queue_depth,
                             double device_measured_ns_per_item,
                             double other_measured_ns_per_item,
                             bool allow_first_block);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::scheduler

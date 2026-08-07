// lib/acr/routing/benchmark_route_estimator.hpp
//
// ACR Benchmark 驱动路由估计器（06 号规范）：
//   - 只读离线 Profile v2；
//   - 对 RouteRequest 预测 OpenMP / GPU Direct / Mixed 三条候选路径的
//     端到端完工时间；
//   - 二维插值（log2 output_items × frame_count），有效范围检查；
//   - Mixed 用 CPU/GPU chunk 服务曲线模拟共享池动态 claim（无固定份额）；
//   - 叠加队列等待、内存可行性、p95 误差界；
//   - 选择最低 score；无画像/范围外安全回退 OpenMP。
#pragma once

#include "route_profile_v2.hpp"

#include <memory>
#include <string>

namespace astro::compute::routing {

class BenchmarkRouteEstimator {
public:
    BenchmarkRouteEstimator();
    ~BenchmarkRouteEstimator();

    // 设置只读 Profile v2（生命周期由调用方保证；nullptr=无画像）
    void set_profile(const RouteProfileV2* profile) noexcept;
    const RouteProfileV2* profile() const noexcept;

    // 主入口：对一次 RouteRequest 生成三条路径预测并选择最低 score。
    // 无画像/范围外 → chosen=OpenMP，feasible 路径为空并给出 reason。
    RouteDecision decide(const RouteRequest& request) const;

    // ===== 预测子过程（公开便于单测）=====

    // 二维插值：log2(output_items) 分段线性 × frame_count 相邻插值。
    // 有效范围内返回 true；范围外返回 false（不无条件外推）。
    static bool interpolate_e2e(const RoutePath& path,
                                std::uint64_t output_items,
                                std::uint32_t frame_count,
                                double& median_ms,
                                double& p90_ms);

    // 范围检查：items 在 [min,max] 且 frame_count 已测（或可相邻插值）
    static bool in_validated_domain(const RoutePath& path,
                                    std::uint64_t output_items,
                                    std::uint32_t frame_count);

    // Mixed 共享池模拟（无固定份额）：
    //   CPU/GPU 各自 ready 时间从 queue delay 开始；谁预计最早空闲谁领取
    //   下一块；搜索 CPU×GPU 候选块组合取最小 makespan。
    // 返回模拟 makespan（ms）；cpu_chunk/gpu_chunk 为推荐块。
    static double simulate_mixed(
        const std::vector<ChunkServicePoint>& cpu_curve,
        const std::vector<ChunkServicePoint>& gpu_curve,
        std::uint64_t total_items,
        std::uint32_t frame_count,
        double cpu_queue_delay_ms,
        double gpu_queue_delay_ms,
        std::uint64_t& cpu_chunk,
        std::uint64_t& gpu_chunk);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::routing

// lib/acr/routing/benchmark_route_estimator.hpp
//
// ACR Benchmark 驱动路由估计器（06 号规范）：
// - 只读离线 Profile v2；
// - 对 RouteRequest 预测 OpenMP / GPU Direct / Mixed 三条候选路径的
// 端到端完工时间；
// - 二维插值（log2 output_items × frame_count），有效范围检查；
// - Mixed 用 CPU/GPU chunk 服务曲线模拟共享池动态 claim（无固定份额）；
// - 叠加队列等待、内存可行性、p95 误差界；
// - 选择最低 score；无画像/范围外安全回退 OpenMP。
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
    // - 生产（diagnostic=false）：scenario_qualified 才进行三候选比较；
    // 场景未 qualified 或无画像/范围外 → chosen=OpenMP fallback；
    // - 诊断（diagnostic=true）：所有 model_available 候选参加预测
    // （即使模型未 trusted），用于发现模型不足（06 号规范 §6）。
    RouteDecision decide(const RouteRequest& request,
                         bool diagnostic = false) const;

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

    // 二维 chunk 服务插值：先沿 chunk 轴（log2），再沿 frame_count 轴。
    // 与 E2E 插值一致：只允许相邻已测帧数之间插值，禁止"未命中→混用全部
    // 曲线"（BDR Reviewed 08 计划 F）。
    static bool interpolate_chunk_service(
        const std::vector<ChunkServicePoint>& curve,
        std::uint64_t chunk_items,
        std::uint32_t frame_count,
        double& median_service_ms,
        double& p90_service_ms);

    // chunk 服务曲线物理合理性 gate（BDR Reviewed 08 计划 E）：
    // - 同 frame_count 下 chunk 增大，服务时间不应显著下降；
    // - 同 chunk 下 frame_count 增大，服务时间不应显著下降。
    // 返回 true=通过；false=异常（调用方必须重测或标记模型不可信）。
    static bool chunk_curve_sanity(
        const std::vector<ChunkServicePoint>& curve,
        std::string& reason);

    // Mixed 共享池模拟（无固定份额）：
    // CPU/GPU 各自 ready 时间从 queue delay 开始；谁预计最早空闲谁领取
    // 下一块；搜索 CPU×GPU 候选块组合取最小 makespan。
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

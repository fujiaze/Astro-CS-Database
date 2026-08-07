// lib/acr/routing/benchmark_route_estimator.cpp
#include "benchmark_route_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace astro::compute::routing {

namespace {

// 按 output_items 升序排序的样本视图（同一 frame_count 分组）
std::vector<const RouteSamplePoint*> samples_for_frames(
    const RoutePath& path, std::uint32_t frame_count) {
    std::vector<const RouteSamplePoint*> out;
    for (const auto& s : path.samples) {
        if (s.frame_count == frame_count) out.push_back(&s);
    }
    std::sort(out.begin(), out.end(),
              [](const RouteSamplePoint* a, const RouteSamplePoint* b) {
                  return a->output_items < b->output_items;
              });
    return out;
}

// 单个 frame_count 下的 log2-items 分段线性插值
bool interpolate_items_curve(const std::vector<const RouteSamplePoint*>& pts,
                             std::uint64_t output_items,
                             double& ms) {
    if (pts.empty()) return false;
    const double x = std::log2(static_cast<double>(output_items));
    if (output_items <= pts.front()->output_items) {
        ms = pts.front()->median_ms;
        return output_items >= pts.front()->output_items;
    }
    if (output_items >= pts.back()->output_items) {
        ms = pts.back()->median_ms;
        return output_items <= pts.back()->output_items;
    }
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double x0 = std::log2(static_cast<double>(pts[i - 1]->output_items));
        const double x1 = std::log2(static_cast<double>(pts[i]->output_items));
        if (x <= x1) {
            const double f =
                (x1 - x0) > 1e-12 ? (x - x0) / (x1 - x0) : 0.0;
            ms = pts[i - 1]->median_ms +
                 f * (pts[i]->median_ms - pts[i - 1]->median_ms);
            return true;
        }
    }
    return false;
}

} // anonymous namespace

struct BenchmarkRouteEstimator::Impl {
    const RouteProfileV2* profile{nullptr};
};

BenchmarkRouteEstimator::BenchmarkRouteEstimator()
    : impl_(std::make_unique<Impl>()) {}
BenchmarkRouteEstimator::~BenchmarkRouteEstimator() = default;

void BenchmarkRouteEstimator::set_profile(
    const RouteProfileV2* profile) noexcept {
    impl_->profile = profile;
}

const RouteProfileV2* BenchmarkRouteEstimator::profile() const noexcept {
    return impl_->profile;
}

bool BenchmarkRouteEstimator::in_validated_domain(
    const RoutePath& path, std::uint64_t output_items,
    std::uint32_t frame_count) {
    if (!path.eligible || path.samples.empty()) return false;
    if (output_items < path.min_output_items ||
        output_items > path.max_output_items) {
        return false;
    }
    // frame_count 必须已测，或介于两个已测帧数之间（允许相邻插值）
    if (path.frame_counts.empty()) return false;
    const auto lo = std::min_element(
        path.frame_counts.begin(), path.frame_counts.end());
    const auto hi = std::max_element(
        path.frame_counts.begin(), path.frame_counts.end());
    if (frame_count < *lo || frame_count > *hi) return false;
    return true;
}

bool BenchmarkRouteEstimator::interpolate_e2e(
    const RoutePath& path, std::uint64_t output_items,
    std::uint32_t frame_count, double& median_ms, double& p90_ms) {
    if (!in_validated_domain(path, output_items, frame_count)) return false;

    // 收集相邻已测帧数的 items 曲线；若恰好命中某帧数则直接插值
    std::vector<std::uint32_t> frames = path.frame_counts;
    std::sort(frames.begin(), frames.end());
    const auto it = std::lower_bound(frames.begin(), frames.end(), frame_count);
    if (it != frames.end() && *it == frame_count) {
        auto pts = samples_for_frames(path, frame_count);
        if (pts.empty()) return false;
        double m = 0.0;
        if (!interpolate_items_curve(pts, output_items, m)) return false;
        median_ms = m;
        // p90：同 items 曲线末样本 p90 近似
        p90_ms = pts.back()->p90_ms;
        return true;
    }
    // 相邻两帧数之间插值
    if (it == frames.begin() || it == frames.end()) return false;
    const std::uint32_t f0 = *(it - 1);
    const std::uint32_t f1 = *it;
    const double w =
        static_cast<double>(frame_count - f0) /
        static_cast<double>(f1 - f0);
    auto p0 = samples_for_frames(path, f0);
    auto p1 = samples_for_frames(path, f1);
    double m0 = 0.0, m1 = 0.0;
    if (!interpolate_items_curve(p0, output_items, m0)) return false;
    if (!interpolate_items_curve(p1, output_items, m1)) return false;
    median_ms = m0 + w * (m1 - m0);
    p90_ms = p0.back()->p90_ms + w * (p1.back()->p90_ms - p0.back()->p90_ms);
    return true;
}

double BenchmarkRouteEstimator::simulate_mixed(
    const std::vector<ChunkServicePoint>& cpu_curve,
    const std::vector<ChunkServicePoint>& gpu_curve,
    std::uint64_t total_items,
    std::uint32_t frame_count,
    double cpu_queue_delay_ms,
    double gpu_queue_delay_ms,
    std::uint64_t& cpu_chunk,
    std::uint64_t& gpu_chunk) {
    cpu_chunk = 0;
    gpu_chunk = 0;
    if (total_items == 0) return 0.0;

    // 按 frame_count 过滤 chunk 服务曲线（未命中帧数则用全部点）
    auto filter = [&](const std::vector<ChunkServicePoint>& curve)
        -> std::vector<ChunkServicePoint> {
        std::vector<ChunkServicePoint> out;
        for (const auto& c : curve) {
            if (c.frame_count == 0 || c.frame_count == frame_count) {
                out.push_back(c);
            }
        }
        return out.empty() ? curve : out;
    };
    const auto cpu_pts = filter(cpu_curve);
    const auto gpu_pts = filter(gpu_curve);
    if (cpu_pts.empty() || gpu_pts.empty()) return -1.0;

    // chunk 服务时间插值：按 chunk_items 线性（log2）插值
    auto service_ms = [](const std::vector<ChunkServicePoint>& pts,
                         std::uint64_t chunk) -> double {
        std::vector<const ChunkServicePoint*> sorted;
        for (const auto& p : pts) sorted.push_back(&p);
        std::sort(sorted.begin(), sorted.end(),
                  [](const ChunkServicePoint* a, const ChunkServicePoint* b) {
                      return a->chunk_items < b->chunk_items;
                  });
        if (chunk <= sorted.front()->chunk_items) {
            return sorted.front()->median_ms;
        }
        if (chunk >= sorted.back()->chunk_items) {
            return sorted.back()->median_ms;
        }
        for (std::size_t i = 1; i < sorted.size(); ++i) {
            if (chunk <= sorted[i]->chunk_items) {
                const double x0 = static_cast<double>(sorted[i - 1]->chunk_items);
                const double x1 = static_cast<double>(sorted[i]->chunk_items);
                const double f =
                    (x1 - x0) > 1e-12
                        ? static_cast<double>(chunk - sorted[i - 1]->chunk_items) /
                              (x1 - x0)
                        : 0.0;
                return sorted[i - 1]->median_ms +
                       f * (sorted[i]->median_ms - sorted[i - 1]->median_ms);
            }
        }
        return sorted.back()->median_ms;
    };

    // 搜索 CPU×GPU 候选块组合的最小模拟 makespan
    double best = std::numeric_limits<double>::infinity();
    std::uint64_t best_cpu = cpu_pts.front().chunk_items;
    std::uint64_t best_gpu = gpu_pts.front().chunk_items;
    for (const auto& c : cpu_pts) {
        for (const auto& g : gpu_pts) {
            // 动态共享池模拟：谁预计最早空闲谁领取下一块
            double cpu_ready = cpu_queue_delay_ms;
            double gpu_ready = gpu_queue_delay_ms;
            std::uint64_t remaining = total_items;
            while (remaining > 0) {
                const std::uint64_t chunk =
                    (cpu_ready <= gpu_ready) ? c.chunk_items : g.chunk_items;
                const std::uint64_t take =
                    std::min(chunk, remaining);
                const double svc = (cpu_ready <= gpu_ready)
                    ? service_ms(cpu_pts, c.chunk_items) *
                          (static_cast<double>(take) /
                           static_cast<double>(c.chunk_items))
                    : service_ms(gpu_pts, g.chunk_items) *
                          (static_cast<double>(take) /
                           static_cast<double>(g.chunk_items));
                if (cpu_ready <= gpu_ready) {
                    cpu_ready += svc;
                } else {
                    gpu_ready += svc;
                }
                remaining -= take;
            }
            const double makespan = std::max(cpu_ready, gpu_ready);
            if (makespan < best) {
                best = makespan;
                best_cpu = c.chunk_items;
                best_gpu = g.chunk_items;
            }
        }
    }
    cpu_chunk = best_cpu;
    gpu_chunk = best_gpu;
    return best;
}

RouteDecision BenchmarkRouteEstimator::decide(
    const RouteRequest& request) const {
    RouteDecision d;
    if (impl_->profile == nullptr) {
        d.chosen = RouteKind::OpenMP;
        d.reason = "no-route-profile";
        d.openmp.feasible = true;
        d.openmp.route = RouteKind::OpenMP;
        d.openmp.reason = "fallback";
        return d;
    }
    const OperationRouteProfile* op =
        impl_->profile->find_operation(request.operation_id);
    if (op == nullptr || !op->qualified) {
        d.chosen = RouteKind::OpenMP;
        d.reason = op == nullptr ? "operation-not-in-profile"
                                 : "operation-not-qualified";
        d.openmp.feasible = true;
        d.openmp.route = RouteKind::OpenMP;
        d.openmp.reason = "fallback";
        return d;
    }

    const std::string sid = scenario_id(
        request.input_residency, request.output_policy,
        request.reuse_count_hint);
    const RouteScenarioProfile* sc = nullptr;
    for (auto& s : op->scenarios) {
        if (s.scenario_id == sid) { sc = &s; break; }
    }
    if (sc == nullptr) {
        d.chosen = RouteKind::OpenMP;
        d.reason = "scenario-not-calibrated: " + sid;
        d.openmp.feasible = true;
        d.openmp.route = RouteKind::OpenMP;
        d.openmp.reason = "fallback";
        return d;
    }

    // ---- OpenMP ----
    {
        RoutePrediction& p = d.openmp;
        p.route = RouteKind::OpenMP;
        double med = 0.0, p90 = 0.0;
        if (sc->openmp.eligible &&
            interpolate_e2e(sc->openmp, request.output_items,
                            request.frame_count, med, p90)) {
            p.feasible = true;
            p.predicted_ms = med;
            p.error_bound_ms = p90 * sc->openmp.p95_error_ratio;
            p.queue_delay_ms = request.queues.cpu_delay_ms;
            p.score_ms = med + p.queue_delay_ms + p.error_bound_ms;
            p.reason = "profile-e2e";
        } else {
            p.feasible = false;
            p.reason = "openmp-not-validated";
        }
    }

    // ---- GPU Direct ----
    {
        RoutePrediction& p = d.gpu_direct;
        p.route = RouteKind::GpuDirect;
        double med = 0.0, p90 = 0.0;
        const bool vram_ok =
            request.memory.vram_available_bytes == 0 ||
            request.output_bytes + request.input_bytes <=
                request.memory.vram_available_bytes;
        if (sc->gpu_direct.eligible &&
            interpolate_e2e(sc->gpu_direct, request.output_items,
                            request.frame_count, med, p90) &&
            vram_ok) {
            p.feasible = true;
            p.predicted_ms = med;
            p.error_bound_ms = p90 * sc->gpu_direct.p95_error_ratio;
            p.queue_delay_ms = request.queues.gpu_delay_ms;
            p.score_ms = med + p.queue_delay_ms + p.error_bound_ms;
            p.reason = "profile-e2e";
        } else if (!vram_ok) {
            p.feasible = false;
            p.reason = "vram-insufficient";
        } else {
            p.feasible = false;
            p.reason = "gpu-not-validated";
        }
    }

    // ---- Mixed ----
    {
        RoutePrediction& p = d.mixed;
        p.route = RouteKind::Mixed;
        const bool vram_ok =
            request.memory.vram_available_bytes == 0 ||
            request.output_bytes + request.input_bytes <=
                request.memory.vram_available_bytes;
        std::uint64_t cpu_chunk = 0, gpu_chunk = 0;
        const double makespan = simulate_mixed(
            op->cpu_chunk_service, op->gpu_chunk_service,
            request.output_items, request.frame_count,
            request.queues.cpu_delay_ms, request.queues.gpu_delay_ms,
            cpu_chunk, gpu_chunk);
        if (sc->mixed.eligible && makespan >= 0.0 && vram_ok) {
            p.feasible = true;
            p.predicted_ms = makespan + op->mixed_fixed_overhead_ms;
            p.error_bound_ms = makespan * sc->mixed.p95_error_ratio;
            p.queue_delay_ms = 0.0;  // 已计入模拟初值
            p.score_ms = p.predicted_ms + p.error_bound_ms;
            d.cpu_chunk_items = cpu_chunk;
            d.gpu_chunk_items = gpu_chunk;
            p.reason = "mixed-simulation";
        } else if (!vram_ok) {
            p.feasible = false;
            p.reason = "vram-insufficient";
        } else {
            p.feasible = false;
            p.reason = "mixed-not-validated";
        }
    }

    // ---- 选择最低 score（平局：资源更少路径 = OpenMP < GPU < Mixed）----
    const RoutePrediction* best = nullptr;
    if (d.openmp.feasible) best = &d.openmp;
    if (d.gpu_direct.feasible &&
        (best == nullptr || d.gpu_direct.score_ms < best->score_ms)) {
        best = &d.gpu_direct;
    }
    if (d.mixed.feasible &&
        (best == nullptr || d.mixed.score_ms < best->score_ms)) {
        best = &d.mixed;
    }
    if (best == nullptr) {
        d.chosen = RouteKind::OpenMP;
        d.reason = "no-feasible-profile-path";
        d.openmp.feasible = true;
        d.openmp.route = RouteKind::OpenMP;
        d.openmp.reason = "fallback";
        return d;
    }
    d.chosen = best->route;
    d.reason = "min-score";
    return d;
}

} // namespace astro::compute::routing

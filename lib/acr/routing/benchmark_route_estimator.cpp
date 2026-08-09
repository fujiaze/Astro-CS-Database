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

// 单个 frame_count 下的 items 曲线插值（按 interpolation_id 选模型）
bool interpolate_items_curve(const std::vector<const RouteSamplePoint*>& pts,
                             std::uint64_t output_items,
                             const std::string& interpolation_id,
                             double& ms, double& p90) {
    if (pts.empty()) return false;
    const bool loglog =
        (interpolation_id == "piecewise-loglog-items-frames-time");
    // items 轴：linear 用原值；loglog 用 log2
    const double x =
        loglog ? std::log2(static_cast<double>(output_items))
               : static_cast<double>(output_items);
    if (output_items <= pts.front()->output_items) {
        ms = pts.front()->median_ms;
        p90 = pts.front()->p90_ms;
        return output_items >= pts.front()->output_items;
    }
    if (output_items >= pts.back()->output_items) {
        ms = pts.back()->median_ms;
        p90 = pts.back()->p90_ms;
        return output_items <= pts.back()->output_items;
    }
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double x0 =
            loglog ? std::log2(static_cast<double>(pts[i - 1]->output_items))
                   : static_cast<double>(pts[i - 1]->output_items);
        const double x1 =
            loglog ? std::log2(static_cast<double>(pts[i]->output_items))
                   : static_cast<double>(pts[i]->output_items);
        if (x <= x1) {
            const double f =
                (x1 - x0) > 1e-12 ? (x - x0) / (x1 - x0) : 0.0;
            // loglog 在 log(time) 空间插值；linear 在 time 空间插值
            if (loglog) {
                const double lt0 = std::log2(std::max(pts[i - 1]->median_ms, 1e-9));
                const double lt1 = std::log2(std::max(pts[i]->median_ms, 1e-9));
                ms = std::exp2(lt0 + f * (lt1 - lt0));
                const double lp0 = std::log2(std::max(pts[i - 1]->p90_ms, 1e-9));
                const double lp1 = std::log2(std::max(pts[i]->p90_ms, 1e-9));
                p90 = std::exp2(lp0 + f * (lp1 - lp0));
            } else {
                ms = pts[i - 1]->median_ms +
                     f * (pts[i]->median_ms - pts[i - 1]->median_ms);
                p90 = pts[i - 1]->p90_ms +
                      f * (pts[i]->p90_ms - pts[i - 1]->p90_ms);
            }
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
    // 范围检查只依赖样本与 validated domain，不依赖资格字段：
    // 生产 decide 已用 model_trusted / model_available 筛选调用；
    // 标定阶段 evaluate_final 在设置资格前需要先预测（顺序依赖）。
    if (path.samples.empty()) return false;
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

bool BenchmarkRouteEstimator::interpolate_chunk_service(
    const std::vector<ChunkServicePoint>& curve,
    std::uint64_t chunk_items,
    std::uint32_t frame_count,
    double& median_service_ms,
    double& p90_service_ms) {
    if (curve.empty() || chunk_items == 0 || frame_count == 0) return false;

    // 已测帧数集合
    std::vector<std::uint32_t> frames;
    for (const auto& c : curve) {
        if (std::find(frames.begin(), frames.end(), c.frame_count) ==
            frames.end()) {
            frames.push_back(c.frame_count);
        }
    }
    std::sort(frames.begin(), frames.end());

    auto chunk_interp = [&](std::uint32_t fc, double& ms,
                            double& p90) -> bool {
        std::vector<const ChunkServicePoint*> pts;
        for (const auto& c : curve) {
            if (c.frame_count == fc) pts.push_back(&c);
        }
        if (pts.empty()) return false;
        std::sort(pts.begin(), pts.end(),
                  [](const ChunkServicePoint* a,
                     const ChunkServicePoint* b) {
                      return a->chunk_items < b->chunk_items;
                  });
        // 只允许有效范围内插值（不无条件外推）
        if (chunk_items < pts.front()->chunk_items ||
            chunk_items > pts.back()->chunk_items) {
            return false;
        }
        if (pts.size() == 1 ||
            chunk_items == pts.front()->chunk_items) {
            ms = pts.front()->median_service_ms;
            p90 = pts.front()->p90_service_ms;
            return true;
        }
        const double x =
            std::log2(static_cast<double>(chunk_items));
        for (std::size_t i = 1; i < pts.size(); ++i) {
            if (chunk_items <= pts[i]->chunk_items) {
                const double x0 =
                    std::log2(static_cast<double>(pts[i - 1]->chunk_items));
                const double x1 =
                    std::log2(static_cast<double>(pts[i]->chunk_items));
                const double f =
                    (x1 - x0) > 1e-12 ? (x - x0) / (x1 - x0) : 0.0;
                const double s0 = pts[i - 1]->median_service_ms;
                const double s1 = pts[i]->median_service_ms;
                const double l0 = std::log2(std::max(s0, 1e-9));
                const double l1 = std::log2(std::max(s1, 1e-9));
                ms = std::exp2(l0 + f * (l1 - l0));
                const double q0 = pts[i - 1]->p90_service_ms;
                const double q1 = pts[i]->p90_service_ms;
                const double lq0 = std::log2(std::max(q0, 1e-9));
                const double lq1 = std::log2(std::max(q1, 1e-9));
                p90 = std::exp2(lq0 + f * (lq1 - lq0));
                return true;
            }
        }
        return false;
    };

    const auto it =
        std::lower_bound(frames.begin(), frames.end(), frame_count);
    if (it != frames.end() && *it == frame_count) {
        return chunk_interp(frame_count, median_service_ms, p90_service_ms);
    }
    // 相邻已测帧数之间插值（先 chunk 轴、再 frame 轴）
    if (it == frames.begin() || it == frames.end()) return false;
    const std::uint32_t f0 = *(it - 1);
    const std::uint32_t f1 = *it;
    double m0 = 0.0, p0 = 0.0, m1 = 0.0, p1 = 0.0;
    if (!chunk_interp(f0, m0, p0)) return false;
    if (!chunk_interp(f1, m1, p1)) return false;
    const double w =
        static_cast<double>(frame_count - f0) /
        static_cast<double>(f1 - f0);
    median_service_ms = m0 + w * (m1 - m0);
    p90_service_ms = p0 + w * (p1 - p0);
    return true;
}

bool BenchmarkRouteEstimator::chunk_curve_sanity(
    const std::vector<ChunkServicePoint>& curve,
    std::string& reason) {
    if (curve.empty()) {
        reason = "empty curve";
        return false;
    }
    std::vector<std::uint32_t> frames;
    for (const auto& c : curve) {
        if (std::find(frames.begin(), frames.end(), c.frame_count) ==
            frames.end()) {
            frames.push_back(c.frame_count);
        }
    }
    std::sort(frames.begin(), frames.end());
    for (std::uint32_t fc : frames) {
        std::vector<const ChunkServicePoint*> pts;
        for (const auto& c : curve) {
            if (c.frame_count == fc) pts.push_back(&c);
        }
        std::sort(pts.begin(), pts.end(),
                  [](const ChunkServicePoint* a,
                     const ChunkServicePoint* b) {
                      return a->chunk_items < b->chunk_items;
                  });
        for (std::size_t i = 1; i < pts.size(); ++i) {
            const std::uint64_t w0 = pts[i - 1]->chunk_items;
            const std::uint64_t w1 = pts[i]->chunk_items;
            const double t0 = pts[i - 1]->median_service_ms;
            const double t1 = pts[i]->median_service_ms;
            // 工作量显著增加（>=1.5x）但服务时间下降超过 30%：物理异常
            if (w1 >= w0 * 3 / 2 && t1 > 0.0 && t1 < t0 * 0.70) {
                reason = "chunk monotonicity violation frame=" +
                         std::to_string(fc) + " " +
                         std::to_string(w0) + "->" + std::to_string(w1) +
                         " time " + std::to_string(t0) + "->" +
                         std::to_string(t1);
                return false;
            }
        }
    }
    // 同 chunk 下 frame_count 显著增加（>=2x）而服务时间下降超过 50%
    if (frames.size() >= 2) {
        for (std::size_t i = 1; i < frames.size(); ++i) {
            const std::uint32_t f0 = frames[i - 1];
            const std::uint32_t f1 = frames[i];
            for (const auto& c0 : curve) {
                if (c0.frame_count != f0) continue;
                double m1 = 0.0, p90 = 0.0;
                if (!interpolate_chunk_service(curve, c0.chunk_items, f1,
                                               m1, p90)) {
                    continue;
                }
                if (f1 >= f0 * 2 && m1 > 0.0 &&
                    m1 < c0.median_service_ms * 0.50) {
                    reason = "frame monotonicity violation chunk=" +
                             std::to_string(c0.chunk_items) + " frames " +
                             std::to_string(f0) + "->" + std::to_string(f1) +
                             " time " +
                             std::to_string(c0.median_service_ms) + "->" +
                             std::to_string(m1);
                    return false;
                }
            }
        }
    }
    reason.clear();
    return true;
}

bool BenchmarkRouteEstimator::interpolate_e2e(
    const RoutePath& path, std::uint64_t output_items,
    std::uint32_t frame_count, double& median_ms, double& p90_ms) {
    if (!in_validated_domain(path, output_items, frame_count)) return false;

    // 收集相邻已测帧数的 items 曲线；若恰好命中某帧数则直接插值
    std::vector<std::uint32_t> frames = path.frame_counts;
    std::sort(frames.begin(), frames.end());
    // 只保留"对请求 items 能成功插值"的已测帧：
    // adaptive 转入的单样本帧（如 12/28 帧仅 1 个点）若无法覆盖请求 items，
    // 不得参与相邻帧插值（否则相邻插值失败），应退回 4/16/32 基础网格。
    std::vector<std::uint32_t> usable;
    for (std::uint32_t f : frames) {
        auto pts = samples_for_frames(path, f);
        double m = 0.0, p = 0.0;
        if (interpolate_items_curve(pts, output_items,
                                    path.interpolation_id, m, p)) {
            usable.push_back(f);
        }
    }
    if (usable.empty()) return false;
    const auto it =
        std::lower_bound(usable.begin(), usable.end(), frame_count);
    if (it != usable.end() && *it == frame_count) {
        // 精确命中且该帧可用（usable 已保证）
        auto pts = samples_for_frames(path, frame_count);
        double m = 0.0;
        interpolate_items_curve(pts, output_items, path.interpolation_id,
                                m, p90_ms);
        median_ms = m;
        return true;
    }
    // 相邻两帧数之间插值
    if (it == usable.begin() || it == usable.end()) return false;
    const std::uint32_t f0 = *(it - 1);
    const std::uint32_t f1 = *it;
    const double w =
        static_cast<double>(frame_count - f0) /
        static_cast<double>(f1 - f0);
    auto p0 = samples_for_frames(path, f0);
    auto p1 = samples_for_frames(path, f1);
    double m0 = 0.0, m1 = 0.0, p0v = 0.0, p1v = 0.0;
    interpolate_items_curve(p0, output_items, path.interpolation_id,
                            m0, p0v);
    interpolate_items_curve(p1, output_items, path.interpolation_id,
                            m1, p1v);
    median_ms = m0 + w * (m1 - m0);
    p90_ms = p0v + w * (p1v - p0v);
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

    // BDR Reviewed（08 计划 F）：chunk 服务必须按 (chunk_items, frame_count)
    // 二维插值；12/24 帧在相邻已测帧曲线间插值，禁止混用全部曲线。
    auto service_ms = [](const std::vector<ChunkServicePoint>& pts,
                         std::uint64_t chunk, std::uint32_t fc) -> double {
        double ms = 0.0, p90 = 0.0;
        if (!interpolate_chunk_service(pts, chunk, fc, ms, p90)) {
            return -1.0;
        }
        return ms;
    };

    // 候选块集合（去重）
    auto unique_chunks = [](const std::vector<ChunkServicePoint>& curve) {
        std::vector<std::uint64_t> out;
        for (const auto& c : curve) {
            if (std::find(out.begin(), out.end(), c.chunk_items) ==
                out.end()) {
                out.push_back(c.chunk_items);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    };
    const auto cpu_chunks = unique_chunks(cpu_curve);
    const auto gpu_chunks = unique_chunks(gpu_curve);
    if (cpu_chunks.empty() || gpu_chunks.empty()) return -1.0;

    // 搜索 CPU×GPU 候选块组合的最小模拟 makespan
    double best = std::numeric_limits<double>::infinity();
    std::uint64_t best_cpu = cpu_chunks.front();
    std::uint64_t best_gpu = gpu_chunks.front();
    for (std::uint64_t c : cpu_chunks) {
        for (std::uint64_t g : gpu_chunks) {
            // 动态共享池模拟：谁预计最早空闲谁领取下一块
            double cpu_ready = cpu_queue_delay_ms;
            double gpu_ready = gpu_queue_delay_ms;
            bool svc_ok = true;
            std::uint64_t remaining = total_items;
            while (remaining > 0) {
                const std::uint64_t chunk =
                    (cpu_ready <= gpu_ready) ? c : g;
                const std::uint64_t take =
                    std::min(chunk, remaining);
                const double svc = (cpu_ready <= gpu_ready)
                    ? service_ms(cpu_curve, take, frame_count)
                    : service_ms(gpu_curve, take, frame_count);
                if (svc < 0.0) {
                    svc_ok = false;
                    break;
                }
                if (cpu_ready <= gpu_ready) {
                    cpu_ready += svc;
                } else {
                    gpu_ready += svc;
                }
                remaining -= take;
            }
            if (!svc_ok) continue;
            const double makespan = std::max(cpu_ready, gpu_ready);
            if (makespan < best) {
                best = makespan;
                best_cpu = c;
                best_gpu = g;
            }
        }
    }
    if (best == std::numeric_limits<double>::infinity()) return -1.0;
    cpu_chunk = best_cpu;
    gpu_chunk = best_gpu;
    return best;
}

RouteDecision BenchmarkRouteEstimator::decide(
    const RouteRequest& request, bool diagnostic) const {
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
    if (op == nullptr || (!diagnostic && !op->qualified)) {
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

    // Dispatcher Finalization（08 计划 1）：Route-centric 资格。
    // 场景 routing_trusted=false → 生产只 OpenMP fallback；
    // routing_trusted=true → 生产与诊断一致使用全部 model_available
    // 候选比较（单路径 10%/15% 绝对误差只做 error guard，不删除候选）。
    if (!diagnostic && !(sc->routing_trusted || sc->scenario_qualified)) {
        d.chosen = RouteKind::OpenMP;
        d.reason = "scenario-not-qualified: " + sid;
        d.openmp.feasible = true;
        d.openmp.route = RouteKind::OpenMP;
        d.openmp.reason = "fallback";
        return d;
    }

    const auto path_usable = [&](const RoutePath& p) {
        return p.model_available;
    };

    // ---- OpenMP ----
    {
        RoutePrediction& p = d.openmp;
        p.route = RouteKind::OpenMP;
        double med = 0.0, p90 = 0.0;
        if (path_usable(sc->openmp) &&
            interpolate_e2e(sc->openmp, request.output_items,
                            request.frame_count, med, p90)) {
            p.feasible = true;
            p.predicted_ms = med;
            p.error_bound_ms = med * sc->openmp.max_error_ratio;
            p.queue_delay_ms = request.queues.cpu_delay_ms;
            p.score_ms =
                med * (1.0 + sc->openmp.max_error_ratio) +
                p.queue_delay_ms;
            p.reason = "profile-e2e";
        } else {
            // OpenMP 永远是执行可行的安全 fallback（06 号规范 §2）
            p.feasible = true;
            p.reason = "openmp-fallback-no-trusted-model";
            p.score_ms = std::numeric_limits<double>::infinity();
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
        if (path_usable(sc->gpu_direct) &&
            interpolate_e2e(sc->gpu_direct, request.output_items,
                            request.frame_count, med, p90) &&
            vram_ok) {
            p.feasible = true;
            p.predicted_ms = med;
            p.error_bound_ms = med * sc->gpu_direct.max_error_ratio;
            p.queue_delay_ms = request.queues.gpu_delay_ms;
            p.score_ms =
                med * (1.0 + sc->gpu_direct.max_error_ratio) +
                p.queue_delay_ms;
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
        // BDR 复核（08 计划 E）：真实 Mixed E2E 插值是主成本；
        // chunk simulator 只做分块与排队修正。
        double base = 0.0, base_p90 = 0.0;
        const bool has_e2e = path_usable(sc->mixed) &&
            interpolate_e2e(sc->mixed, request.output_items,
                            request.frame_count, base, base_p90);
        std::uint64_t cpu_chunk = 0, gpu_chunk = 0;
        const double sim0 = simulate_mixed(
            op->cpu_chunk_service, op->gpu_chunk_service,
            request.output_items, request.frame_count,
            0.0, 0.0,
            cpu_chunk, gpu_chunk);
        std::uint64_t cpu_chunk_q = 0, gpu_chunk_q = 0;
        const double simq = simulate_mixed(
            op->cpu_chunk_service, op->gpu_chunk_service,
            request.output_items, request.frame_count,
            request.queues.cpu_delay_ms, request.queues.gpu_delay_ms,
            cpu_chunk_q, gpu_chunk_q);
        if (has_e2e && sim0 >= 0.0 && simq >= 0.0 && vram_ok) {
            p.feasible = true;
            p.predicted_ms =
                base + std::max(0.0, simq - sim0);
            p.error_bound_ms = base * sc->mixed.max_error_ratio;
            p.queue_delay_ms = 0.0;  // 已计入 simq-sim0
            p.score_ms = p.predicted_ms * (1.0 + sc->mixed.max_error_ratio);
            d.cpu_chunk_items = cpu_chunk_q;
            d.gpu_chunk_items = gpu_chunk_q;
            p.reason = "mixed-e2e-plus-queue-correction";
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
    if (d.openmp.feasible &&
        d.openmp.score_ms < std::numeric_limits<double>::infinity()) {
        best = &d.openmp;
    }
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

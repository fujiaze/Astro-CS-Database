// lib/acr/utilization/gpu_controller.cpp — GpuController 实现
//
// Phase G：真实 GPU 利用率控制。
//   - NVML 可用：实读 nvmlDeviceGetUtilizationRates
//   - NVML 不可用：按队列预算估算（queue_depth / max_depth），明确标记 estimated=true
//   - 通过 queue/stream 深度、batch 大小、提交节奏控制
//   - 不允许无界排队（max_queue_depth 上限）
//   - 多 GPU 独立控制
//   - 严重超目标时 throttle（暂停提交）
#include "gpu_controller.hpp"

#include "system_metrics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace astro::compute::utilization {

namespace {
constexpr double kDefaultTarget = 0.95;
constexpr double kMinTarget = 0.0;
constexpr double kMaxTarget = 1.0;
constexpr std::uint32_t kDefaultMaxQueueDepth = 8;

inline std::uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
} // anonymous namespace

struct GpuController::Impl {
    mutable std::mutex mtx;
    std::atomic<double> target{kDefaultTarget};
    std::atomic<std::uint32_t> max_qdepth{kDefaultMaxQueueDepth};
    std::atomic<bool> cancelled{false};
    std::vector<std::string> backends;
    // 每个 backend 的最后观察值（status_json 用）
    mutable std::unordered_map<std::string, double> last_actual;
    mutable std::unordered_map<std::string, double> last_error;
    mutable std::unordered_map<std::string, bool> last_estimated;
    mutable std::unordered_map<std::string, bool> last_valid;

    SystemMetrics metrics;

    // 控制决策核心
    GpuControlDecision decide_impl(const std::string& backend,
                                   double actual_ratio,
                                   bool estimated,
                                   bool valid) {
        GpuControlDecision d;
        d.backend = backend;
        d.target_ratio = target.load(std::memory_order_relaxed);
        d.actual_ratio = actual_ratio;
        d.actual_estimated = estimated;
        d.valid = valid;
        d.error_ratio = actual_ratio - d.target_ratio;
        d.timestamp_ns = now_ns();
        d.max_queue_depth = max_qdepth.load(std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lk(mtx);
            last_actual[backend] = actual_ratio;
            last_error[backend] = d.error_ratio;
            last_estimated[backend] = estimated;
            last_valid[backend] = valid;
        }

        // 取消状态：最小提交 + throttle
        if (cancelled.load(std::memory_order_acquire)) {
            d.queue_depth = 1;
            d.batch_size = 1;
            d.throttle = true;
            return d;
        }

        if (!valid) {
            d.queue_depth = 1;
            d.batch_size = 1;
            d.throttle = false;
            return d;
        }

        // 控制策略：误差带 ±0.05
        const double tolerance = 0.05;
        bool too_high = actual_ratio > d.target_ratio + tolerance;
        bool too_low = actual_ratio < d.target_ratio - tolerance;
        // 严重超目标（+0.15）→ throttle
        bool severe = actual_ratio > d.target_ratio + 0.15;

        std::uint32_t maxq = d.max_queue_depth;
        if (maxq == 0) maxq = 1;

        if (too_high) {
            d.queue_depth = 1;                          // 浅队列减少积压
            d.batch_size = 1;
            d.throttle = severe;
        } else if (too_low) {
            d.queue_depth = std::min<std::uint32_t>(4, maxq);  // 深队列增加积压，但不超过上限
            d.batch_size = 4;
            d.throttle = false;
        } else {
            d.queue_depth = std::min<std::uint32_t>(2, maxq);
            d.batch_size = 2;
            d.throttle = false;
        }
        return d;
    }
};

GpuController::GpuController() : impl_(std::make_unique<Impl>()) {}
GpuController::~GpuController() = default;

void GpuController::set_target(double target_ratio) noexcept {
    if (target_ratio < kMinTarget) target_ratio = kMinTarget;
    if (target_ratio > kMaxTarget) target_ratio = kMaxTarget;
    impl_->target.store(target_ratio, std::memory_order_relaxed);
}

double GpuController::target() const noexcept {
    return impl_->target.load(std::memory_order_relaxed);
}

void GpuController::set_max_queue_depth(std::uint32_t max_depth) noexcept {
    if (max_depth == 0) max_depth = 1;
    impl_->max_qdepth.store(max_depth, std::memory_order_relaxed);
    // 同步到 SystemMetrics 的队列预算上限
    impl_->metrics.set_queue_budget_max_depth(max_depth);
}

std::uint32_t GpuController::max_queue_depth() const noexcept {
    return impl_->max_qdepth.load(std::memory_order_relaxed);
}

void GpuController::register_backend(const std::string& backend) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (std::find(impl_->backends.begin(), impl_->backends.end(), backend)
        == impl_->backends.end()) {
        impl_->backends.push_back(backend);
        impl_->last_actual[backend] = 0.0;
        impl_->last_error[backend] = 0.0;
        impl_->last_estimated[backend] = false;
        impl_->last_valid[backend] = false;
    }
    impl_->metrics.register_backend(backend);
}

std::vector<std::string> GpuController::backends() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->backends;
}

void GpuController::report_queue_depth(const std::string& backend, std::uint32_t depth) {
    impl_->metrics.report_queue_depth(backend, depth);
    // 确保已注册
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (std::find(impl_->backends.begin(), impl_->backends.end(), backend)
        == impl_->backends.end()) {
        impl_->backends.push_back(backend);
        impl_->last_actual[backend] = 0.0;
        impl_->last_error[backend] = 0.0;
        impl_->last_estimated[backend] = false;
        impl_->last_valid[backend] = false;
    }
}

// ===== 实际利用率采样 + 决策 =====
std::vector<GpuControlDecision> GpuController::sample_and_decide() {
    // 读取所有 backend 的实际利用率（NVML 或队列预算估算）
    std::vector<GpuUtilizationSample> samples = impl_->metrics.read_gpu_utilizations();
    std::vector<GpuControlDecision> out;
    out.reserve(samples.size());
    for (const auto& s : samples) {
        out.push_back(impl_->decide_impl(s.backend, s.ratio, s.estimated, s.valid));
    }
    // 也为已注册但未在 samples 中的 backend 补决策（valid=false）
    std::vector<std::string> registered;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        registered = impl_->backends;
    }
    for (const auto& b : registered) {
        bool found = false;
        for (const auto& s : samples) {
            if (s.backend == b) { found = true; break; }
        }
        if (!found) {
            out.push_back(impl_->decide_impl(b, 0.0, /*estimated=*/true, /*valid=*/false));
        }
    }
    return out;
}

GpuControlDecision GpuController::sample_and_decide(const std::string& backend) {
    // 读取所有，筛选指定 backend
    auto all = sample_and_decide();
    for (auto& d : all) {
        if (d.backend == backend) return d;
    }
    // 未注册，返回 invalid
    return impl_->decide_impl(backend, 0.0, /*estimated=*/true, /*valid=*/false);
}

GpuControlDecision GpuController::decide_with_actual(const std::string& backend,
                                                     double actual_ratio, bool estimated) {
    return impl_->decide_impl(backend, actual_ratio, estimated, /*valid=*/true);
}

// ===== NVML 状态 =====
bool GpuController::nvml_available() const noexcept {
    return impl_->metrics.nvml_available();
}

std::size_t GpuController::gpu_count() const noexcept {
    return impl_->metrics.gpu_count();
}

bool GpuController::reload_nvml() {
    return impl_->metrics.reload_nvml();
}

// ===== 取消与响应 =====
void GpuController::request_cancel() noexcept {
    impl_->cancelled.store(true, std::memory_order_release);
}

bool GpuController::cancelled() const noexcept {
    return impl_->cancelled.load(std::memory_order_acquire);
}

void GpuController::clear_cancel() noexcept {
    impl_->cancelled.store(false, std::memory_order_release);
}

// ===== 状态 JSON =====
std::string GpuController::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"target\":" << impl_->target.load(std::memory_order_relaxed);
    os << ",\"max_queue_depth\":" << impl_->max_qdepth.load(std::memory_order_relaxed);
    os << ",\"cancelled\":" << (impl_->cancelled.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"nvml_available\":" << (impl_->metrics.nvml_available() ? "true" : "false");
    os << ",\"gpu_count\":" << impl_->metrics.gpu_count();
    os << ",\"backends\":[";
    for (std::size_t i = 0; i < impl_->backends.size(); ++i) {
        if (i > 0) os << ",";
        const auto& b = impl_->backends[i];
        os << "{\"backend\":\"" << b << "\"";
        os << ",\"last_actual\":" << impl_->last_actual[b];
        os << ",\"last_error\":" << impl_->last_error[b];
        os << ",\"last_estimated\":" << (impl_->last_estimated[b] ? "true" : "false");
        os << ",\"last_valid\":" << (impl_->last_valid[b] ? "true" : "false");
        os << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace astro::compute::utilization

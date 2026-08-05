// lib/acr/utilization/cpu_controller.cpp — CpuController 实现
//
// Phase G：真实 CPU 利用率控制。
//   - 通过 SystemMetrics 读取 GetSystemTimes 实际利用率
//   - 根据误差调节提交节奏：批次大小/队列水位/worker 让步/优先级
//   - 不通过永久少开线程实现 95%（所有 worker 可注册参与）
//   - 错峰让步：yield_stride 让 worker 轮流让步，避免全线程同步睡眠
//   - 控制窗口 100-500ms（仅作日志/采样节奏参考，不阻塞）
//   - 保持取消、状态响应
#include "cpu_controller.hpp"

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
constexpr double kMinTarget = 0.0;
constexpr double kMaxTarget = 1.0;
constexpr double kDefaultTarget = 0.95;
constexpr std::uint32_t kDefaultWindowMs = 200;
constexpr std::uint32_t kMinWindowMs = 100;
constexpr std::uint32_t kMaxWindowMs = 500;

inline std::uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
} // anonymous namespace

struct CpuController::Impl {
    mutable std::mutex mtx;
    std::atomic<double> target{kDefaultTarget};
    std::atomic<int> strategy{static_cast<int>(ControlStrategy::BatchSize)};
    std::atomic<std::uint32_t> window_ms{kDefaultWindowMs};
    std::atomic<bool> cancelled{false};

    // worker 参与
    std::unordered_map<std::uint32_t, bool> workers;  // id → active
    std::uint32_t next_worker_id{1};

    // 实际利用率读取
    SystemMetrics metrics;

    // 上次决策缓存（status_json 用）
    mutable std::atomic<double> last_actual{0.0};
    mutable std::atomic<double> last_error{0.0};
    mutable std::atomic<bool> last_valid{false};
    mutable std::atomic<std::uint64_t> last_ts{0};
    // 24 号计划 §4：活跃 worker 预算状态（可升可降）
    mutable std::atomic<double> last_active_budget{1.0};

    // 控制策略核心：根据 actual vs target 做决策
    CpuControlDecision decide_impl(double actual_ratio, bool estimated, bool valid) {
        CpuControlDecision d;
        d.target_ratio = target.load(std::memory_order_relaxed);
        d.actual_ratio = actual_ratio;
        d.actual_estimated = estimated;
        d.valid = valid;
        d.error_ratio = actual_ratio - d.target_ratio;
        d.timestamp_ns = now_ns();
        last_actual.store(actual_ratio, std::memory_order_relaxed);
        last_error.store(d.error_ratio, std::memory_order_relaxed);
        last_valid.store(valid, std::memory_order_relaxed);
        last_ts.store(d.timestamp_ns, std::memory_order_relaxed);

        // 取消状态：建议最小批次 + 让步
        if (cancelled.load(std::memory_order_acquire)) {
            d.batch_size = 1;
            d.queue_depth = 1;
            d.priority = -1;
            d.should_yield = true;
            d.yield_stride = 1;
            d.active_budget = last_active_budget.load(std::memory_order_relaxed);
            return d;
        }

        if (!valid) {
            // 首次采样无基线，给保守默认值
            d.batch_size = 1;
            d.queue_depth = 1;
            d.should_yield = false;
            d.active_budget = last_active_budget.load(std::memory_order_relaxed);
            return d;
        }

        // 控制策略：误差带 ±0.05
        // - actual > target + 0.05：降低利用率（更小批次 / 更深队列 / 让步）
        // - actual < target - 0.05：增加利用率（更大批次 / 浅队列 / 不让步）
        // - |error| < 0.05：保持
        const double tolerance = 0.05;
        bool too_high = actual_ratio > d.target_ratio + tolerance;
        bool too_low = actual_ratio < d.target_ratio - tolerance;

        // 24 号计划 §4：活跃预算按误差带升降（下限 0.25：至少保留 25% 并发，
        // 避免降到单线程导致任务被拖死；上限 1.0）。
        // 取代“actual > target+0.10 关闭全局 gate”（95%/100% 永不触发的问题）。
        double budget = last_active_budget.load(std::memory_order_relaxed);
        if (too_high) {
            budget = std::max(0.25, budget - 0.15);
        } else if (too_low) {
            budget = std::min(1.0, budget + 0.15);
        }
        last_active_budget.store(budget, std::memory_order_relaxed);
        d.active_budget = budget;

        // 错峰让步步幅：基于活跃 worker 数（>=1），让 worker 轮流让步
        std::uint32_t active_workers = 0;
        {
            std::lock_guard<std::mutex> lk(mtx);
            for (const auto& kv : workers) {
                if (kv.second) ++active_workers;
            }
        }
        if (active_workers == 0) active_workers = 1;

        switch (static_cast<ControlStrategy>(strategy.load(std::memory_order_relaxed))) {
            case ControlStrategy::BatchSize:
                if (too_high) {
                    d.batch_size = 1;          // 最小批次 → 频繁让步机会
                    d.should_yield = true;
                    d.yield_stride = active_workers;  // 错峰：每 N 个 worker 轮一次
                } else if (too_low) {
                    d.batch_size = 8;          // 大批次 → 少让步
                    d.should_yield = false;
                    d.yield_stride = 0;
                } else {
                    d.batch_size = 4;
                    d.should_yield = false;
                    d.yield_stride = 0;
                }
                d.queue_depth = 2;
                break;
            case ControlStrategy::QueueDepth:
                if (too_high) {
                    d.queue_depth = 1;         // 浅队列 → 减少积压
                    d.should_yield = true;
                    d.yield_stride = active_workers;
                } else if (too_low) {
                    d.queue_depth = 4;         // 深队列 → 增加积压
                    d.should_yield = false;
                    d.yield_stride = 0;
                } else {
                    d.queue_depth = 2;
                    d.should_yield = false;
                    d.yield_stride = 0;
                }
                d.batch_size = 2;
                break;
            case ControlStrategy::Priority:
                if (too_high) {
                    d.priority = -1;           // 降优先级
                    d.should_yield = true;
                    d.yield_stride = active_workers;
                } else if (too_low) {
                    d.priority = 1;            // 升优先级
                    d.should_yield = false;
                    d.yield_stride = 0;
                } else {
                    d.priority = 0;
                    d.should_yield = false;
                    d.yield_stride = 0;
                }
                d.batch_size = 2;
                d.queue_depth = 2;
                break;
            case ControlStrategy::Yield:
                d.batch_size = 2;
                d.queue_depth = 2;
                if (too_high) {
                    d.should_yield = true;
                    d.yield_stride = active_workers;  // 错峰
                } else {
                    d.should_yield = false;
                    d.yield_stride = 0;
                }
                break;
        }
        return d;
    }
};

CpuController::CpuController() : impl_(std::make_unique<Impl>()) {}
CpuController::~CpuController() = default;

void CpuController::set_target(double target_ratio) noexcept {
    if (target_ratio < kMinTarget) target_ratio = kMinTarget;
    if (target_ratio > kMaxTarget) target_ratio = kMaxTarget;
    impl_->target.store(target_ratio, std::memory_order_relaxed);
}

double CpuController::target() const noexcept {
    return impl_->target.load(std::memory_order_relaxed);
}

void CpuController::set_strategy(ControlStrategy s) noexcept {
    impl_->strategy.store(static_cast<int>(s), std::memory_order_relaxed);
}

ControlStrategy CpuController::strategy() const noexcept {
    return static_cast<ControlStrategy>(impl_->strategy.load(std::memory_order_relaxed));
}

void CpuController::set_control_window_ms(std::uint32_t ms) noexcept {
    if (ms < kMinWindowMs) ms = kMinWindowMs;
    if (ms > kMaxWindowMs) ms = kMaxWindowMs;
    impl_->window_ms.store(ms, std::memory_order_relaxed);
}

std::uint32_t CpuController::control_window_ms() const noexcept {
    return impl_->window_ms.load(std::memory_order_relaxed);
}

// ===== Worker 参与 =====
std::uint32_t CpuController::register_worker() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::uint32_t id = impl_->next_worker_id++;
    impl_->workers[id] = false;  // 初始空闲
    return id;
}

void CpuController::unregister_worker(std::uint32_t worker_id) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->workers.erase(worker_id);
}

void CpuController::mark_worker_active(std::uint32_t worker_id) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->workers.find(worker_id);
    if (it != impl_->workers.end()) {
        it->second = true;
    }
}

void CpuController::mark_worker_idle(std::uint32_t worker_id) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->workers.find(worker_id);
    if (it != impl_->workers.end()) {
        it->second = false;
    }
}

WorkerParticipation CpuController::worker_participation() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    WorkerParticipation p;
    p.registered_count = static_cast<std::uint32_t>(impl_->workers.size());
    p.worker_ids.reserve(impl_->workers.size());
    p.active_flags.reserve(impl_->workers.size());
    for (const auto& kv : impl_->workers) {
        p.worker_ids.push_back(kv.first);
        p.active_flags.push_back(kv.second);
        if (kv.second) ++p.active_count;
        else ++p.idle_count;
    }
    return p;
}

// ===== 实际利用率采样 + 决策 =====
CpuControlDecision CpuController::sample_and_decide() {
    // 读取实际 CPU 利用率（GetSystemTimes）
    CpuUtilizationSample sample = impl_->metrics.read_cpu_utilization();
    // 决策（CPU 实读，estimated=false）
    return impl_->decide_impl(sample.ratio, /*estimated=*/false, sample.valid);
}

CpuControlDecision CpuController::decide_with_actual(double actual_ratio) {
    // 注入接口，标记 estimated=true（用于测试/受限平台）
    return impl_->decide_impl(actual_ratio, /*estimated=*/true, /*valid=*/true);
}

// ===== 取消与响应 =====
void CpuController::request_cancel() noexcept {
    impl_->cancelled.store(true, std::memory_order_release);
}

bool CpuController::cancelled() const noexcept {
    return impl_->cancelled.load(std::memory_order_acquire);
}

void CpuController::clear_cancel() noexcept {
    impl_->cancelled.store(false, std::memory_order_release);
}

// ===== 状态 JSON =====
std::string CpuController::status_json() const {
    std::ostringstream os;
    WorkerParticipation p = worker_participation();
    os << "{";
    os << "\"target\":" << impl_->target.load(std::memory_order_relaxed);
    os << ",\"strategy\":" << impl_->strategy.load(std::memory_order_relaxed);
    os << ",\"window_ms\":" << impl_->window_ms.load(std::memory_order_relaxed);
    os << ",\"cancelled\":" << (impl_->cancelled.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"last_actual\":" << impl_->last_actual.load(std::memory_order_relaxed);
    os << ",\"last_error\":" << impl_->last_error.load(std::memory_order_relaxed);
    os << ",\"last_valid\":" << (impl_->last_valid.load(std::memory_order_relaxed) ? "true" : "false");
    os << ",\"worker_registered\":" << p.registered_count;
    os << ",\"worker_active\":" << p.active_count;
    os << ",\"worker_idle\":" << p.idle_count;
    os << ",\"nvml_available\":" << (impl_->metrics.nvml_available() ? "true" : "false");
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

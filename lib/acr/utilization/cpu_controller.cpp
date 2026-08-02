// lib/acr/utilization/cpu_controller.cpp — CpuController 实现
#include "cpu_controller.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <thread>

namespace astro::compute::utilization {

namespace {
constexpr double kMinTarget = 0.0;
constexpr double kMaxTarget = 1.0;
constexpr double kDefaultTarget = 0.95;
} // anonymous namespace

struct CpuController::Impl {
    std::mutex mtx;
    std::atomic<double> target{kDefaultTarget};
    std::atomic<int> strategy{static_cast<int>(ControlStrategy::BatchSize)};
    mutable std::atomic<double> last_actual{0.0};
    mutable std::atomic<double> last_error{0.0};

    CpuControlDecision decide_impl(double actual_ratio) const {
        CpuControlDecision d;
        d.target_ratio = target.load(std::memory_order_relaxed);
        d.actual_ratio = actual_ratio;
        d.error_ratio = actual_ratio - d.target_ratio;
        last_actual.store(actual_ratio, std::memory_order_relaxed);
        last_error.store(d.error_ratio, std::memory_order_relaxed);

        // 控制策略实现：
        // - actual > target + 0.05：需要降低利用率（更小批次 / 更深队列 / 让步）
        // - actual < target - 0.05：可以增加利用率（更大批次 / 浅队列 / 不让步）
        // - 误差 < 0.05：保持当前
        const double tolerance = 0.05;
        bool too_high = actual_ratio > d.target_ratio + tolerance;
        bool too_low = actual_ratio < d.target_ratio - tolerance;

        switch (static_cast<ControlStrategy>(strategy.load(std::memory_order_relaxed))) {
            case ControlStrategy::BatchSize:
                if (too_high) d.batch_size = 1;          // 最小批次 → 频繁让步
                else if (too_low) d.batch_size = 8;      // 大批次 → 少让步
                else d.batch_size = 4;
                break;
            case ControlStrategy::QueueDepth:
                if (too_high) d.queue_depth = 1;         // 浅队列 → 减少积压
                else if (too_low) d.queue_depth = 4;     // 深队列 → 增加积压
                else d.queue_depth = 2;
                break;
            case ControlStrategy::Priority:
                if (too_high) d.priority = -1;           // 降优先级
                else if (too_low) d.priority = 1;        // 升优先级
                else d.priority = 0;
                break;
            case ControlStrategy::Yield:
                d.should_yield = too_high;
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

CpuControlDecision CpuController::decide(double actual_ratio) const {
    return impl_->decide_impl(actual_ratio);
}

std::string CpuController::status_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"target\":" << impl_->target.load(std::memory_order_relaxed);
    os << ",\"strategy\":" << impl_->strategy.load(std::memory_order_relaxed);
    os << ",\"last_actual\":" << impl_->last_actual.load(std::memory_order_relaxed);
    os << ",\"last_error\":" << impl_->last_error.load(std::memory_order_relaxed);
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

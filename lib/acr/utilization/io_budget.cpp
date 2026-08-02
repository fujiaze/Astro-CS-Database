// lib/acr/utilization/io_budget.cpp — IoBudgetController 实现
#include "io_budget.hpp"

#include <atomic>
#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

struct IoBudgetController::Impl {
    std::mutex mtx;
    IoBudgetConfig cfg;
    mutable std::atomic<double> last_actual{0.0};
};

IoBudgetController::IoBudgetController() : impl_(std::make_unique<Impl>()) {}
IoBudgetController::~IoBudgetController() = default;

void IoBudgetController::configure(const IoBudgetConfig& cfg) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg = cfg;
}

const IoBudgetConfig& IoBudgetController::config() const noexcept {
    return impl_->cfg;
}

IoBudgetDecision IoBudgetController::report(double actual_mbps) const {
    IoBudgetDecision d;
    d.budget_mbps = impl_->cfg.budget_mbps;
    d.actual_mbps = actual_mbps;
    impl_->last_actual.store(actual_mbps, std::memory_order_relaxed);
    if (impl_->cfg.budget_mbps > 0.0) {
        d.utilization_ratio = actual_mbps / impl_->cfg.budget_mbps;
        d.exceeded = d.utilization_ratio > 1.0;
        d.warn = d.utilization_ratio > impl_->cfg.warn_threshold;
    } else {
        // 不限速
        d.utilization_ratio = 0.0;
        d.exceeded = false;
        d.warn = false;
    }
    return d;
}

std::string IoBudgetController::status_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"budget_mbps\":" << impl_->cfg.budget_mbps;
    os << ",\"warn_threshold\":" << impl_->cfg.warn_threshold;
    os << ",\"last_actual_mbps\":" << impl_->last_actual.load(std::memory_order_relaxed);
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

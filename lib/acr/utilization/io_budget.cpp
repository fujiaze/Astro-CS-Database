// lib/acr/utilization/io_budget.cpp — IoBudgetController 实现
#include "io_budget.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

namespace {
inline std::uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
} // anonymous namespace

struct IoBudgetController::Impl {
    std::mutex mtx;
    IoBudgetConfig cfg;
    // 累计 I/O（自上次 sample() 重置）
    std::uint64_t window_bytes{0};
    std::uint64_t window_duration_ns{0};
    std::uint64_t window_count{0};
    std::uint64_t window_start_ns{0};
    bool window_has_data{false};
    // 全局累计（status_json 用）
    std::uint64_t total_bytes{0};
    std::uint64_t total_duration_ns{0};
    std::uint64_t total_count{0};
    // 上次采样吞吐量
    double last_actual_mbps{0.0};
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

void IoBudgetController::record_io(std::uint64_t bytes, std::uint64_t duration_ns) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->window_has_data) {
        impl_->window_start_ns = now_ns();
        impl_->window_has_data = true;
    }
    impl_->window_bytes += bytes;
    impl_->window_duration_ns += duration_ns;
    impl_->window_count += 1;
    impl_->total_bytes += bytes;
    impl_->total_duration_ns += duration_ns;
    impl_->total_count += 1;
}

IoBudgetDecision IoBudgetController::sample() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    IoBudgetDecision d;
    d.budget_mbps = impl_->cfg.budget_mbps;
    d.total_bytes = impl_->window_bytes;
    d.total_duration_ns = impl_->window_duration_ns;
    d.record_count = impl_->window_count;

    if (!impl_->window_has_data || impl_->window_count == 0) {
        d.valid = false;
        d.actual_mbps = 0.0;
    } else {
        // 实际吞吐量 = bytes / duration
        // 优先用累计 duration（I/O 实际耗时）
        double seconds = 0.0;
        if (impl_->window_duration_ns > 0) {
            seconds = static_cast<double>(impl_->window_duration_ns) / 1e9;
        } else {
            // 退化用 wall time
            std::uint64_t elapsed = now_ns() - impl_->window_start_ns;
            seconds = static_cast<double>(elapsed) / 1e9;
        }
        if (seconds > 0.0) {
            // Mbps = bytes / seconds / (1024*1024)
            d.actual_mbps = static_cast<double>(impl_->window_bytes) / seconds / (1024.0 * 1024.0);
        }
        d.valid = true;
        impl_->last_actual_mbps = d.actual_mbps;
    }

    if (impl_->cfg.budget_mbps > 0.0 && d.valid) {
        d.utilization_ratio = d.actual_mbps / impl_->cfg.budget_mbps;
        d.exceeded = d.utilization_ratio > 1.0;
        d.warn = d.utilization_ratio > impl_->cfg.warn_threshold;
    } else {
        d.utilization_ratio = 0.0;
        d.exceeded = false;
        d.warn = false;
    }

    // 重置窗口
    impl_->window_bytes = 0;
    impl_->window_duration_ns = 0;
    impl_->window_count = 0;
    impl_->window_has_data = false;

    return d;
}

IoBudgetDecision IoBudgetController::report(double actual_mbps) const {
    IoBudgetDecision d;
    d.budget_mbps = impl_->cfg.budget_mbps;
    d.actual_mbps = actual_mbps;
    d.valid = true;
    if (impl_->cfg.budget_mbps > 0.0) {
        d.utilization_ratio = actual_mbps / impl_->cfg.budget_mbps;
        d.exceeded = d.utilization_ratio > 1.0;
        d.warn = d.utilization_ratio > impl_->cfg.warn_threshold;
    } else {
        d.utilization_ratio = 0.0;
        d.exceeded = false;
        d.warn = false;
    }
    return d;
}

std::string IoBudgetController::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"budget_mbps\":" << impl_->cfg.budget_mbps;
    os << ",\"warn_threshold\":" << impl_->cfg.warn_threshold;
    os << ",\"last_actual_mbps\":" << impl_->last_actual_mbps;
    os << ",\"total_bytes\":" << impl_->total_bytes;
    os << ",\"total_duration_ns\":" << impl_->total_duration_ns;
    os << ",\"total_count\":" << impl_->total_count;
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

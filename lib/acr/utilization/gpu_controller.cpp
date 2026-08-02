// lib/acr/utilization/gpu_controller.cpp — GpuController 实现
#include "gpu_controller.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace astro::compute::utilization {

namespace {
constexpr double kDefaultTarget = 0.95;
constexpr double kMinTarget = 0.0;
constexpr double kMaxTarget = 1.0;
} // anonymous namespace

struct GpuController::Impl {
    mutable std::mutex mtx;
    std::atomic<double> target{kDefaultTarget};
    std::vector<std::string> backends;
    // 每个 backend 的最后观察值
    mutable std::unordered_map<std::string, double> last_actual;
    mutable std::unordered_map<std::string, double> last_error;
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

void GpuController::register_backend(const std::string& backend) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (std::find(impl_->backends.begin(), impl_->backends.end(), backend)
        == impl_->backends.end()) {
        impl_->backends.push_back(backend);
        impl_->last_actual[backend] = 0.0;
        impl_->last_error[backend] = 0.0;
    }
}

GpuControlDecision GpuController::decide(const std::string& backend, double actual_ratio) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    GpuControlDecision d;
    d.backend = backend;
    d.target_ratio = impl_->target.load(std::memory_order_relaxed);
    d.actual_ratio = actual_ratio;
    d.error_ratio = actual_ratio - d.target_ratio;
    impl_->last_actual[backend] = actual_ratio;
    impl_->last_error[backend] = d.error_ratio;

    const double tolerance = 0.05;
    bool too_high = actual_ratio > d.target_ratio + tolerance;
    bool too_low = actual_ratio < d.target_ratio - tolerance;

    if (too_high) {
        d.queue_depth = 1;     // 浅队列减少积压
        d.batch_size = 1;
        d.throttle = actual_ratio > d.target_ratio + 0.15;  // 严重超目标时节流
    } else if (too_low) {
        d.queue_depth = 4;     // 深队列增加积压
        d.batch_size = 4;
        d.throttle = false;
    } else {
        d.queue_depth = 2;
        d.batch_size = 2;
        d.throttle = false;
    }
    return d;
}

std::vector<std::string> GpuController::backends() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->backends;
}

std::string GpuController::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"target\":" << impl_->target.load(std::memory_order_relaxed);
    os << ",\"backends\":[";
    for (std::size_t i = 0; i < impl_->backends.size(); ++i) {
        if (i > 0) os << ",";
        const auto& b = impl_->backends[i];
        os << "{\"backend\":\"" << b << "\"";
        os << ",\"last_actual\":" << impl_->last_actual[b];
        os << ",\"last_error\":" << impl_->last_error[b];
        os << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace astro::compute::utilization

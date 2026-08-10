// lib/acr/utilization/actual_tracker.cpp — ActualTracker 实现
#include "actual_tracker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <sstream>
#include <vector>

namespace astro::compute::utilization {

namespace {
inline std::uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

// 计算 p95：排序后取第 95% 位置的值
double percentile(std::vector<double> sorted_vals, double p) {
    if (sorted_vals.empty()) return 0.0;
    std::sort(sorted_vals.begin(), sorted_vals.end());
    std::size_t n = sorted_vals.size();
    if (n == 1) return sorted_vals[0];
    // 线性插值
    double idx = p * static_cast<double>(n - 1);
    std::size_t lo = static_cast<std::size_t>(idx);
    std::size_t hi = std::min(lo + 1, n - 1);
    double frac = idx - static_cast<double>(lo);
    return sorted_vals[lo] * (1.0 - frac) + sorted_vals[hi] * frac;
}
} // anonymous namespace

struct ActualTracker::Impl {
    mutable std::mutex mtx;
    std::size_t capacity{1024};
    std::vector<UtilizationSample> samples;
    std::vector<WorkerParticipationEntry> worker_entries;
    std::atomic<std::size_t> total_count{0};
};

ActualTracker::ActualTracker() : impl_(std::make_unique<Impl>()) {}
ActualTracker::~ActualTracker() = default;

void ActualTracker::set_capacity(std::size_t cap) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (cap == 0) cap = 1;
    impl_->capacity = cap;
    if (impl_->samples.size() > cap) {
        impl_->samples.erase(impl_->samples.begin(),
                              impl_->samples.end() - static_cast<std::ptrdiff_t>(cap));
    }
    if (impl_->worker_entries.size() > cap) {
        impl_->worker_entries.erase(impl_->worker_entries.begin(),
                                     impl_->worker_entries.end() - static_cast<std::ptrdiff_t>(cap));
    }
}

std::size_t ActualTracker::capacity() const noexcept {
    return impl_->capacity;
}

void ActualTracker::record(const UtilizationSample& sample) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->samples.push_back(sample);
    if (impl_->samples.size() > impl_->capacity) {
        impl_->samples.erase(impl_->samples.begin());
    }
    impl_->total_count.fetch_add(1, std::memory_order_relaxed);
}

void ActualTracker::record_worker_participation(std::uint32_t registered,
                                                std::uint32_t active,
                                                std::uint32_t idle) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    WorkerParticipationEntry e;
    e.timestamp_ns = now_ns();
    e.registered = registered;
    e.active = active;
    e.idle = idle;
    impl_->worker_entries.push_back(e);
    if (impl_->worker_entries.size() > impl_->capacity) {
        impl_->worker_entries.erase(impl_->worker_entries.begin());
    }
}

std::vector<UtilizationSample> ActualTracker::recent(std::size_t n) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->samples.size() <= n) return impl_->samples;
    return std::vector<UtilizationSample>(impl_->samples.end() - static_cast<std::ptrdiff_t>(n),
                                           impl_->samples.end());
}

std::vector<UtilizationSample> ActualTracker::all() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->samples;
}

std::vector<WorkerParticipationEntry> ActualTracker::worker_history() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->worker_entries;
}

std::size_t ActualTracker::sample_count() const noexcept {
    return impl_->total_count.load(std::memory_order_relaxed);
}

UtilizationStats ActualTracker::stats() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    UtilizationStats s;
    s.sample_count = impl_->samples.size();
    if (impl_->samples.empty()) return s;

    std::vector<double> actuals;
    std::vector<double> errors;
    std::vector<double> abs_errors;
    actuals.reserve(impl_->samples.size());
    errors.reserve(impl_->samples.size());
    abs_errors.reserve(impl_->samples.size());

    double sum_actual = 0.0;
    double sum_error = 0.0;
    double max_actual = std::numeric_limits<double>::lowest();
    double min_actual = std::numeric_limits<double>::max();
    double max_error = std::numeric_limits<double>::lowest();
    double min_error = std::numeric_limits<double>::max();
    std::uint32_t estimated_count = 0;
    std::uint32_t cancelled_count = 0;

    for (const auto& smp : impl_->samples) {
        actuals.push_back(smp.actual_ratio);
        errors.push_back(smp.error_ratio);
        abs_errors.push_back(std::fabs(smp.error_ratio));
        sum_actual += smp.actual_ratio;
        sum_error += smp.error_ratio;
        if (smp.actual_ratio > max_actual) max_actual = smp.actual_ratio;
        if (smp.actual_ratio < min_actual) min_actual = smp.actual_ratio;
        if (smp.error_ratio > max_error) max_error = smp.error_ratio;
        if (smp.error_ratio < min_error) min_error = smp.error_ratio;
        if (smp.estimated) ++estimated_count;
        if (smp.cancelled) ++cancelled_count;
    }

    double n = static_cast<double>(s.sample_count);
    s.average_actual = sum_actual / n;
    s.average_error = sum_error / n;
    s.max_actual = max_actual;
    s.min_actual = min_actual;
    s.max_error = max_error;
    s.min_error = min_error;
    s.p95_actual = percentile(actuals, 0.95);
    s.average_p95_error = percentile(abs_errors, 0.95);
    s.estimated_count = estimated_count;
    s.cancelled_count = cancelled_count;
    return s;
}

double ActualTracker::average_error() const noexcept {
    auto s = stats();
    return s.average_error;
}

double ActualTracker::max_error() const noexcept {
    auto s = stats();
    return s.max_error;
}

double ActualTracker::min_error() const noexcept {
    auto s = stats();
    return s.min_error;
}

std::string ActualTracker::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"sample_count\":" << impl_->total_count.load(std::memory_order_relaxed);
    os << ",\"capacity\":" << impl_->capacity;
    os << ",\"in_memory\":" << impl_->samples.size();

    // 统计摘要（局部计算，避免递归锁）
    UtilizationStats s;
    if (!impl_->samples.empty()) {
        std::vector<double> actuals;
        std::vector<double> errors;
        std::vector<double> abs_errors;
        actuals.reserve(impl_->samples.size());
        errors.reserve(impl_->samples.size());
        abs_errors.reserve(impl_->samples.size());
        double sum_actual = 0.0, sum_error = 0.0;
        double max_actual = std::numeric_limits<double>::lowest();
        double min_actual = std::numeric_limits<double>::max();
        double max_error = std::numeric_limits<double>::lowest();
        double min_error = std::numeric_limits<double>::max();
        std::uint32_t est = 0, canc = 0;
        for (const auto& smp : impl_->samples) {
            actuals.push_back(smp.actual_ratio);
            errors.push_back(smp.error_ratio);
            abs_errors.push_back(std::fabs(smp.error_ratio));
            sum_actual += smp.actual_ratio;
            sum_error += smp.error_ratio;
            if (smp.actual_ratio > max_actual) max_actual = smp.actual_ratio;
            if (smp.actual_ratio < min_actual) min_actual = smp.actual_ratio;
            if (smp.error_ratio > max_error) max_error = smp.error_ratio;
            if (smp.error_ratio < min_error) min_error = smp.error_ratio;
            if (smp.estimated) ++est;
            if (smp.cancelled) ++canc;
        }
        double n = static_cast<double>(impl_->samples.size());
        s.sample_count = impl_->samples.size();
        s.average_actual = sum_actual / n;
        s.average_error = sum_error / n;
        s.max_actual = max_actual;
        s.min_actual = min_actual;
        s.max_error = max_error;
        s.min_error = min_error;
        s.p95_actual = percentile(actuals, 0.95);
        s.average_p95_error = percentile(abs_errors, 0.95);
        s.estimated_count = est;
        s.cancelled_count = canc;
    }

    os << ",\"stats\":{";
    os << "\"average_actual\":" << s.average_actual;
    os << ",\"p95_actual\":" << s.p95_actual;
    os << ",\"max_actual\":" << s.max_actual;
    os << ",\"min_actual\":" << s.min_actual;
    os << ",\"average_error\":" << s.average_error;
    os << ",\"max_error\":" << s.max_error;
    os << ",\"min_error\":" << s.min_error;
    os << ",\"p95_abs_error\":" << s.average_p95_error;
    os << ",\"estimated_count\":" << s.estimated_count;
    os << ",\"cancelled_count\":" << s.cancelled_count;
    os << "}";

    os << ",\"recent\":[";
    std::size_t show = std::min<std::size_t>(impl_->samples.size(), 10);
    for (std::size_t i = 0; i < show; ++i) {
        if (i > 0) os << ",";
        const auto& smp = impl_->samples[impl_->samples.size() - show + i];
        os << "{\"timestamp_ns\":" << smp.timestamp_ns;
        os << ",\"actual\":" << smp.actual_ratio;
        os << ",\"target\":" << smp.target_ratio;
        os << ",\"error\":" << smp.error_ratio;
        os << ",\"estimated\":" << (smp.estimated ? "true" : "false");
        os << ",\"cancelled\":" << (smp.cancelled ? "true" : "false");
        os << ",\"backend\":\"" << smp.backend << "\"";
        os << ",\"worker_registered\":" << smp.worker_registered;
        os << ",\"worker_active\":" << smp.worker_active;
        os << ",\"worker_idle\":" << smp.worker_idle;
        os << "}";
    }
    os << "]";

    // worker 参与历史最近 5 条
    os << ",\"worker_history\":[";
    std::size_t wshow = std::min<std::size_t>(impl_->worker_entries.size(), 5);
    for (std::size_t i = 0; i < wshow; ++i) {
        if (i > 0) os << ",";
        const auto& w = impl_->worker_entries[impl_->worker_entries.size() - wshow + i];
        os << "{\"timestamp_ns\":" << w.timestamp_ns;
        os << ",\"registered\":" << w.registered;
        os << ",\"active\":" << w.active;
        os << ",\"idle\":" << w.idle;
        os << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace astro::compute::utilization

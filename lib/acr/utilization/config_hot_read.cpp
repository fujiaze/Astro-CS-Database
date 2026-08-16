// lib/acr/utilization/config_hot_read.cpp — ConfigHotReader 实现
#include "config_hot_read.hpp"

#include <atomic>
#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

struct ConfigHotReader::Impl {
    mutable std::mutex mtx;
    HotConfig cfg;
    std::atomic<bool> initialized{false};
    std::atomic<bool> cold_frozen{false};

    // HotMutable 用 atomic 加速热读（26：仅内存容量相关）
    std::atomic<double> ram_ratio{0.95};
    std::atomic<double> vram_ratio{0.95};
    std::atomic<std::uint64_t> ram_reserve{2048ULL * 1024 * 1024};
    std::atomic<std::uint64_t> vram_reserve{512ULL * 1024 * 1024};
    std::atomic<int> fallback_policy{static_cast<int>(FallbackPolicy::BestEffort)};
};

ConfigHotReader::ConfigHotReader() : impl_(std::make_unique<Impl>()) {}
ConfigHotReader::~ConfigHotReader() = default;

void ConfigHotReader::init(const HotConfig& cfg) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg = cfg;
    impl_->ram_ratio.store(cfg.ram_ratio, std::memory_order_relaxed);
    impl_->vram_ratio.store(cfg.vram_ratio, std::memory_order_relaxed);
    impl_->ram_reserve.store(cfg.ram_fixed_reserve_bytes, std::memory_order_relaxed);
    impl_->vram_reserve.store(cfg.vram_fixed_reserve_bytes, std::memory_order_relaxed);
    impl_->fallback_policy.store(static_cast<int>(cfg.fallback_policy), std::memory_order_relaxed);
    impl_->initialized.store(true, std::memory_order_release);
    impl_->cold_frozen.store(true, std::memory_order_release);
}

void ConfigHotReader::update_hot(const HotConfig& cfg) {
    if (!impl_->initialized.load(std::memory_order_acquire)) {
        init(cfg);
        return;
    }
    std::lock_guard<std::mutex> lk(impl_->mtx);
    // 仅更新 HotMutable 项
    impl_->cfg.ram_ratio = cfg.ram_ratio;
    impl_->cfg.vram_ratio = cfg.vram_ratio;
    impl_->cfg.ram_fixed_reserve_bytes = cfg.ram_fixed_reserve_bytes;
    impl_->cfg.vram_fixed_reserve_bytes = cfg.vram_fixed_reserve_bytes;
    impl_->cfg.fallback_policy = cfg.fallback_policy;
    // backend_enabled 合并（新增/更新）
    for (const auto& kv : cfg.backend_enabled) {
        impl_->cfg.backend_enabled[kv.first] = kv.second;
    }
    impl_->ram_ratio.store(cfg.ram_ratio, std::memory_order_relaxed);
    impl_->vram_ratio.store(cfg.vram_ratio, std::memory_order_relaxed);
    impl_->ram_reserve.store(cfg.ram_fixed_reserve_bytes, std::memory_order_relaxed);
    impl_->vram_reserve.store(cfg.vram_fixed_reserve_bytes, std::memory_order_relaxed);
    impl_->fallback_policy.store(static_cast<int>(cfg.fallback_policy), std::memory_order_relaxed);
}

HotConfig ConfigHotReader::read() const {
    if (!impl_->initialized.load(std::memory_order_acquire)) {
        return HotConfig{};
    }
    HotConfig out;
    out.ram_ratio = impl_->ram_ratio.load(std::memory_order_relaxed);
    out.vram_ratio = impl_->vram_ratio.load(std::memory_order_relaxed);
    out.ram_fixed_reserve_bytes = impl_->ram_reserve.load(std::memory_order_relaxed);
    out.vram_fixed_reserve_bytes = impl_->vram_reserve.load(std::memory_order_relaxed);
    out.fallback_policy = static_cast<FallbackPolicy>(impl_->fallback_policy.load(std::memory_order_relaxed));
    // ColdStatic + backend_enabled 从 mtx 保护下读取
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        out.max_threads = impl_->cfg.max_threads;
        out.gpu_backend = impl_->cfg.gpu_backend;
        out.isa_level = impl_->cfg.isa_level;
        out.backend_enabled = impl_->cfg.backend_enabled;
    }
    return out;
}

void ConfigHotReader::set_ram_ratio(double ratio) noexcept {
    impl_->ram_ratio.store(ratio, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.ram_ratio = ratio;
}

void ConfigHotReader::set_vram_ratio(double ratio) noexcept {
    impl_->vram_ratio.store(ratio, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.vram_ratio = ratio;
}

void ConfigHotReader::set_ram_reserve(std::uint64_t bytes) noexcept {
    impl_->ram_reserve.store(bytes, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.ram_fixed_reserve_bytes = bytes;
}

void ConfigHotReader::set_vram_reserve(std::uint64_t bytes) noexcept {
    impl_->vram_reserve.store(bytes, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.vram_fixed_reserve_bytes = bytes;
}

void ConfigHotReader::set_fallback_policy(FallbackPolicy policy) noexcept {
    impl_->fallback_policy.store(static_cast<int>(policy), std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.fallback_policy = policy;
}

void ConfigHotReader::set_backend_enabled(const std::string& backend, bool enabled) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.backend_enabled[backend] = enabled;
}

bool ConfigHotReader::is_backend_enabled(const std::string& backend) const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    auto it = impl_->cfg.backend_enabled.find(backend);
    if (it == impl_->cfg.backend_enabled.end()) {
        return true;  // 未配置默认启用
    }
    return it->second;
}

bool ConfigHotReader::initialized() const noexcept {
    return impl_->initialized.load(std::memory_order_acquire);
}

std::vector<std::string> ConfigHotReader::configured_backends() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::vector<std::string> out;
    out.reserve(impl_->cfg.backend_enabled.size());
    for (const auto& kv : impl_->cfg.backend_enabled) {
        out.push_back(kv.first);
    }
    return out;
}

std::string ConfigHotReader::status_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"initialized\":" << (impl_->initialized.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"ram_ratio\":" << impl_->ram_ratio.load(std::memory_order_relaxed);
    os << ",\"vram_ratio\":" << impl_->vram_ratio.load(std::memory_order_relaxed);
    os << ",\"ram_reserve\":" << impl_->ram_reserve.load(std::memory_order_relaxed);
    os << ",\"vram_reserve\":" << impl_->vram_reserve.load(std::memory_order_relaxed);
    os << ",\"fallback_policy\":" << impl_->fallback_policy.load(std::memory_order_relaxed);
    os << ",\"backends\":{";
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        bool first = true;
        for (const auto& kv : impl_->cfg.backend_enabled) {
            if (!first) os << ",";
            first = false;
            os << "\"" << kv.first << "\":" << (kv.second ? "true" : "false");
        }
    }
    os << "}}";
    return os.str();
}

} // namespace astro::compute::utilization

// lib/acr/utilization/config_hot_read.cpp — ConfigHotReader 实现
#include "config_hot_read.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

struct ConfigHotReader::Impl {
    mutable std::mutex mtx;
    HotConfig cfg;
    std::atomic<bool> initialized{false};
    std::atomic<bool> cold_frozen{false};

    // HotMutable 用 atomic 加速热读
    std::atomic<double> cpu_target{0.95};
    std::atomic<double> gpu_target{0.95};
    std::atomic<double> ram_ratio{0.9};
    std::atomic<double> vram_ratio{0.9};
    std::atomic<std::uint64_t> mem_reserve{512ULL * 1024 * 1024};
    std::atomic<double> io_budget{0.0};
};

ConfigHotReader::ConfigHotReader() : impl_(std::make_unique<Impl>()) {}
ConfigHotReader::~ConfigHotReader() = default;

void ConfigHotReader::init(const HotConfig& cfg) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg = cfg;
    impl_->cpu_target.store(cfg.cpu_target_ratio, std::memory_order_relaxed);
    impl_->gpu_target.store(cfg.gpu_target_ratio, std::memory_order_relaxed);
    impl_->ram_ratio.store(cfg.ram_ratio, std::memory_order_relaxed);
    impl_->vram_ratio.store(cfg.vram_ratio, std::memory_order_relaxed);
    impl_->mem_reserve.store(cfg.memory_fixed_reserve_bytes, std::memory_order_relaxed);
    impl_->io_budget.store(cfg.io_budget_mbps, std::memory_order_relaxed);
    impl_->initialized.store(true, std::memory_order_release);
    impl_->cold_frozen.store(true, std::memory_order_release);
}

void ConfigHotReader::update_hot(const HotConfig& cfg) {
    if (!impl_->initialized.load(std::memory_order_acquire)) {
        // 未 init，直接 init
        init(cfg);
        return;
    }
    std::lock_guard<std::mutex> lk(impl_->mtx);
    // 仅更新 HotMutable 项
    impl_->cfg.cpu_target_ratio = cfg.cpu_target_ratio;
    impl_->cfg.gpu_target_ratio = cfg.gpu_target_ratio;
    impl_->cfg.ram_ratio = cfg.ram_ratio;
    impl_->cfg.vram_ratio = cfg.vram_ratio;
    impl_->cfg.memory_fixed_reserve_bytes = cfg.memory_fixed_reserve_bytes;
    impl_->cfg.io_budget_mbps = cfg.io_budget_mbps;
    impl_->cpu_target.store(cfg.cpu_target_ratio, std::memory_order_relaxed);
    impl_->gpu_target.store(cfg.gpu_target_ratio, std::memory_order_relaxed);
    impl_->ram_ratio.store(cfg.ram_ratio, std::memory_order_relaxed);
    impl_->vram_ratio.store(cfg.vram_ratio, std::memory_order_relaxed);
    impl_->mem_reserve.store(cfg.memory_fixed_reserve_bytes, std::memory_order_relaxed);
    impl_->io_budget.store(cfg.io_budget_mbps, std::memory_order_relaxed);
}

HotConfig ConfigHotReader::read() const {
    if (!impl_->initialized.load(std::memory_order_acquire)) {
        return HotConfig{};
    }
    HotConfig out;
    out.cpu_target_ratio = impl_->cpu_target.load(std::memory_order_relaxed);
    out.gpu_target_ratio = impl_->gpu_target.load(std::memory_order_relaxed);
    out.ram_ratio = impl_->ram_ratio.load(std::memory_order_relaxed);
    out.vram_ratio = impl_->vram_ratio.load(std::memory_order_relaxed);
    out.memory_fixed_reserve_bytes = impl_->mem_reserve.load(std::memory_order_relaxed);
    out.io_budget_mbps = impl_->io_budget.load(std::memory_order_relaxed);
    // ColdStatic 项从 mtx 保护下读取
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        out.max_threads = impl_->cfg.max_threads;
        out.gpu_backend = impl_->cfg.gpu_backend;
    }
    return out;
}

void ConfigHotReader::set_cpu_target(double ratio) noexcept {
    impl_->cpu_target.store(ratio, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.cpu_target_ratio = ratio;
}

void ConfigHotReader::set_gpu_target(double ratio) noexcept {
    impl_->gpu_target.store(ratio, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.gpu_target_ratio = ratio;
}

void ConfigHotReader::set_ram_ratio(double ratio) noexcept {
    impl_->ram_ratio.store(ratio, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.ram_ratio = ratio;
}

void ConfigHotReader::set_io_budget(double mbps) noexcept {
    impl_->io_budget.store(mbps, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg.io_budget_mbps = mbps;
}

bool ConfigHotReader::initialized() const noexcept {
    return impl_->initialized.load(std::memory_order_acquire);
}

std::string ConfigHotReader::status_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"initialized\":" << (impl_->initialized.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"cpu_target\":" << impl_->cpu_target.load(std::memory_order_relaxed);
    os << ",\"gpu_target\":" << impl_->gpu_target.load(std::memory_order_relaxed);
    os << ",\"ram_ratio\":" << impl_->ram_ratio.load(std::memory_order_relaxed);
    os << ",\"vram_ratio\":" << impl_->vram_ratio.load(std::memory_order_relaxed);
    os << ",\"mem_reserve\":" << impl_->mem_reserve.load(std::memory_order_relaxed);
    os << ",\"io_budget\":" << impl_->io_budget.load(std::memory_order_relaxed);
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

// lib/acr/utilization/memory_budget.cpp — MemoryBudgetController 实现
#include "memory_budget.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

struct MemoryBudgetController::Impl {
    std::mutex mtx;
    MemoryBudgetConfig cfg;
    std::atomic<std::uint64_t> total_ram{0};
    std::atomic<std::uint64_t> total_vram{0};
};

MemoryBudgetController::MemoryBudgetController()
    : impl_(std::make_unique<Impl>()) {}
MemoryBudgetController::~MemoryBudgetController() = default;

void MemoryBudgetController::configure(const MemoryBudgetConfig& cfg) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cfg = cfg;
}

const MemoryBudgetConfig& MemoryBudgetController::config() const noexcept {
    return impl_->cfg;
}

void MemoryBudgetController::set_system_memory(std::uint64_t total_ram,
                                                std::uint64_t total_vram) noexcept {
    impl_->total_ram.store(total_ram, std::memory_order_relaxed);
    impl_->total_vram.store(total_vram, std::memory_order_relaxed);
}

std::uint64_t MemoryBudgetController::compute_limit(std::uint64_t total, double ratio,
                                                     std::uint64_t fixed_reserve) noexcept {
    if (total == 0) return 0;
    std::uint64_t by_ratio = static_cast<std::uint64_t>(total * ratio);
    std::uint64_t by_reserve = (total > fixed_reserve) ? (total - fixed_reserve) : 0;
    return std::min(by_ratio, by_reserve);
}

MemoryBudget MemoryBudgetController::report(std::uint64_t used_ram, std::uint64_t used_vram) const {
    MemoryBudget m;
    m.total_ram = impl_->total_ram.load(std::memory_order_relaxed);
    m.total_vram = impl_->total_vram.load(std::memory_order_relaxed);
    m.limit_ram = compute_limit(m.total_ram, impl_->cfg.ram_ratio, impl_->cfg.fixed_reserve_bytes);
    m.limit_vram = compute_limit(m.total_vram, impl_->cfg.vram_ratio, impl_->cfg.fixed_reserve_bytes);
    m.used_ram = used_ram;
    m.used_vram = used_vram;
    m.ram_exceeded = (used_ram > m.limit_ram);
    m.vram_exceeded = (used_vram > m.limit_vram);
    return m;
}

std::string MemoryBudgetController::status_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"ram_ratio\":" << impl_->cfg.ram_ratio;
    os << ",\"vram_ratio\":" << impl_->cfg.vram_ratio;
    os << ",\"fixed_reserve_bytes\":" << impl_->cfg.fixed_reserve_bytes;
    os << ",\"total_ram\":" << impl_->total_ram.load(std::memory_order_relaxed);
    os << ",\"total_vram\":" << impl_->total_vram.load(std::memory_order_relaxed);
    os << ",\"limit_ram\":" << compute_limit(impl_->total_ram.load(std::memory_order_relaxed),
                                              impl_->cfg.ram_ratio, impl_->cfg.fixed_reserve_bytes);
    os << ",\"limit_vram\":" << compute_limit(impl_->total_vram.load(std::memory_order_relaxed),
                                               impl_->cfg.vram_ratio, impl_->cfg.fixed_reserve_bytes);
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

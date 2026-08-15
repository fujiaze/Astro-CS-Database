// lib/acr/utilization/memory_budget.cpp — MemoryBudgetController 实现
//
// Phase G：真实 RAM/VRAM 读取。
// - RAM: GlobalMemoryStatusEx（total/avail），used = total - avail
// - VRAM: NVML nvmlDeviceGetMemoryInfo；无 NVML 时 estimated=true
// - limit = min(total*ratio, total-fixed_reserve)
// - 达到上限时 suggest_action 给出降级路径
#include "memory_budget.hpp"

#include "system_metrics.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <sstream>

namespace astro::compute::utilization {

struct MemoryBudgetController::Impl {
    mutable std::mutex mtx;
    MemoryBudgetConfig cfg;
    SystemMetrics metrics;
    std::vector<std::string> backends;
    // 上次采样的系统总量（report_with 注入时若 total=0 用此值）
    std::uint64_t last_total_ram{0};
    std::uint64_t last_total_vram{0};
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

void MemoryBudgetController::register_backend(const std::string& backend) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (std::find(impl_->backends.begin(), impl_->backends.end(), backend)
        == impl_->backends.end()) {
        impl_->backends.push_back(backend);
    }
    impl_->metrics.register_backend(backend);
}

std::vector<std::string> MemoryBudgetController::backends() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->backends;
}

std::uint64_t MemoryBudgetController::compute_limit(std::uint64_t total, double ratio,
                                                     std::uint64_t fixed_reserve) noexcept {
    if (total == 0) return 0;
    double r = ratio;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    std::uint64_t by_ratio = static_cast<std::uint64_t>(static_cast<double>(total) * r);
    std::uint64_t by_reserve = (total > fixed_reserve) ? (total - fixed_reserve) : 0;
    return std::min(by_ratio, by_reserve);
}

MemoryBudgetController::ExceedAction MemoryBudgetController::suggest_action(
    std::uint64_t used, std::uint64_t limit, std::uint64_t total) noexcept {
    if (limit == 0) return ExceedAction::Fail;
    if (used <= limit) return ExceedAction::None;
    // 超限程度
    double over_ratio = static_cast<double>(used - limit) / static_cast<double>(limit);
    if (over_ratio < 0.05) {
        // 轻微超限：停止新提交
        return ExceedAction::StopNewSubmit;
    } else if (over_ratio < 0.15) {
        // 中度：缩小块 + 释放缓存
        return ExceedAction::ShrinkBlock;
    } else if (over_ratio < 0.30) {
        // 较重：释放缓存 + 低内存路径
        return ExceedAction::ReleaseCache;
    } else if (over_ratio < 0.50) {
        // 严重：低内存路径 + 回退其他设备
        return ExceedAction::LowMemoryPath;
    } else if (total > 0 && used >= total) {
        // 用满：明确失败
        return ExceedAction::Fail;
    } else {
        // 极重：回退其他设备
        return ExceedAction::FallbackOtherDevice;
    }
}

MemoryBudget MemoryBudgetController::sample() {
    MemoryBudget m;
    // RAM
    MemorySample ram = impl_->metrics.read_ram();
    m.total_ram = ram.total_bytes;
    m.avail_ram = ram.avail_bytes;
    m.used_ram = (ram.total_bytes > ram.avail_bytes)
                 ? (ram.total_bytes - ram.avail_bytes) : 0;
    m.limit_ram = compute_limit(m.total_ram, impl_->cfg.ram_ratio,
                                impl_->cfg.ram_fixed_reserve_bytes);
    m.pinned_limit = compute_limit(m.total_ram, impl_->cfg.pinned_ratio,
                                   impl_->cfg.pinned_fixed_reserve_bytes);
    // pinned staging 是 RAM 的一部分；默认按 RAM 用量估算
    m.pinned_used = std::min(m.used_ram, m.pinned_limit);
    m.pinned_valid = ram.valid;
    m.ram_exceeded = (m.used_ram > m.limit_ram);
    m.ram_valid = ram.valid;

    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->last_total_ram = m.total_ram;
    }

    // VRAM（多 GPU）
    std::vector<GpuMemorySample> vrams = impl_->metrics.read_gpu_memories();
    for (const auto& v : vrams) {
        GpuMemoryBudget g;
        g.backend = v.backend;
        g.total_vram = v.total_bytes;
        g.used_vram = v.used_bytes;
        g.limit_vram = compute_limit(v.total_bytes, impl_->cfg.vram_ratio,
                                     impl_->cfg.vram_fixed_reserve_bytes);
        g.vram_exceeded = (v.valid && v.used_bytes > g.limit_vram);
        g.estimated = v.estimated;
        g.valid = v.valid;
        m.gpus.push_back(g);
        if (v.valid) {
            std::lock_guard<std::mutex> lk(impl_->mtx);
            impl_->last_total_vram = v.total_bytes;
        }
    }
    // 也为已注册但未在 vrams 中的 backend 补占位
    std::vector<std::string> registered = backends();
    for (const auto& b : registered) {
        bool found = false;
        for (const auto& g : m.gpus) {
            if (g.backend == b) { found = true; break; }
        }
        if (!found) {
            GpuMemoryBudget g;
            g.backend = b;
            g.estimated = true;
            g.valid = false;
            m.gpus.push_back(g);
        }
    }
    return m;
}

MemoryBudget MemoryBudgetController::report_with(std::uint64_t used_ram,
                                                  std::uint64_t used_vram,
                                                  const std::string& backend) {
    MemoryBudget m;
    // 用上次采样的系统总量，若无则尝试采样
    std::uint64_t total_ram = impl_->last_total_ram;
    std::uint64_t total_vram = impl_->last_total_vram;
    if (total_ram == 0) {
        MemorySample ram = impl_->metrics.read_ram();
        total_ram = ram.total_bytes;
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->last_total_ram = total_ram;
    }
    m.total_ram = total_ram;
    m.used_ram = used_ram;
    m.limit_ram = compute_limit(total_ram, impl_->cfg.ram_ratio,
                                impl_->cfg.ram_fixed_reserve_bytes);
    m.pinned_limit = compute_limit(total_ram, impl_->cfg.pinned_ratio,
                                   impl_->cfg.pinned_fixed_reserve_bytes);
    m.pinned_used = std::min(used_ram, m.pinned_limit);
    m.pinned_valid = true;
    m.ram_exceeded = (used_ram > m.limit_ram);
    m.ram_valid = true;

    // VRAM（单 backend 注入）
    GpuMemoryBudget g;
    g.backend = backend;
    g.total_vram = total_vram;
    g.used_vram = used_vram;
    g.limit_vram = compute_limit(total_vram, impl_->cfg.vram_ratio,
                                 impl_->cfg.vram_fixed_reserve_bytes);
    g.vram_exceeded = (used_vram > g.limit_vram);
    g.estimated = true;  // 注入接口标记估算
    g.valid = true;
    m.gpus.push_back(g);
    return m;
}

MemoryBudget MemoryBudgetController::report_pinned(
    std::uint64_t used_pinned, std::uint64_t total_ram_for_limit) {
    MemoryBudget m;
    std::uint64_t total = total_ram_for_limit;
    if (total == 0) {
        MemorySample ram = impl_->metrics.read_ram();
        total = ram.total_bytes;
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->last_total_ram = total;
    }
    m.total_ram = total;
    m.limit_ram = compute_limit(total, impl_->cfg.ram_ratio,
                                impl_->cfg.ram_fixed_reserve_bytes);
    m.pinned_limit = compute_limit(total, impl_->cfg.pinned_ratio,
                                   impl_->cfg.pinned_fixed_reserve_bytes);
    m.pinned_used = used_pinned;
    m.pinned_valid = true;
    m.pinned_exceeded = (used_pinned > m.pinned_limit);
    m.ram_valid = true;
    return m;
}

bool MemoryBudgetController::nvml_available() const noexcept {
    return impl_->metrics.nvml_available();
}

bool MemoryBudgetController::reload_nvml() {
    return impl_->metrics.reload_nvml();
}

std::string MemoryBudgetController::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"ram_ratio\":" << impl_->cfg.ram_ratio;
    os << ",\"vram_ratio\":" << impl_->cfg.vram_ratio;
    os << ",\"ram_fixed_reserve_bytes\":" << impl_->cfg.ram_fixed_reserve_bytes;
    os << ",\"vram_fixed_reserve_bytes\":" << impl_->cfg.vram_fixed_reserve_bytes;
    os << ",\"last_total_ram\":" << impl_->last_total_ram;
    os << ",\"last_total_vram\":" << impl_->last_total_vram;
    os << ",\"nvml_available\":" << (impl_->metrics.nvml_available() ? "true" : "false");
    os << ",\"registered_backends\":" << impl_->backends.size();
    os << "}";
    return os.str();
}

} // namespace astro::compute::utilization

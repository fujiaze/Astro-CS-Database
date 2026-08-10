// lib/acr/scheduler/fallback.cpp — FallbackPolicy 实现
#include "fallback.hpp"

#include <algorithm>

namespace astro::compute::scheduler {

FallbackPolicy::FallbackPolicy() = default;
FallbackPolicy::~FallbackPolicy() = default;

void FallbackPolicy::set_strategy(FallbackStrategy s) noexcept { strategy_ = s; }
FallbackStrategy FallbackPolicy::strategy() const noexcept { return strategy_; }

FallbackDecision FallbackPolicy::decide(
    const std::string& device_failed,
    const CoverageBitmap& bitmap,
    const std::vector<std::string>& available_backends) const {

    FallbackDecision d;
    d.skip_already_done = true;  // 不重放已完成的 chunk
    d.pending_chunks = bitmap.pending_indices();  // 未完成的 chunk（包括失败设备正在执行的）

    if (strategy_ == FallbackStrategy::None) {
        d.strategy = FallbackStrategy::None;
        return d;
    }
    if (strategy_ == FallbackStrategy::ToCpu) {
        d.strategy = FallbackStrategy::ToCpu;
        d.target_backend = "cpu";
        return d;
    }
    // ToNextDevice：优先选与失败设备同类型的其他设备
    // 例如 cuda:0 失败 → 优先选 cuda:1；如果失败的是 cpu 或无同类设备 → 回退 cpu
    auto is_same_type = [](const std::string& a, const std::string& b) {
        // 用 ":" 之前的前缀比较（"cuda:0" vs "cuda:1" 同类）
        auto pos_a = a.find(':');
        auto pos_b = b.find(':');
        std::string type_a = (pos_a == std::string::npos) ? a : a.substr(0, pos_a);
        std::string type_b = (pos_b == std::string::npos) ? b : b.substr(0, pos_b);
        return type_a == type_b;
    };
    // 第一轮：找同类非失败设备
    for (const auto& b : available_backends) {
        if (b == device_failed) continue;
        if (is_same_type(b, device_failed)) {
            d.strategy = FallbackStrategy::ToNextDevice;
            d.target_backend = b;
            return d;
        }
    }
    // 第二轮：找任何非失败设备（不同类型则降级为 ToCpu）
    for (const auto& b : available_backends) {
        if (b == device_failed) continue;
        // 不同类型设备的回退视为 ToCpu 策略（如 cuda 失败 → cpu 回退）
        d.strategy = (b == "cpu") ? FallbackStrategy::ToCpu : FallbackStrategy::ToNextDevice;
        d.target_backend = b;
        return d;
    }
    // 没有其他可用设备，回退 CPU
    d.strategy = FallbackStrategy::ToCpu;
    d.target_backend = "cpu";
    return d;
}

} // namespace astro::compute::scheduler

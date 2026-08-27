// lib/phase2/include/astro/phase2/execution_options.h — 全局执行预算唯一来源
//
// CON-002 global worker budget contract:
//   并行/IO/GPU 路由/确定性/内存预算的唯一 ExecutionOptions 对象。所有嵌套模块
//   只能从该预算借用，不得各自创建等规模线程池。
#pragma once

#include <cstdint>
#include <string>
#include <thread>

namespace astro::phase2 {

// 唯一执行预算对象。cpu_workers=0 表示 auto（=default_cpu_workers()）。
struct ExecutionOptions {
    int    cpu_workers = 0;         // 0 => max(1, hardware_concurrency)
    int    io_workers = 0;          // 0 => auto (cpu_workers/2, 至少 1)
    std::string gpu_route = "auto"; // "cpu" | "auto" | "cuda"
    bool   deterministic = true;    // 固定 seed/顺序/归并 ⇒ 可复现结果
    std::uint64_t memory_budget_bytes = 0; // 0 => 由配置 memory_limit_mb 决定
};

inline int default_cpu_workers() {
    const unsigned hc = std::thread::hardware_concurrency();
    return hc > 0 ? static_cast<int>(hc) : 1;
}

inline int effective_cpu_workers(const ExecutionOptions& e) {
    return e.cpu_workers > 0 ? e.cpu_workers : default_cpu_workers();
}

inline int effective_io_workers(const ExecutionOptions& e) {
    if (e.io_workers > 0) return e.io_workers;
    const int c = effective_cpu_workers(e);
    return c > 1 ? c / 2 : 1;
}

// 默认：cpu=max(1,hardware_concurrency)，io=cpu/2，route=auto，deterministic=true。
inline ExecutionOptions default_execution_options() {
    ExecutionOptions e;
    e.cpu_workers = default_cpu_workers();
    e.io_workers = effective_io_workers(e);
    e.gpu_route = "auto";
    e.deterministic = true;
    e.memory_budget_bytes = 0;
    return e;
}

} // namespace astro::phase2

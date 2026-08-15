// lib/acr/scheduler/mixed_runner.hpp — CPU+GPU 混合执行
// Phase F：CPU+单 GPU、CPU+多 GPU 执行。
//
// 设计：
// 1. 接受已分好的 chunks + per-chunk kernel
// 2. 按路由策略分发到 CPU/GPU backend
// 3. 用 CoverageBitmap 跟踪进度，保证不重复不遗漏
// 4. 失败时通过 FallbackPolicy 回退
// 5. 公共头不暴露第三方类型（GPU backend 通过字符串标识，不暴露 cuda 类型）
#pragma once

#include "fallback.hpp"
#include "partitioner.hpp"
#include "queue_aware.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== Per-chunk kernel 函数签名 =====
// 参数：chunk 索引、begin、end、user_data
using ChunkKernelFn = void(*)(std::size_t chunk_idx, std::size_t begin, std::size_t end, void* user_data);

// ===== 混合执行结果 =====
struct MixedRunResult {
    std::size_t total_chunks{0};
    std::size_t executed_on_cpu{0};
    std::size_t executed_on_gpu{0};
    std::size_t failed_chunks{0};
    std::size_t fallback_chunks{0};  // 回退到 CPU 的 chunk 数
    bool all_done{false};
    std::string error_message;
};

// ===== MixedRunner 配置 =====
struct MixedRunnerConfig {
    bool enable_gpu{false};                  // 是否启用 GPU（CPU-only 构建应 false）
    std::vector<std::string> gpu_backends;   // ["cuda:0", "cuda:1"] 等
    FallbackStrategy fallback_strategy{FallbackStrategy::ToCpu};
};

// ===== MixedRunner =====
// CPU+GPU 混合执行器（线程安全可重入）
class MixedRunner {
public:
    MixedRunner();
    ~MixedRunner();

    void configure(const MixedRunnerConfig& cfg);

    // 执行 range 拆分后的 chunks
    // chunk_size: 每块大小；fn: 每块 kernel
    MixedRunResult run_range(std::size_t begin, std::size_t end,
                             std::size_t chunk_size,
                             ChunkKernelFn fn, void* user_data);

    // 执行预拆分的 chunks
    MixedRunResult run_chunks(const std::vector<RangeChunk>& chunks,
                              ChunkKernelFn fn, void* user_data);

    // 上次运行的 coverage bitmap（只读引用）
    const CoverageBitmap& last_coverage() const noexcept;

    // 上次运行的统计
    const MixedRunResult& last_result() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::scheduler

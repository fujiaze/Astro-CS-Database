// lib/acr/scheduler/mixed_runner.cpp — MixedRunner 实现
// Phase F：CPU+GPU 混合执行（CPU-only 构建下 GPU 路径占位）
#include "mixed_runner.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "astro/compute/acr.hpp"

namespace astro::compute::scheduler {

namespace {

// per-chunk 执行包装数据
struct ChunkBatchData {
    ChunkKernelFn fn;
    void* user_data;
    const std::vector<RangeChunk>* chunks;
    CoverageBitmap* bitmap;
    std::atomic<std::size_t>* executed;
    std::atomic<std::size_t>* failed;
};

} // anonymous namespace

struct MixedRunner::Impl {
    MixedRunnerConfig cfg;
    CoverageBitmap last_bitmap{0};
    MixedRunResult last_result{};

    MixedRunResult run_impl(const std::vector<RangeChunk>& chunks,
                            ChunkKernelFn fn, void* user_data) {
        MixedRunResult r;
        r.total_chunks = chunks.size();
        last_bitmap = CoverageBitmap(chunks.size());

        if (chunks.empty()) {
            r.all_done = true;
            last_result = r;
            return r;
        }

        // 简化策略：
        // - enable_gpu=false 时全部走 CPU
        // - enable_gpu=true 时理论上应分发部分到 GPU；但 CUDA 集成待 Phase H
        //   此处全部走 CPU + 标记 fallback_chunks=total
        bool use_cpu = true;
        if (cfg.enable_gpu) {
            // GPU 路径占位（Phase H 接入真实 CUDA 后分发）
            use_cpu = true;
            r.fallback_chunks = chunks.size();
        }

        if (use_cpu) {
            // 通过 acr parallel_for 把每个 chunk 作为独立任务执行
            std::atomic<std::size_t> executed{0}, failed{0};
            // per-chunk 成功标记：每个 chunk 写入唯一位置（无冲突），
            // 避免 CoverageBitmap::mark_done 的非原子 read-modify-write 竞争。
            std::vector<char> success(chunks.size(), 0);
            // 用 parallel_batch（每个 chunk 一个 task）
            Event ev = astro::compute::parallel_batch(
                astro::compute::KernelId::Custom, chunks.size(),
                [&](std::size_t idx) {
                    const auto& c = chunks[idx];
                    try {
                        fn(c.index, c.begin, c.end, user_data);
                        success[idx] = 1;
                        executed.fetch_add(1, std::memory_order_relaxed);
                    } catch (...) {
                        failed.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            (void)ev;  // Phase B 同步执行，Event 已 ready

            r.executed_on_cpu = executed.load();
            r.failed_chunks = failed.load();

            // 在主线程顺序标记 bitmap（无并发，安全）
            for (std::size_t i = 0; i < chunks.size(); ++i) {
                if (success[i]) last_bitmap.mark_done(i);
            }
        }

        r.all_done = last_bitmap.all_done();
        last_result = r;
        return r;
    }
};

MixedRunner::MixedRunner() : impl_(std::make_unique<Impl>()) {}
MixedRunner::~MixedRunner() = default;

void MixedRunner::configure(const MixedRunnerConfig& cfg) {
    impl_->cfg = cfg;
}

MixedRunResult MixedRunner::run_range(std::size_t begin, std::size_t end,
                                       std::size_t chunk_size,
                                       ChunkKernelFn fn, void* user_data) {
    auto chunks = partition_range(begin, end, chunk_size);
    return run_chunks(chunks, fn, user_data);
}

MixedRunResult MixedRunner::run_chunks(const std::vector<RangeChunk>& chunks,
                                       ChunkKernelFn fn, void* user_data) {
    return impl_->run_impl(chunks, fn, user_data);
}

const CoverageBitmap& MixedRunner::last_coverage() const noexcept {
    return impl_->last_bitmap;
}

const MixedRunResult& MixedRunner::last_result() const noexcept {
    return impl_->last_result;
}

} // namespace astro::compute::scheduler

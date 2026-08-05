// lib/acr/qualification/focused/focused_operations.cpp — 聚焦目标合成 Operation 实现
#include "focused_operations.hpp"

#include "astro/compute/task_traits.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace astro::compute::qualification::focused {

namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t elapsed_ns(Clock::time_point t0, Clock::time_point t1) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
}

// 确定性 LCG（与 benchmark_common 一致）
struct Lcg {
    std::uint64_t state;
    explicit Lcg(std::uint64_t s) : state(s) {}
    std::uint64_t next() noexcept {
        state = 6364136223846793005ULL * state + 1442695040888963407ULL;
        return state;
    }
    float next_fp32() noexcept {
        std::uint64_t v = next();
        std::uint32_t bits = static_cast<std::uint32_t>(v >> 40) & 0xFFFFFF;
        return (static_cast<float>(bits) / static_cast<float>(0x1000000)) * 2.0f - 1.0f;
    }
};

std::size_t hash_bin(std::size_t i, std::size_t bins) noexcept {
    std::size_t h = i;
    h ^= h >> 17; h *= 0xed5ad4bbULL; h ^= h >> 11;
    return h % bins;
}

} // anonymous namespace

const char* focused_op_id(FocusedOp op) noexcept {
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
            return "synthetic.dense_pixel_accumulate.fp32";
        case FocusedOp::DenseAccumulateFp64Acc:
            return "synthetic.dense_pixel_accumulate.fp64acc";
        case FocusedOp::PixelReduceFp64Acc:
            return "synthetic.pixel_reduce.fp64acc";
        case FocusedOp::DrizzleScatterFp64Acc:
            return "synthetic.drizzle_like_scatter.fp64acc";
        case FocusedOp::ResidentChain:
            return "synthetic.resident_chain";
    }
    return "unknown";
}

std::size_t focused_input_bytes_per_item(FocusedOp op) noexcept {
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
        case FocusedOp::DenseAccumulateFp64Acc:
            return 2 * sizeof(float);  // x + y
        case FocusedOp::PixelReduceFp64Acc:
            return sizeof(float);
        case FocusedOp::DrizzleScatterFp64Acc:
            return sizeof(float);
        case FocusedOp::ResidentChain:
            return sizeof(float);
    }
    return sizeof(float);
}

std::size_t focused_output_bytes_per_item(FocusedOp op) noexcept {
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
        case FocusedOp::DenseAccumulateFp64Acc:
            return sizeof(float);
        case FocusedOp::PixelReduceFp64Acc:
            return 0;  // 归约输出为标量
        case FocusedOp::DrizzleScatterFp64Acc:
            return 0;  // 输出桶由 bins 决定
        case FocusedOp::ResidentChain:
            return sizeof(float);
    }
    return sizeof(float);
}

void fill_uniform_fp32(float* p, std::size_t n, std::uint64_t seed) {
    Lcg lcg(seed);
    for (std::size_t i = 0; i < n; ++i) p[i] = lcg.next_fp32();
}

std::uint64_t run_cpu_operation(FocusedOp op,
                                const std::vector<float>& x,
                                std::vector<float>& y,
                                std::vector<double>& partials,
                                std::size_t bins) {
    const std::size_t n = x.size();
    auto t0 = Clock::now();
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
            astro::compute::parallel_for(
                KernelId::Custom, Range1D{0, n},
                [&](std::size_t i) { y[i] += x[i]; });
            break;
        case FocusedOp::DenseAccumulateFp64Acc:
            astro::compute::parallel_for(
                KernelId::Custom, Range1D{0, n},
                [&](std::size_t i) {
                    y[i] = static_cast<float>(
                        static_cast<double>(y[i]) + static_cast<double>(x[i]));
                });
            break;
        case FocusedOp::PixelReduceFp64Acc: {
            const std::size_t chunk = 4096;
            const std::size_t max_slots = (n + chunk - 1) / chunk;
            std::vector<double> local(max_slots, 0.0);
            std::atomic<std::size_t> slots{0};
            astro::compute::parallel_chunks(
                KernelId::Custom, Range1D{0, n}, chunk,
                [&](std::size_t b, std::size_t e) {
                    double acc = 0.0;
                    for (std::size_t i = b; i < e; ++i) {
                        acc += static_cast<double>(x[i]);
                    }
                    const std::size_t slot =
                        slots.fetch_add(1, std::memory_order_relaxed);
                    if (slot < local.size()) local[slot] = acc;
                });
            double total = 0.0;
            for (double v : local) total += v;
            if (!partials.empty()) partials[0] = total;
            break;
        }
        case FocusedOp::DrizzleScatterFp64Acc: {
            // 每 chunk 独立局部桶 + 串行合并（避免多线程并发写共享桶）
            const std::size_t chunk = 4096;
            const std::size_t max_slots = (n + chunk - 1) / chunk;
            std::vector<std::vector<double>> locals(
                max_slots, std::vector<double>(bins, 0.0));
            std::atomic<std::size_t> slots{0};
            astro::compute::parallel_chunks(
                KernelId::Custom, Range1D{0, n}, chunk,
                [&](std::size_t b, std::size_t e) {
                    std::vector<double> local(bins, 0.0);
                    for (std::size_t i = b; i < e; ++i) {
                        local[hash_bin(i, bins)] += static_cast<double>(x[i]);
                    }
                    const std::size_t slot =
                        slots.fetch_add(1, std::memory_order_relaxed);
                    if (slot < locals.size()) {
                        locals[slot] = std::move(local);
                    }
                });
            for (const auto& l : locals) {
                for (std::size_t b = 0; b < bins; ++b) {
                    partials[b] += l[b];
                }
            }
            break;
        }
        case FocusedOp::ResidentChain:
            astro::compute::parallel_for(
                KernelId::Custom, Range1D{0, n},
                [&](std::size_t i) { y[i] = x[i] + 1.0f; });
            astro::compute::parallel_for(
                KernelId::Custom, Range1D{0, n},
                [&](std::size_t i) { y[i] = y[i] * 2.0f; });
            break;
    }
    auto t1 = Clock::now();
    return elapsed_ns(t0, t1);
}

bool run_gpu_operation(FocusedOp op,
                       const std::vector<float>& x,
                       std::vector<float>& y,
                       std::vector<double>& partials,
                       std::size_t bins,
                       std::uint64_t& elapsed_ns,
                       std::uint64_t& transfer_ns) {
    using namespace astro::compute::cuda::bridge;
    ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    if (!api.loaded() || !api.submit_dense_accumulate_fp64acc ||
        !api.submit_drizzle_scatter || !api.submit_chain ||
        !api.submit_launch_event) {
        return false;
    }
    static void* gpu_handle = nullptr;
    if (gpu_handle == nullptr) {
        const char* err = nullptr;
        if (api.init(&err) <= 0) return false;
        gpu_handle = api.executor_create(0, 65536, 256, &err);
        if (gpu_handle == nullptr) return false;
    }
    const std::size_t n = x.size();
    std::uint64_t el = 0, tr = 0;
    const char* err = nullptr;
    int rc = 1;
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
            // FP32 累加：等价于 axpy(alpha=1)
            rc = api.submit_axpy(gpu_handle, 0, n, y.data(), x.data(), 1.0f,
                                 &el, &err);
            break;
        case FocusedOp::DenseAccumulateFp64Acc:
            rc = api.submit_dense_accumulate_fp64acc(gpu_handle, 0, n,
                                                     y.data(), x.data(),
                                                     &el, &err);
            break;
        case FocusedOp::PixelReduceFp64Acc: {
            const std::size_t blocks = (n + 255) / 256;
            std::vector<double> host_partials(blocks, 0.0);
            rc = api.submit_reduce(gpu_handle, 0, n, x.data(),
                                   host_partials.data(), blocks, 0, &el, &err);
            if (rc == 0) {
                double total = 0.0;
                for (double v : host_partials) total += v;
                if (!partials.empty()) partials[0] = total;
            }
            break;
        }
        case FocusedOp::DrizzleScatterFp64Acc:
            rc = api.submit_drizzle_scatter(gpu_handle, 0, n, x.data(),
                                            partials.data(), bins, &el, &err);
            break;
        case FocusedOp::ResidentChain:
            rc = api.submit_chain(gpu_handle, 0, n, y.data(), x.data(),
                                  &el, &err);
            break;
    }
    if (rc != 0) return false;
    elapsed_ns = el;
    transfer_ns = tr;  // 当前桥接为同步语义，传输未单独拆分
    return true;
}

void reference_dense_accumulate(const std::vector<float>& x,
                                std::vector<float>& y,
                                bool fp64_acc) {
    const std::size_t n = x.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (fp64_acc) {
            y[i] = static_cast<float>(
                static_cast<double>(y[i]) + static_cast<double>(x[i]));
        } else {
            y[i] += x[i];
        }
    }
}

double reference_pixel_reduce(const std::vector<float>& x) {
    double acc = 0.0;
    for (float v : x) acc += static_cast<double>(v);
    return acc;
}

void reference_drizzle_scatter(const std::vector<float>& x,
                               std::vector<double>& bins_out,
                               std::size_t n_bins,
                               std::uint64_t seed) {
    (void)seed;
    bins_out.assign(n_bins, 0.0);
    for (std::size_t i = 0; i < x.size(); ++i) {
        bins_out[hash_bin(i, n_bins)] += static_cast<double>(x[i]);
    }
}

void reference_resident_chain(const std::vector<float>& x,
                              std::vector<float>& z) {
    const std::size_t n = x.size();
    for (std::size_t i = 0; i < n; ++i) z[i] = (x[i] + 1.0f) * 2.0f;
}

} // namespace astro::compute::qualification::focused

// lib/acr/qualification/focused/focused_operations.cpp — 聚焦目标合成 Operation 实现
#include "focused_operations.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
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

FocusedOp op_from_id(std::string_view id) noexcept {
    if (id == kOpDensePixelAccumulateFp32) return FocusedOp::DenseAccumulateFp32;
    if (id == kOpDensePixelAccumulateFp64Acc) return FocusedOp::DenseAccumulateFp64Acc;
    if (id == kOpPixelReduceFp64Acc) return FocusedOp::PixelReduceFp64Acc;
    if (id == kOpDrizzleLikeScatterFp64Acc) return FocusedOp::DrizzleScatterFp64Acc;
    return FocusedOp::ResidentChain;
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

bool run_gpu_operation_resident(FocusedOp op,
                                const std::vector<float>& x,
                                std::vector<float>& y,
                                std::vector<double>& partials,
                                std::size_t bins,
                                std::uint64_t& elapsed_ns,
                                std::uint64_t& transfer_ns) {
    using namespace astro::compute::cuda::bridge;
    ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    if (!api.loaded() || !api.upload_persistent ||
        !api.submit_dense_accumulate_resident ||
        !api.submit_reduce_resident ||
        !api.submit_drizzle_scatter_resident ||
        !api.submit_chain_resident) {
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
    // 上传一次（persistent），作为本次 resident 测量的传输
    if (api.upload_persistent(gpu_handle, 0, n, x.data(), &tr, &err) != 0) {
        return false;
    }
    int rc = 1;
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
        case FocusedOp::DenseAccumulateFp64Acc:
            rc = api.submit_dense_accumulate_resident(
                gpu_handle, 0, n, y.data(), &el, &err);
            break;
        case FocusedOp::PixelReduceFp64Acc: {
            const std::size_t blocks = (n + 255) / 256;
            std::vector<double> host_partials(blocks, 0.0);
            rc = api.submit_reduce_resident(
                gpu_handle, 0, n, host_partials.data(), blocks, 0, &el, &err);
            if (rc == 0) {
                double total = 0.0;
                for (double v : host_partials) total += v;
                if (!partials.empty()) partials[0] = total;
            }
            break;
        }
        case FocusedOp::DrizzleScatterFp64Acc:
            rc = api.submit_drizzle_scatter_resident(
                gpu_handle, 0, n, partials.data(), bins, &el, &err);
            break;
        case FocusedOp::ResidentChain:
            rc = api.submit_chain_resident(
                gpu_handle, 0, n, y.data(), &el, &err);
            break;
    }
    if (rc != 0) return false;
    elapsed_ns = el;
    transfer_ns = tr;
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

namespace {

// ===== KernelRegistry launcher（处理 chunk 子域）=====
void cpu_dense_launcher(const KernelInvocation& inv, void*, bool fp64_acc) {
    const BufferBinding* yb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    if (!yb || !xb) throw std::runtime_error("dense: missing buffers");
    float* y = static_cast<float*>(yb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        if (fp64_acc) {
            y[i] = static_cast<float>(
                static_cast<double>(y[i]) + static_cast<double>(x[i]));
        } else {
            y[i] += x[i];
        }
    }
}

void cpu_dense_fp32_launcher(const KernelInvocation& inv, void* ud) {
    cpu_dense_launcher(inv, ud, false);
}

void cpu_dense_fp64acc_launcher(const KernelInvocation& inv, void* ud) {
    cpu_dense_launcher(inv, ud, true);
}

void cpu_reduce_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* xb = inv.buffers.find(0);
    const BufferBinding* pb = inv.buffers.find(1);
    if (!xb || !pb) throw std::runtime_error("reduce: missing buffers");
    const float* x = static_cast<const float*>(xb->data);
    double* partials = static_cast<double*>(pb->data);
    double sum = 0.0;
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        sum += static_cast<double>(x[i]);
    }
    // attempt>0（重试）：清零槽位，避免重复累计（08 号计划 §4）
    if (inv.attempt > 0) partials[inv.token_id] = 0.0;
    partials[inv.token_id] += sum;  // 私有槽位（PrivatePartialThenMerge）
}

void cpu_drizzle_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* xb = inv.buffers.find(0);
    const BufferBinding* pb = inv.buffers.find(1);
    if (!xb || !pb) throw std::runtime_error("drizzle: missing buffers");
    auto bins = read_scalar<std::size_t>(inv.scalars, 0);
    if (!bins || *bins == 0) throw std::runtime_error("drizzle: missing bins");
    const float* x = static_cast<const float*>(xb->data);
    double* partials = static_cast<double*>(pb->data);
    // 每 token 私有桶：partials[token_id * bins + bin]（06 号规范 §3：
    // 禁止多个 CPU worker 并发写同一 bins 数组）
    double* local = partials + inv.token_id * (*bins);
    // attempt>0（重试）：清零本地桶，避免重复累计
    if (inv.attempt > 0) {
        std::memset(local, 0, (*bins) * sizeof(double));
    }
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        local[hash_bin(i, *bins)] += static_cast<double>(x[i]);
    }
}

void cpu_chain_launcher(const KernelInvocation& inv, void*) {
    const BufferBinding* zb = inv.buffers.find(0);
    const BufferBinding* xb = inv.buffers.find(1);
    if (!zb || !xb) throw std::runtime_error("chain: missing buffers");
    float* z = static_cast<float*>(zb->data);
    const float* x = static_cast<const float*>(xb->data);
    for (std::size_t i = inv.domain.begin; i < inv.domain.end; ++i) {
        z[i] = (x[i] + 1.0f) * 2.0f;
    }
}

// CUDA launcher（经桥接 TLS 句柄；桥接不可用时抛异常 → 如实 Failed）
void cuda_launcher(const KernelInvocation& inv, void*, FocusedOp op) {
    using namespace astro::compute::cuda::bridge;
    auto& api = astro::compute::cuda::bridge::api();
    if (!api.loaded()) throw std::runtime_error("cuda bridge not loaded");
    void* h = get_tls_handle();
    if (!h) throw std::runtime_error("no cuda handle");
    // 聚焦版 v3（08 号计划 §3）：输入已驻留 → resident 提交路径。
    // 不创建每 token host vector、不逐块 H2D；d_x 已在 worker 启动前
    // prefetch 整帧，桥接 resident 提交用 d_x + begin 复用。
    if (inv.input_resident) {
        std::uint64_t rel = 0;
        const char* rerr = nullptr;
        int rrc = 1;
        switch (op) {
            case FocusedOp::DenseAccumulateFp32:
            case FocusedOp::DenseAccumulateFp64Acc: {
                const BufferBinding* yb = inv.buffers.find(0);
                if (!yb) throw std::runtime_error("cuda: missing y");
                rrc = api.submit_dense_accumulate_resident(
                    h, inv.domain.begin, inv.domain.end,
                    static_cast<float*>(yb->data), &rel, &rerr);
                break;
            }
            case FocusedOp::PixelReduceFp64Acc: {
                const std::size_t blocks = (inv.domain.size() + 255) / 256;
                const BufferBinding* pb = inv.buffers.find(1);
                if (!pb) throw std::runtime_error("cuda: missing partials");
                std::vector<double> host_part(blocks, 0.0);
                rrc = api.submit_reduce_resident(
                    h, inv.domain.begin, inv.domain.end,
                    host_part.data(), blocks, 0, &rel, &rerr);
                if (rrc == 0) {
                    double sum = 0.0;
                    for (double v : host_part) sum += v;
                    double* slot =
                        static_cast<double*>(pb->data) + inv.token_id;
                    if (inv.attempt > 0) *slot = 0.0;  // 重试清零
                    *slot += sum;
                }
                break;
            }
            case FocusedOp::DrizzleScatterFp64Acc: {
                auto bins = read_scalar<std::size_t>(inv.scalars, 0);
                const std::size_t nb = bins ? *bins : 256;
                const BufferBinding* pb = inv.buffers.find(1);
                if (!pb) throw std::runtime_error("cuda: missing partials");
                rrc = api.submit_drizzle_scatter_resident(
                    h, inv.domain.begin, inv.domain.end,
                    static_cast<double*>(pb->data) + inv.token_id * nb,
                    nb, &rel, &rerr);
                break;
            }
            case FocusedOp::ResidentChain: {
                const BufferBinding* zb = inv.buffers.find(0);
                if (!zb) throw std::runtime_error("cuda: missing z");
                rrc = api.submit_chain_resident(
                    h, inv.domain.begin, inv.domain.end,
                    static_cast<float*>(zb->data), &rel, &rerr);
                break;
            }
        }
        set_tls_elapsed(rel);
        if (rrc != 0) {
            throw std::runtime_error(rerr ? rerr : "cuda resident kernel failed");
        }
        return;
    }
    std::vector<float> y(inv.domain.size(), 2.0f);
    std::vector<float> x(inv.domain.size());
    // buffer 布局（与 CPU launcher 一致）：
    //   dense/chain：buffer0=y 输出、buffer1=x 输入
    //   reduce/drizzle：buffer0=x 输入、buffer1=partials
    const bool input_is_buf1 =
        (op != FocusedOp::PixelReduceFp64Acc &&
         op != FocusedOp::DrizzleScatterFp64Acc);
    const BufferBinding* yb = inv.buffers.find(
        (op == FocusedOp::PixelReduceFp64Acc ||
         op == FocusedOp::DrizzleScatterFp64Acc) ? 0 : 0);
    const BufferBinding* xb = inv.buffers.find(input_is_buf1 ? 1 : 0);
    if (!yb || !xb) throw std::runtime_error("cuda: missing buffers");
    const float* xsrc = static_cast<const float*>(xb->data);
    const float* ysrc = (op == FocusedOp::PixelReduceFp64Acc ||
                         op == FocusedOp::DrizzleScatterFp64Acc)
        ? xsrc : static_cast<const float*>(yb->data);
    std::memcpy(y.data(), ysrc + inv.domain.begin,
                inv.domain.size() * sizeof(float));
    std::memcpy(x.data(), xsrc + inv.domain.begin,
                inv.domain.size() * sizeof(float));
    std::uint64_t el = 0;
    const char* err = nullptr;
    int rc = 1;
    switch (op) {
        case FocusedOp::DenseAccumulateFp32:
            rc = api.submit_axpy(h, 0, inv.domain.size(), y.data(), x.data(),
                                 1.0f, &el, &err);
            break;
        case FocusedOp::DenseAccumulateFp64Acc:
            rc = api.submit_dense_accumulate_fp64acc(
                h, 0, inv.domain.size(), y.data(), x.data(), &el, &err);
            break;
        case FocusedOp::PixelReduceFp64Acc: {
            const std::size_t blocks = (inv.domain.size() + 255) / 256;
            std::vector<double> partials(blocks, 0.0);
            rc = api.submit_reduce(h, 0, inv.domain.size(), x.data(),
                                   partials.data(), blocks, 0, &el, &err);
            if (rc == 0) {
                double sum = 0.0;
                for (double v : partials) sum += v;
                const BufferBinding* pb = inv.buffers.find(1);
                if (pb) {
                    double* slot =
                        static_cast<double*>(pb->data) + inv.token_id;
                    if (inv.attempt > 0) *slot = 0.0;  // 重试清零
                    *slot += sum;
                }
            }
            break;
        }
        case FocusedOp::DrizzleScatterFp64Acc: {
            auto bins = read_scalar<std::size_t>(inv.scalars, 0);
            const std::size_t nb = bins ? *bins : 256;
            const BufferBinding* pb = inv.buffers.find(1);
            if (!pb) throw std::runtime_error("cuda: missing partials");
            // 每 token 私有桶（与 CPU launcher 同布局：token_id * bins 偏移）
            double* part =
                static_cast<double*>(pb->data) + inv.token_id * nb;
            rc = api.submit_drizzle_scatter(
                h, 0, inv.domain.size(), x.data(), part, nb, &el, &err);
            break;
        }
        case FocusedOp::ResidentChain:
            rc = api.submit_chain(h, 0, inv.domain.size(), y.data(), x.data(),
                                  &el, &err);
            break;
    }
    if (rc != 0) {
        throw std::runtime_error(err ? err : "cuda kernel failed");
    }
    // 写回 y（dense/chain）
    if (op == FocusedOp::DenseAccumulateFp32 ||
        op == FocusedOp::DenseAccumulateFp64Acc ||
        op == FocusedOp::ResidentChain) {
        float* ydst = static_cast<float*>(yb->data);
        std::memcpy(ydst + inv.domain.begin, y.data(),
                    inv.domain.size() * sizeof(float));
    }
    set_tls_elapsed(el);
}

} // anonymous namespace

void register_focused_kernels() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        // CUDA launcher 必须是静态函数指针（不能捕获）
        auto cuda_dispatch = +[](const KernelInvocation& inv, void* ud) {
            cuda_launcher(inv, ud, op_from_id(inv.id));
        };
        auto reg = [](const char* id, CpuKernelLauncher cpu,
                      CudaKernelLauncher cuda,
                      std::size_t buf_count, std::size_t scalar_bytes) {
            KernelRegistration r;
            r.id = id;
            r.args.buffer_count = buf_count;
            r.args.scalar_bytes = scalar_bytes;
            r.cpu = cpu;
            r.cuda = cuda;
            global_kernel_registry().register_kernel(r);
        };
        reg("synthetic.dense_pixel_accumulate.fp32",
            &cpu_dense_fp32_launcher, cuda_dispatch, 2, 0);
        reg("synthetic.dense_pixel_accumulate.fp64acc",
            &cpu_dense_fp64acc_launcher, cuda_dispatch, 2, 0);
        reg("synthetic.pixel_reduce.fp64acc",
            &cpu_reduce_launcher, cuda_dispatch, 2, 0);
        reg("synthetic.drizzle_like_scatter.fp64acc",
            &cpu_drizzle_launcher, cuda_dispatch, 2, sizeof(std::size_t));
        reg("synthetic.resident_chain",
            &cpu_chain_launcher, cuda_dispatch, 2, 0);
    });
}

void merge_drizzle_partials(const double* token_partials,
                            std::size_t token_count,
                            std::size_t bins,
                            double* out) {
    for (std::size_t t = 0; t < token_count; ++t) {
        for (std::size_t b = 0; b < bins; ++b) {
            out[b] += token_partials[t * bins + b];
        }
    }
}

double merge_reduce_partials(const double* token_partials,
                             std::size_t token_count) {
    double total = 0.0;
    for (std::size_t t = 0; t < token_count; ++t) {
        total += token_partials[t];
    }
    return total;
}

std::size_t partial_slots_for(std::size_t work_size,
                              std::size_t min_chunk) {
    if (work_size == 0) return 1;
    if (min_chunk == 0) min_chunk = 1;
    return (work_size + min_chunk - 1) / min_chunk + 1;
}

} // namespace astro::compute::qualification::focused

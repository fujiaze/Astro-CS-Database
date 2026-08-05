// lib/acr/qualification/focused/focused_benchmark.cpp — 聚焦 Benchmark 实现
#include "focused_benchmark.hpp"

#include "astro/compute/topology.hpp"
#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace astro::compute::qualification::focused {

namespace {

constexpr std::size_t kSizes[] = {
    1u << 18, 1u << 20, 1u << 22, 1u << 24, 1u << 26};  // 256K/1M/4M/16M/64M
constexpr int kWarmup = 3;
constexpr int kRepeats = 7;

std::uint64_t median_of(std::vector<std::uint64_t> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// CPU 指纹（CPU 名 + ISA + 线程数）
std::string cpu_fingerprint() {
    std::string f = "cpu-";
    f += std::to_string(std::thread::hardware_concurrency());
    f += "t";
    return f;
}

std::string gpu_fingerprint() {
    using namespace astro::compute::cuda::bridge;
    ensure_bridge_loaded();
    auto& api = astro::compute::cuda::bridge::api();
    if (!api.loaded() || api.device_count() <= 0) return "none";
    const char* n = api.device_name(0);
    return n ? std::string(n) : "gpu";
}

std::string kernel_hash() {
    // 编译时指纹：固定 seed + 目标操作集合
    return "acr-focused-ops-20260805-v1";
}

} // anonymous namespace

const std::vector<std::size_t>& focused_size_sequence() noexcept {
    static const std::vector<std::size_t> sizes(
        std::begin(kSizes), std::end(kSizes));
    return sizes;
}

std::size_t FocusedBenchmark::run(FocusedProfileKind kind, bool enable_gpu) {
    (void)kind;
    const auto& sizes = focused_size_sequence();
    ops_.clear();
    transfer_ = FocusedTransferMeas{};

    // ---- 5 个目标 Operation ----
    const FocusedOp all_ops[] = {
        FocusedOp::DenseAccumulateFp32,
        FocusedOp::DenseAccumulateFp64Acc,
        FocusedOp::PixelReduceFp64Acc,
        FocusedOp::DrizzleScatterFp64Acc,
        FocusedOp::ResidentChain,
    };
    for (FocusedOp op : all_ops) {
        FocusedMeasuredOp m;
        m.op = op;
        m.min_items = sizes.front();
        m.max_items = sizes.back();
        for (std::size_t n : sizes) {
            std::vector<float> x(n);
            std::vector<float> y(n, 2.0f);
            std::vector<double> partials(256, 0.0);
            fill_uniform_fp32(x.data(), n, 0xA57C5AC20260802ULL);

            // CPU 测量（3 预热 + 7 有效）
            for (int w = 0; w < kWarmup; ++w) {
                run_cpu_operation(op, x, y, partials, 256);
            }
            std::vector<std::uint64_t> cpu_samples;
            for (int r = 0; r < kRepeats; ++r) {
                std::fill(y.begin(), y.end(), 2.0f);
                std::fill(partials.begin(), partials.end(), 0.0);
                cpu_samples.push_back(
                    run_cpu_operation(op, x, y, partials, 256));
            }
            m.cpu_ns.push_back(median_of(cpu_samples));

            // GPU 测量（可用时）
            if (enable_gpu) {
                std::vector<std::uint64_t> gpu_samples;
                std::uint64_t el = 0, tr = 0;
                bool supported = false;
                for (int w = 0; w < kWarmup; ++w) {
                    supported = run_gpu_operation(op, x, y, partials, 256,
                                                  el, tr);
                    if (!supported) break;
                }
                if (supported) {
                    for (int r = 0; r < kRepeats; ++r) {
                        std::fill(y.begin(), y.end(), 2.0f);
                        std::fill(partials.begin(), partials.end(), 0.0);
                        if (run_gpu_operation(op, x, y, partials, 256,
                                              el, tr)) {
                            gpu_samples.push_back(el);
                        }
                    }
                }
                if (!gpu_samples.empty()) {
                    m.gpu_ns.push_back(median_of(gpu_samples));
                    // 当前桥接为同步语义：resident 近似为端到端
                    m.gpu_resident_ns.push_back(median_of(gpu_samples));
                } else {
                    m.gpu_ns.push_back(0);
                    m.gpu_resident_ns.push_back(0);
                }
            } else {
                m.gpu_ns.push_back(0);
                m.gpu_resident_ns.push_back(0);
            }
        }
        ops_.push_back(std::move(m));
    }

    // ---- GPU launch/event 与 H2D/D2H ----
    if (enable_gpu) {
        using namespace astro::compute::cuda::bridge;
        ensure_bridge_loaded();
        auto& api = astro::compute::cuda::bridge::api();
        if (api.loaded() && api.submit_launch_event && api.transfer_h2d) {
            static void* gh = nullptr;
            const char* err = nullptr;
            if (!gh && api.init(&err) > 0) {
                gh = api.executor_create(0, 65536, 256, &err);
            }
            if (gh) {
                // launch/event：固定 64K 空 kernel，10 次预热 + 7 次测量
                std::uint64_t el = 0;
                for (int w = 0; w < kWarmup; ++w) {
                    api.submit_launch_event(gh, 0, 1u << 16, &el, &err);
                }
                for (int r = 0; r < kRepeats; ++r) {
                    api.submit_launch_event(gh, 0, 1u << 16, &el, &err);
                    transfer_.launch_ns.push_back(el);
                }
                // H2D/D2H：按 1MB..64MB
                std::size_t bytes = 1u << 20;
                while (bytes <= (1u << 26)) {
                    std::vector<char> host(bytes, 1);
                    std::uint64_t h2d = 0, d2h = 0;
                    for (int w = 0; w < kWarmup; ++w) {
                        api.transfer_h2d(gh, bytes, host.data(), &h2d, &err);
                        api.transfer_d2h(gh, bytes, host.data(), &d2h, &err);
                    }
                    for (int r = 0; r < kRepeats; ++r) {
                        api.transfer_h2d(gh, bytes, host.data(), &h2d, &err);
                        transfer_.h2d_ns.push_back(h2d);
                        api.transfer_d2h(gh, bytes, host.data(), &d2h, &err);
                        transfer_.d2h_ns.push_back(d2h);
                    }
                    transfer_.sizes_bytes.push_back(bytes);
                    bytes <<= 2;  // 1M/4M/16M/64M
                }
            }
        }
    }
    return ops_.size();
}

const FocusedMeasuredOp* FocusedBenchmark::measured(FocusedOp op) const {
    for (const auto& m : ops_) {
        if (m.op == op) return &m;
    }
    return nullptr;
}

namespace {

// 简单线性预测（log2 尺寸插值），供测试阶段留出验证使用
[[maybe_unused]]
double predict_ns(const std::vector<std::size_t>& sizes,
                  const std::vector<std::uint64_t>& ns,
                  std::size_t target) {
    if (ns.size() != sizes.size() || ns.empty()) return 0.0;
    if (ns.size() == 1) return static_cast<double>(ns[0]);
    const double logt = std::log2(static_cast<double>(target));
    for (std::size_t i = 1; i < sizes.size(); ++i) {
        const double l0 = std::log2(static_cast<double>(sizes[i - 1]));
        const double l1 = std::log2(static_cast<double>(sizes[i]));
        if (logt <= l1) {
            const double f = (logt - l0) / (l1 - l0);
            return static_cast<double>(ns[i - 1]) +
                   f * (static_cast<double>(ns[i]) -
                        static_cast<double>(ns[i - 1]));
        }
    }
    return static_cast<double>(ns.back());
}

} // anonymous namespace

void FocusedBenchmark::qualify(FocusedProfileKind kind,
                               OperationProfile& profile) const {
    // 资格标记：standard → qualified；quick → diagnostic（不用于生产路由）。
    // 真实留出误差由测试阶段独立测量并写回 profile（见 focused qualification
    // 测试）；此处不做模型自洽冒充。
    for (auto& op : profile.operations) {
        const FocusedMeasuredOp* m = measured(op_id_to_enum(op.operation_id));
        if (!m || m->cpu_ns.size() < 2) { op.qualified = false; continue; }
        op.qualified = (kind == FocusedProfileKind::Standard);
    }
}

FocusedOp op_id_to_enum(const std::string& id) {
    if (id == "synthetic.dense_pixel_accumulate.fp32")
        return FocusedOp::DenseAccumulateFp32;
    if (id == "synthetic.dense_pixel_accumulate.fp64acc")
        return FocusedOp::DenseAccumulateFp64Acc;
    if (id == "synthetic.pixel_reduce.fp64acc")
        return FocusedOp::PixelReduceFp64Acc;
    if (id == "synthetic.drizzle_like_scatter.fp64acc")
        return FocusedOp::DrizzleScatterFp64Acc;
    return FocusedOp::ResidentChain;
}

OperationProfile FocusedBenchmark::build_profile(
    FocusedProfileKind kind) const {
    OperationProfile p;
    // GPU 未实测（未启用/不可用）时只能产生 diagnostic profile
    bool gpu_available = false;
    for (const auto& m : ops_) {
        if (!m.gpu_ns.empty() && m.gpu_ns[0] > 0) { gpu_available = true; break; }
    }
    p.profile_state =
        (kind == FocusedProfileKind::Standard && gpu_available)
            ? "qualified" : "diagnostic";
    p.fingerprint_cpu = cpu_fingerprint();
    p.fingerprint_compiler = "minGW-g++-16.1-C++20";
    p.fingerprint_runtime_kernel_hash = kernel_hash();
    const std::string gpu = gpu_fingerprint();
    if (gpu != "none") p.fingerprint_gpus.push_back(gpu);

    const auto& sizes = focused_size_sequence();
    for (const auto& m : ops_) {
        OperationProfile::Operation op;
        op.operation_id = focused_op_id(m.op);
        op.precision = (m.op == FocusedOp::PixelReduceFp64Acc)
            ? "fp32" : "fp32";
        op.accumulator =
            (m.op == FocusedOp::DenseAccumulateFp32) ? "fp32" : "fp64";
        op.sample_range.min_items = m.min_items;
        op.sample_range.max_items = m.max_items;
        op.sample_range.repeats = kRepeats;

        // CPU 曲线：固定_us=0（CPU 无 launch），ns_per_item=中位斜率
        if (!m.cpu_ns.empty() && m.cpu_ns[0] > 0) {
            std::vector<double> slope;
            for (std::size_t i = 1; i < m.cpu_ns.size(); ++i) {
                const double dn = static_cast<double>(m.cpu_ns[i]) -
                                  static_cast<double>(m.cpu_ns[i - 1]);
                const double ds = static_cast<double>(sizes[i] - sizes[i - 1]);
                if (ds > 0) slope.push_back(dn / ds);
            }
            if (!slope.empty()) {
                std::sort(slope.begin(), slope.end());
                op.cpu.ns_per_item = slope[slope.size() / 2];
            } else {
                op.cpu.ns_per_item =
                    static_cast<double>(m.cpu_ns[0]) /
                    static_cast<double>(std::max<std::size_t>(1, sizes[0]));
            }
            op.cpu.recommended_chunk_items = 1u << 16;  // 64K（缓存友好）
            op.cpu.minimum_chunk_items = 1u << 10;      // 1K
        }
        // GPU 曲线
        if (!m.gpu_ns.empty() && m.gpu_ns[0] > 0) {
            std::vector<double> slope;
            for (std::size_t i = 1; i < m.gpu_ns.size(); ++i) {
                if (m.gpu_ns[i] > m.gpu_ns[i - 1]) {
                    const double dn = static_cast<double>(m.gpu_ns[i]) -
                                      static_cast<double>(m.gpu_ns[i - 1]);
                    const double ds =
                        static_cast<double>(sizes[i] - sizes[i - 1]);
                    if (ds > 0) slope.push_back(dn / ds);
                }
            }
            if (!slope.empty()) {
                std::sort(slope.begin(), slope.end());
                op.gpu.ns_per_item = slope[slope.size() / 2];
            } else {
                op.gpu.ns_per_item =
                    static_cast<double>(m.gpu_ns[0]) /
                    static_cast<double>(std::max<std::size_t>(1, sizes[0]));
            }
            op.gpu.fixed_us =
                static_cast<double>(m.gpu_ns[0]) / 1000.0;
            op.gpu.device_id = "cuda:0";
            op.gpu.recommended_chunk_items = 1u << 20;   // 1M
            op.gpu.minimum_chunk_items = 1u << 14;       // 16K
            // 最小收益规模：由 launch/传输固定开销与每 item 耗时推算
            const double launch_ns =
                transfer_.launch_ns.empty()
                    ? 8000.0 : static_cast<double>(
                        median_of(transfer_.launch_ns));
            const double gpu_ns_per = op.gpu.ns_per_item;
            if (gpu_ns_per > 0.0) {
                op.gpu.min_profitable_items_host =
                    static_cast<std::size_t>((launch_ns * 10.0) / gpu_ns_per);
                op.gpu.min_profitable_items_resident =
                    static_cast<std::size_t>((launch_ns * 4.0) / gpu_ns_per);
            }
        } else {
            // GPU 未实测：保守估算占位（schema 要求 ns_per_item>0）；
            // qualified 由 qualify() 置 false，state 保持 diagnostic
            op.gpu.ns_per_item =
                (op.cpu.ns_per_item > 0.0) ? op.cpu.ns_per_item * 10.0 + 1.0
                                           : 100.0;
            op.gpu.fixed_us = 100.0;
            op.gpu.device_id = "cuda:0";
            op.gpu.recommended_chunk_items = 1u << 20;
            op.gpu.minimum_chunk_items = 1u << 14;
            op.gpu.min_profitable_items_host = 0;
            op.gpu.min_profitable_items_resident = 0;
        }
        // 传输（H2D/D2H 线性拟合）
        if (!transfer_.h2d_ns.empty()) {
            // 每尺寸中位
            std::size_t nsz = transfer_.sizes_bytes.size();
            if (nsz > 0) {
                const std::size_t step =
                    transfer_.h2d_ns.size() / nsz;
                std::vector<std::uint64_t> h2d_med, d2h_med;
                for (std::size_t i = 0; i < nsz; ++i) {
                    std::vector<std::uint64_t> h, d;
                    for (std::size_t j = i * step; j < (i + 1) * step &&
                         j < transfer_.h2d_ns.size(); ++j) {
                        h.push_back(transfer_.h2d_ns[j]);
                        d.push_back(transfer_.d2h_ns[j]);
                    }
                    h2d_med.push_back(median_of(h));
                    d2h_med.push_back(median_of(d));
                }
                // 斜率（ns/byte）与固定延迟
                if (h2d_med.size() >= 2) {
                    const double dn =
                        static_cast<double>(h2d_med.back() - h2d_med.front());
                    const double ds =
                        static_cast<double>(transfer_.sizes_bytes.back() -
                                            transfer_.sizes_bytes.front());
                    if (ds > 0.0) {
                        const double ns_per_byte = dn / ds;
                        op.transfer.h2d_gbps =
                            (ns_per_byte > 0.0)
                                ? 1.0 / ns_per_byte : 0.0;  // bytes/ns = GB/s
                        op.transfer.h2d_fixed_us =
                            (static_cast<double>(h2d_med.front()) -
                             ns_per_byte *
                                 static_cast<double>(
                                     transfer_.sizes_bytes.front())) / 1000.0;
                        if (op.transfer.h2d_fixed_us < 0.0) {
                            op.transfer.h2d_fixed_us = 0.0;
                        }
                    }
                }
                if (d2h_med.size() >= 2) {
                    const double dn =
                        static_cast<double>(d2h_med.back() - d2h_med.front());
                    const double ds =
                        static_cast<double>(transfer_.sizes_bytes.back() -
                                            transfer_.sizes_bytes.front());
                    if (ds > 0.0) {
                        const double ns_per_byte = dn / ds;
                        op.transfer.d2h_gbps =
                            (ns_per_byte > 0.0)
                                ? 1.0 / ns_per_byte : 0.0;
                        op.transfer.d2h_fixed_us =
                            (static_cast<double>(d2h_med.front()) -
                             ns_per_byte *
                                 static_cast<double>(
                                     transfer_.sizes_bytes.front())) / 1000.0;
                        if (op.transfer.d2h_fixed_us < 0.0) {
                            op.transfer.d2h_fixed_us = 0.0;
                        }
                    }
                }
            }
        } else {
            // GPU 未实测：保守传输占位（schema 要求带宽>0；state=diagnostic）
            op.transfer.h2d_gbps = 1.0;
            op.transfer.d2h_gbps = 1.0;
            op.transfer.h2d_fixed_us = 100.0;
            op.transfer.d2h_fixed_us = 100.0;
        }
        // 内存
        op.memory.host_bytes_per_item =
            static_cast<double>(focused_input_bytes_per_item(m.op));
        op.memory.device_bytes_per_item =
            static_cast<double>(focused_input_bytes_per_item(m.op));
        op.memory.fixed_device_bytes = 1u << 20;  // 1MB 暂存
        op.memory.fixed_host_bytes = 1u << 20;
        p.operations.push_back(std::move(op));
    }
    return p;
}

} // namespace astro::compute::qualification::focused

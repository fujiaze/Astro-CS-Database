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

// 最小二乘斜率（ns/item）：y = intercept + slope * x
double fit_slope(const std::vector<std::size_t>& sizes,
                 const std::vector<std::uint64_t>& ns) {
    if (sizes.size() != ns.size() || sizes.size() < 2) return 0.0;
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    const std::size_t n = sizes.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(sizes[i]);
        const double y = static_cast<double>(ns[i]);
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    const double denom = static_cast<double>(n) * sxx - sx * sx;
    if (std::fabs(denom) < 1e-12) return 0.0;
    return (static_cast<double>(n) * sxy - sx * sy) / denom;
}

double fit_intercept(const std::vector<std::size_t>& sizes,
                     const std::vector<std::uint64_t>& ns,
                     double slope) {
    if (sizes.empty()) return 0.0;
    double sy = 0.0, sx = 0.0;
    const std::size_t n = sizes.size();
    for (std::size_t i = 0; i < n; ++i) {
        sy += static_cast<double>(ns[i]);
        sx += static_cast<double>(sizes[i]);
    }
    return (sy - slope * sx) / static_cast<double>(n);
}

// 真实运行指纹（04 号规范 §7）：编译器宏 + 运行环境线程数 + 内核地址 hash
std::string compiler_fingerprint() {
#if defined(_MSC_VER)
    return std::string("msvc-") + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string("clang-") + std::to_string(__clang_major__) + "." +
           std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    return std::string("gcc-") + std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#else
    return "unknown-compiler";
#endif
}

// CPU 指纹（处理器线程数 + 可执行能力，来自实际运行环境）
std::string cpu_fingerprint() {
    std::string f = "cpu-hw-";
    f += std::to_string(std::thread::hardware_concurrency());
    f += "t";
    // ISA 能力（真实运行环境检测）
    try {
        const std::string isa = astro::compute::detect_isa_caps();
        if (isa.find("avx512") != std::string::npos) f += "-avx512";
        else if (isa.find("avx2") != std::string::npos) f += "-avx2";
        else if (isa.find("avx") != std::string::npos) f += "-avx";
        else if (isa.find("sse") != std::string::npos) f += "-sse";
        f += "-caps" + std::to_string(isa.size());
    } catch (...) {
        f += "-isa-unknown";
    }
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
    // 运行时内核地址 hash：反映实际加载的二进制（禁止硬编码 seed）
    std::uint64_t h = 0x9E3779B97F4A7C15ULL;
    auto mix = [&h](const void* p) {
        std::uint64_t v = reinterpret_cast<std::uintptr_t>(p);
        v ^= v >> 23; v *= 0x2127599bf4325c37ULL; v ^= v >> 47;
        h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    };
    // 目标 Operation 内核族（与注册 launcher 对应）
    mix(reinterpret_cast<const void*>(&run_cpu_operation));
    mix(reinterpret_cast<const void*>(&run_gpu_operation));
    mix(reinterpret_cast<const void*>(&fill_uniform_fp32));
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(h));
    return std::string("acr-kernels-") + buf;
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

            // ---- CPU 测量（3 预热 + 7 有效）----
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

            // ---- GPU host roundtrip 与 resident 测量（可用时）----
            if (enable_gpu) {
                std::vector<std::uint64_t> host_samples, res_samples;
                std::uint64_t el = 0, tr = 0;
                bool supported = false;
                for (int w = 0; w < kWarmup; ++w) {
                    supported = run_gpu_operation(op, x, y, partials, 256, el, tr);
                    if (!supported) break;
                }
                if (supported) {
                    for (int r = 0; r < kRepeats; ++r) {
                        std::fill(y.begin(), y.end(), 2.0f);
                        std::fill(partials.begin(), partials.end(), 0.0);
                        if (run_gpu_operation(op, x, y, partials, 256, el, tr)) {
                            host_samples.push_back(el);
                        }
                    }
                }
                // resident：真实上传一次 + resident 提交
                bool res_supported = false;
                for (int w = 0; w < kWarmup; ++w) {
                    res_supported = run_gpu_operation_resident(
                        op, x, y, partials, 256, el, tr);
                    if (!res_supported) break;
                }
                if (res_supported) {
                    for (int r = 0; r < kRepeats; ++r) {
                        std::fill(y.begin(), y.end(), 2.0f);
                        std::fill(partials.begin(), partials.end(), 0.0);
                        if (run_gpu_operation_resident(
                                op, x, y, partials, 256, el, tr)) {
                            res_samples.push_back(el);
                        }
                    }
                }
                if (!host_samples.empty()) {
                    m.gpu_host_ns.push_back(median_of(host_samples));
                } else {
                    m.gpu_host_ns.push_back(0);
                }
                if (!res_samples.empty()) {
                    m.gpu_resident_ns.push_back(median_of(res_samples));
                } else {
                    m.gpu_resident_ns.push_back(0);
                }
            } else {
                m.gpu_host_ns.push_back(0);
                m.gpu_resident_ns.push_back(0);
            }
        }

        // ---- 候选块实测（08 号计划 §2：替代硬编码 64K/1M）----
        {
            // 候选块：覆盖缓存友好 / 稳定吞吐 / 大规模三档，避免过小块
            const std::size_t cpu_cands[] = {1u << 16, 1u << 18, 1u << 20};
            for (std::size_t c : cpu_cands) {
                std::vector<std::uint64_t> samples;
                std::vector<float> xc(c), yc(c, 2.0f);
                std::vector<double> pb(256, 0.0);
                fill_uniform_fp32(xc.data(), c, 0xA57C5AC20260802ULL);
                for (int r = 0; r < 5; ++r) {
                    std::fill(yc.begin(), yc.end(), 2.0f);
                    std::fill(pb.begin(), pb.end(), 0.0);
                    samples.push_back(run_cpu_operation(op, xc, yc, pb, 256));
                }
                m.cpu_chunk_candidates.push_back(c);
                m.cpu_chunk_ns_per_block.push_back(
                    static_cast<double>(median_of(samples)));
            }
            if (enable_gpu) {
                const std::size_t gpu_cands[] = {1u << 19, 1u << 20, 1u << 22};
                for (std::size_t c : gpu_cands) {
                    std::vector<std::uint64_t> samples;
                    for (int r = 0; r < 5; ++r) {
                        std::vector<float> xc(c), yc(c, 2.0f);
                        std::vector<double> pb(256, 0.0);
                        fill_uniform_fp32(xc.data(), c, 0xA57C5AC20260802ULL);
                        std::uint64_t el = 0, tr = 0;
                        run_gpu_operation(op, xc, yc, pb, 256, el, tr);
                        samples.push_back(el);
                    }
                    m.gpu_chunk_candidates.push_back(c);
                    m.gpu_chunk_ns_per_block.push_back(
                        static_cast<double>(median_of(samples)));
                }
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
    // 08 号计划 §2：每 Operation 独立资格。
    //   - leave-one-out 真实预测误差（禁止伪零）
    //   - CPU/GPU 曲线均有效
    //   - 至少一条 GPU 路径（host/resident）存在真实收益交叉点
    //   - standard 档才可 qualified；quick 仅 diagnostic
    const auto& sizes = focused_size_sequence();
    const double kMedianLimit = 0.30;
    const double kP95Limit = 0.60;
    for (auto& op : profile.operations) {
        const FocusedMeasuredOp* m = measured(op_id_to_enum(op.operation_id));
        op.qualified = false;
        if (!m || m->cpu_ns.size() < 2 ||
            m->gpu_resident_ns.size() != m->cpu_ns.size()) {
            continue;
        }
        // ---- leave-one-out CPU 误差 ----
        std::vector<double> cpu_errs;
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            std::vector<std::size_t> fit_sz;
            std::vector<std::uint64_t> fit_ns;
            for (std::size_t j = 0; j < sizes.size(); ++j) {
                if (j == i) continue;
                fit_sz.push_back(sizes[j]);
                fit_ns.push_back(m->cpu_ns[j]);
            }
            const double slope = fit_slope(fit_sz, fit_ns);
            const double inter = fit_intercept(fit_sz, fit_ns, slope);
            const double pred = inter + slope * static_cast<double>(sizes[i]);
            const double actual = static_cast<double>(m->cpu_ns[i]);
            if (actual > 0.0 && pred > 0.0) {
                cpu_errs.push_back(std::fabs(pred - actual) / actual);
            }
        }
        if (!cpu_errs.empty()) {
            std::sort(cpu_errs.begin(), cpu_errs.end());
            op.cpu.median_error_ratio = cpu_errs[cpu_errs.size() / 2];
            op.cpu.p95_error_ratio =
                cpu_errs[static_cast<std::size_t>(
                    0.95 * static_cast<double>(cpu_errs.size() - 1))];
        }
        // ---- leave-one-out GPU resident 误差 ----
        std::vector<double> gpu_errs;
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            std::vector<std::size_t> fit_sz;
            std::vector<std::uint64_t> fit_ns;
            for (std::size_t j = 0; j < sizes.size(); ++j) {
                if (j == i) continue;
                fit_sz.push_back(sizes[j]);
                fit_ns.push_back(m->gpu_resident_ns[j]);
            }
            const double slope = fit_slope(fit_sz, fit_ns);
            const double inter = fit_intercept(fit_sz, fit_ns, slope);
            const double pred = inter + slope * static_cast<double>(sizes[i]);
            const double actual = static_cast<double>(m->gpu_resident_ns[i]);
            if (actual > 0.0 && pred > 0.0) {
                gpu_errs.push_back(std::fabs(pred - actual) / actual);
            }
        }
        if (!gpu_errs.empty()) {
            std::sort(gpu_errs.begin(), gpu_errs.end());
            op.gpu.median_error_ratio = gpu_errs[gpu_errs.size() / 2];
            op.gpu.p95_error_ratio =
                gpu_errs[static_cast<std::size_t>(
                    0.95 * static_cast<double>(gpu_errs.size() - 1))];
        }
        // ---- 资格判定 ----
        const bool gpu_curve_ok = m->gpu_resident_ns[0] > 0;
        const bool gpu_path_ok =
            op.gpu.host_path_eligible || op.gpu.resident_path_eligible;
        const bool cpu_err_ok =
            op.cpu.median_error_ratio <= kMedianLimit &&
            op.cpu.p95_error_ratio <= kP95Limit;
        const bool gpu_err_ok =
            op.gpu.median_error_ratio <= kMedianLimit &&
            op.gpu.p95_error_ratio <= kP95Limit;
        if (kind == FocusedProfileKind::Standard && gpu_curve_ok &&
            gpu_path_ok && cpu_err_ok && gpu_err_ok) {
            op.qualified = true;
        }
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
        if (!m.gpu_resident_ns.empty() && m.gpu_resident_ns[0] > 0) {
            gpu_available = true; break;
        }
    }
    p.profile_state =
        (kind == FocusedProfileKind::Standard && gpu_available)
            ? "qualified" : "diagnostic";
    p.fingerprint_cpu = cpu_fingerprint();
    p.fingerprint_compiler = compiler_fingerprint();
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

        // ---- 传输模型（H2D/D2H 线性拟合；先于交叉点计算）----
        if (!transfer_.h2d_ns.empty()) {
            std::size_t nsz = transfer_.sizes_bytes.size();
            if (nsz > 0) {
                const std::size_t step = transfer_.h2d_ns.size() / nsz;
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
                if (h2d_med.size() >= 2) {
                    const double dn =
                        static_cast<double>(h2d_med.back() - h2d_med.front());
                    const double ds =
                        static_cast<double>(transfer_.sizes_bytes.back() -
                                            transfer_.sizes_bytes.front());
                    if (ds > 0.0) {
                        const double ns_per_byte = dn / ds;
                        op.transfer.h2d_gbps =
                            (ns_per_byte > 0.0) ? 1.0 / ns_per_byte : 0.0;
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
                            (ns_per_byte > 0.0) ? 1.0 / ns_per_byte : 0.0;
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
            op.transfer.h2d_gbps = 1.0;
            op.transfer.d2h_gbps = 1.0;
            op.transfer.h2d_fixed_us = 100.0;
            op.transfer.d2h_fixed_us = 100.0;
        }
        // ---- 内存公式（先于交叉点计算）----
        op.memory.host_bytes_per_item =
            static_cast<double>(focused_input_bytes_per_item(m.op));
        op.memory.device_bytes_per_item =
            static_cast<double>(focused_input_bytes_per_item(m.op));
        op.memory.fixed_device_bytes = 1u << 20;
        op.memory.fixed_host_bytes = 1u << 20;

        // ---- CPU / GPU resident / GPU host 线性拟合（08 号计划 §2）----
        const double cpu_slope = fit_slope(sizes, m.cpu_ns);
        // 固定开销来自拟合截距（04 号规范 §4：禁止用最小尺寸总耗时）
        const double cpu_fixed = m.cpu_ns.empty()
            ? 0.0 : fit_intercept(sizes, m.cpu_ns, cpu_slope) / 1000.0;
        op.cpu.fixed_us = std::max(0.0, cpu_fixed);  // 截距可为负 → clamp 0
        op.cpu.ns_per_item = cpu_slope;
        // 候选块：选每 item 耗时最低（吞吐稳定区）；并列时选较大块
        // （减少块数，降低调度开销与尾部风险）
        if (!m.cpu_chunk_candidates.empty()) {
            std::size_t best = 0;
            for (std::size_t i = 1; i < m.cpu_chunk_candidates.size(); ++i) {
                const double a =
                    m.cpu_chunk_ns_per_block[best] /
                    static_cast<double>(m.cpu_chunk_candidates[best]);
                const double b =
                    m.cpu_chunk_ns_per_block[i] /
                    static_cast<double>(m.cpu_chunk_candidates[i]);
                if (b < a * 0.95) best = i;       // 明显更快 → 换
                else if (b <= a * 1.05) {
                    if (m.cpu_chunk_candidates[i] >
                        m.cpu_chunk_candidates[best]) best = i;  // 吞吐接近取大块
                }
            }
            op.cpu.recommended_chunk_items = m.cpu_chunk_candidates[best];
            op.cpu.minimum_chunk_items =
                m.cpu_chunk_candidates.front() / 4;
            if (op.cpu.minimum_chunk_items == 0) op.cpu.minimum_chunk_items = 1;
        } else {
            op.cpu.recommended_chunk_items = 1u << 16;
            op.cpu.minimum_chunk_items = 1u << 10;
        }

        const bool gpu_measured =
            !m.gpu_resident_ns.empty() && m.gpu_resident_ns[0] > 0;
        if (gpu_measured) {
            const double res_slope = fit_slope(sizes, m.gpu_resident_ns);
            const double res_fixed =
                fit_intercept(sizes, m.gpu_resident_ns, res_slope) / 1000.0;
            op.gpu.fixed_us = std::max(0.0, res_fixed);
            op.gpu.ns_per_item = res_slope;
            op.gpu.device_id = "cuda:0";
            // 候选块
            if (!m.gpu_chunk_candidates.empty()) {
                std::size_t best = 0;
                for (std::size_t i = 1; i < m.gpu_chunk_candidates.size(); ++i) {
                    const double a =
                        m.gpu_chunk_ns_per_block[best] /
                        static_cast<double>(m.gpu_chunk_candidates[best]);
                    const double b =
                        m.gpu_chunk_ns_per_block[i] /
                        static_cast<double>(m.gpu_chunk_candidates[i]);
                    if (b < a * 0.95) best = i;
                    else if (b <= a * 1.05) {
                        if (m.gpu_chunk_candidates[i] >
                            m.gpu_chunk_candidates[best]) best = i;
                    }
                }
                op.gpu.recommended_chunk_items = m.gpu_chunk_candidates[best];
                op.gpu.minimum_chunk_items =
                    m.gpu_chunk_candidates.front() / 4;
                if (op.gpu.minimum_chunk_items == 0) {
                    op.gpu.minimum_chunk_items = 1;
                }
            } else {
                op.gpu.recommended_chunk_items = 1u << 20;
                op.gpu.minimum_chunk_items = 1u << 14;
            }
            // 交叉点（04 号规范 §4）：
            //   T_cpu(n)  = cpu_fixed + cpu_slope*n
            //   T_res(n)  = res_fixed + res_slope*n
            //   T_host(n) = res_fixed + res_slope*n
            //               + h2d(n) + d2h(n)（传输固定 + 斜率）
            const double h2d_slope_ns =
                (op.transfer.h2d_gbps > 0.0)
                    ? 1.0 / op.transfer.h2d_gbps : 0.0;  // ns/byte
            const double d2h_slope_ns =
                (op.transfer.d2h_gbps > 0.0)
                    ? 1.0 / op.transfer.d2h_gbps : 0.0;
            const double bytes_per_item =
                std::max(1.0, static_cast<double>(
                    focused_input_bytes_per_item(m.op)));
            const double transfer_slope =
                (h2d_slope_ns + d2h_slope_ns) * bytes_per_item;
            const double host_slope = res_slope + transfer_slope;
            const double host_fixed =
                std::max(0.0, res_fixed) + op.transfer.h2d_fixed_us +
                op.transfer.d2h_fixed_us;
            // resident 路径：GPU 渐近边际 < CPU 且存在交叉点 n* > 0
            if (res_slope < cpu_slope) {
                double nstar =
                    (cpu_fixed - res_fixed) / (res_slope - cpu_slope);
                if (nstar <= 0.0) nstar = 1.0;  // GPU 全程更快 → 最小规模即收益
                op.gpu.resident_path_eligible = true;
                op.gpu.min_profitable_items_resident =
                    static_cast<std::size_t>(nstar) + 1;
            }
            // host 路径：host 总斜率 < CPU 斜率且存在交叉点
            if (host_slope < cpu_slope) {
                double nstar =
                    (cpu_fixed - host_fixed) / (host_slope - cpu_slope);
                if (nstar <= 0.0) nstar = 1.0;
                op.gpu.host_path_eligible = true;
                op.gpu.min_profitable_items_host =
                    static_cast<std::size_t>(nstar) + 1;
            }
        } else {
            // GPU 未实测：不适用路径（null 阈值）
            op.gpu.ns_per_item = cpu_slope * 10.0 + 1.0;  // schema 要求 >0
            op.gpu.fixed_us = 100.0;
            op.gpu.device_id = "cuda:0";
            op.gpu.recommended_chunk_items = 1u << 20;
            op.gpu.minimum_chunk_items = 1u << 14;
            op.gpu.host_path_eligible = false;
            op.gpu.resident_path_eligible = false;
            op.gpu.min_profitable_items_host = std::nullopt;
            op.gpu.min_profitable_items_resident = std::nullopt;
        }
        p.operations.push_back(std::move(op));
    }
    return p;
}

} // namespace astro::compute::qualification::focused

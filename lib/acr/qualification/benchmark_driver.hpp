// lib/acr/qualification/benchmark_driver.hpp — 微基准框架
// Phase E：预热 + 多轮 + 分离 kernel/transfer/resident + 保存原始样本。
//
// 设计（ 04_QUALIFICATION_SPEC.md）：
// 1. 固定 seed 0xA57C5AC20260802，所有 input 数据确定性可复现
// 2. 三档 profile：Quick（1 轮无预热）/ Standard（3 轮 + 1 预热）/ Full（10 轮 + 3 预热 + resident）
// 3. 启动时打印 "请确保系统空载以获得准确结果"，不自动判断/干预
// 4. CPU benchmark 用 acr::parallel_for 调度（验证 ACR 自身）；GPU benchmark 占位待 CUDA 集成
// 5. 时间用 std::chrono steady_clock，纳秒精度
// 6. 多轮样本全部保存（不预先平均），median/ stddev 由 profile_generator 聚合
// 7. 空载提示是 console 输出，不修改系统状态
#pragma once

#include "profile_schema.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute {
struct Range1D;  // forward declared from acr.hpp
}

namespace astro::compute::qualification {

// ===== Benchmark 配置 =====
struct BenchmarkConfig {
    ProfileKind profile_kind{ProfileKind::Standard};
    // 标定问题的元素数列表（Quick 用最小集，Full 用全集）
    std::vector<std::size_t> problem_sizes;
    // 预热轮数（Quick=0, Standard=1, Full=3）
    std::uint32_t warmup_rounds{1};
    // 测量轮数（Quick=1, Standard=3, Full=10）
    std::uint32_t measure_rounds{3};
    // 是否采集 resident 分离样本（仅 Full）
    bool collect_resident{false};
    // 是否启用 GPU benchmark（CPU-only 构建应置 false）
    bool enable_gpu{false};
    // 24 §1：kernel 过滤（空=测全部 specs；非空=只测列出的 KernelId）
    // 用于 holdout 验证/定向微基准，避免全量 kernel 的内存与时长开销
    std::vector<std::uint32_t> kernel_ids;
};

// 根据档位生成默认配置
BenchmarkConfig make_default_config(ProfileKind kind, bool enable_gpu) noexcept;

// ===== BenchmarkDriver =====
// 执行微基准并返回每个 (kernel, backend, size) 的原始样本。
class BenchmarkDriver {
public:
    BenchmarkDriver();
    ~BenchmarkDriver();

    // 设置配置（必须在 run 前调用）
    void configure(const BenchmarkConfig& cfg);

    // 运行所有标定 kernel × size × backend，返回原始结果列表
    // 空载提示在 run 入口打印一次。
    std::vector<KernelBenchmarkResult> run();

    // 最后一次运行的日志（ human-readable ）
    const std::string& last_log() const noexcept;

    // 25 §3.2：导出原始记录 JSON（保存每次原始耗时，非聚合）
    // 返回 false 表示写入失败。
    static bool write_raw_records_json(
        const std::string& path,
        const std::vector<KernelBenchmarkResult>& results);

private:
    BenchmarkConfig cfg_;
    std::string log_;
    // 24 §1：GPU 真实微基准（经桥接 DLL，惰性探测）
    void* gpu_handle_{nullptr};
    bool gpu_probe_once_{false};

    // 单个 kernel × size × backend 的一次测量
    RawBenchmarkSample measure_once(std::uint32_t kernel_id,
                                    const std::string& variant,
                                    const std::string& backend,
                                    std::size_t problem_size,
                                    std::size_t bytes_per_elem,
                                    bool measure_resident);

    // CPU AXPY kernel：y[i] = a * x[i] + y[i]
    // 用 acr::parallel_for 执行，返回 kernel 执行时间（纳秒）
    std::uint64_t run_cpu_axpy(std::size_t n);

    // CPU Triad kernel：s[i] = a*x[i] + y[i]（写第三个数组）
    std::uint64_t run_cpu_triad(std::size_t n);

    // CPU Copy kernel：y[i] = x[i]
    std::uint64_t run_cpu_copy(std::size_t n);

    // CPU Dot/Sum：每 chunk 独立 FP64 partial + 唯一槽位 + 串行 merge（无数据竞争）；
    // variant="sum" 时只累加 x[i]（sum 曲线），否则为 dot（x·y 曲线）
    std::uint64_t run_cpu_dot(std::size_t n, const std::string& variant);
    // CPU Histogram：variant = hist_tls（thread-local bins+merge）/
    // hist_atomic（共享 atomic bins）
    std::uint64_t run_cpu_histogram(std::size_t n, const std::string& variant);
    // CPU Scatter：variant = scatter_perm（无冲突置换）/
    // scatter_atomic（atomic 写）
    std::uint64_t run_cpu_scatter(std::size_t n, const std::string& variant);
    // CPU 3x3 卷积：n = 总输出元素数（与 GPU 语义一致），w=ceil(sqrt(n))
    std::uint64_t run_cpu_conv2d(std::size_t n);
    // CPU Mandelbrot：n = 总网格元素数
    std::uint64_t run_cpu_mandelbrot(std::size_t n);

    // GPU 真实 kernel（AXPY/Copy/Dot→reduce/Convolution2D→conv3x3）：
    // 经 cuda::bridge::api() 启动真实 CUDA kernel，返回含 H2D/launch/D2H 的
    // 实测耗时（ns）；不支持的 kernel 或桥接不可用时 supported=false 返回 0。
    std::uint64_t run_gpu_kernel(std::uint32_t kernel_id, std::size_t n,
                                 bool& supported);

    // 本机最优 ISA（baseline/sse2/sse4.2/avx/avx2/avx512；GPU 记录为 "gpu"）
    static std::string detect_best_isa();

    // Commit E：补充 kernel（供 profile_generator 生成多维能力曲线）
    std::uint64_t run_cpu_gather(std::size_t n);        // 随机读 → irregular[gather:random]
    std::uint64_t run_cpu_transfer(std::size_t n);      // 传输 → transfer[host:host_plain]
    std::uint64_t run_cpu_overhead_submit(std::size_t n); // 开销 → overhead[submit]

    // 生成确定性输入（用固定 seed 的 LCG）
    void fill_input(float* dst, std::size_t n, std::uint64_t seed);
    void fill_input(double* dst, std::size_t n, std::uint64_t seed);

    // append log line
    void log(const std::string& line);
};

} // namespace astro::compute::qualification

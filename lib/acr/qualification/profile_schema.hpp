// lib/acr/qualification/profile_schema.hpp — Profile 数据结构与 schema 定义
// Phase E：qualification 微基准结果 + 设备指纹 + 路由档案的数据契约。
//
// 设计（控制包 04_QUALIFICATION_SPEC.md / 06_STATIC_ROUTING_SPEC.md）：
//   1. 公共头不暴露第三方类型（无 nlohmann/json 依赖，手写 ostringstream 序列化）
//   2. ProfileKind 三档：Quick / Standard / Full，决定预热/轮数/resident 分离
//   3. 固定 seed：所有 benchmark 用 0xA57C5AC20260802（确定性、可复现）
//   4. Profile 三态：Missing / Stale / Corrupt（路由器据此决定回退策略）
//   5. 设备指纹：CPU 型号 + 核心数 + ISA + GPU 型号 + 显存 + 驱动版本，SHA-256 哈希
//   6. 路由档案只读：正式运行不修改 profile，不在线学习
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::qualification {

// ===== Profile 档位 =====
enum class ProfileKind : std::uint8_t {
    Quick    = 0,   // 1 轮，无预热，仅 kernel 时间（< 5 秒）
    Standard = 1,   // 3 轮 + 1 轮预热，分离 kernel/transfer（< 30 秒）
    Full     = 2,   // 10 轮 + 3 轮预热 + resident 分离 + 稳定性分析（< 5 分钟）
};

// 字符串转换（CLI / JSON 用）
const char* profile_kind_str(ProfileKind k) noexcept;
bool parse_profile_kind(const std::string& s, ProfileKind& out) noexcept;

// ===== 固定随机种子（确定性 benchmark）=====
constexpr std::uint64_t BENCHMARK_FIXED_SEED = 0xA57C5AC20260802ULL;

// ===== 单次 benchmark 原始结果 =====
// 时间单位：纳秒（ns）。吞吐量：GB/s（gigabytes per second，双精度）。
struct RawBenchmarkSample {
    std::uint64_t kernel_ns{0};       // 纯 kernel 执行时间
    std::uint64_t transfer_ns{0};     // H2D + D2H 传输时间（CPU backend 为 0）
    std::uint64_t resident_ns{0};     // 数据驻留场景时间（Full profile 才采集）
    std::uint64_t total_ns{0};        // kernel + transfer（resident 分离时不计入）
    double throughput_gbps{0.0};      // GB/s
};

// ===== Kernel × Backend 聚合结果 =====
// 每个 (kernel_id, backend, problem_size) 一个聚合记录。
struct KernelBenchmarkResult {
    std::uint32_t kernel_id{0};       // KernelId 的整数值
    std::string kernel_name;          // 人类可读名（"AXPY"/"Triad"/...）
    std::string backend;              // "cpu" / "cuda:0" / "cuda:1" / ...
    std::string precision;            // "fp32" / "fp64"
    // 24 号计划 §1：原始记录区分实现维度
    std::string isa;                  // "baseline"/"sse"/"avx"/"avx2"/"avx512"（GPU 为 "gpu"）
    std::uint32_t threads{0};         // 参与线程数（0=默认全部；GPU 为 0）
    std::size_t problem_size{0};      // 元素数
    std::size_t bytes_per_element{0}; // 字节数（fp32=4, fp64=8）
    std::vector<RawBenchmarkSample> samples;  // 多轮原始样本
    // 聚合统计（由 aggregate 计算）
    std::uint64_t median_kernel_ns{0};
    std::uint64_t median_total_ns{0};
    double median_throughput_gbps{0.0};
    double stddev_kernel_ns{0.0};     // 标准差（稳定性指标）
};

// ===== 设备指纹 =====
// 由 topology + GPU callback 生成，SHA-256 哈希为唯一指纹串。
struct DeviceFingerprint {
    std::string cpu_model;            // CPU 型号字符串
    std::uint32_t cpu_cores{0};       // 物理核心数
    std::uint64_t isa_mask{0};        // IsaLevel 位掩码
    std::string gpu_name;             // GPU 型号（无 GPU 为空）
    std::uint64_t gpu_memory_bytes{0};
    std::string gpu_driver_version;   // "595.79" 等
    std::string sha256;               // 上述字段拼接后 SHA-256 哈希（hex）
    std::string to_json() const;
};

// ===== Profile 三态 =====
enum class ProfileState : std::uint8_t {
    Missing  = 0,   // 无 hardware-profile.json
    Stale    = 1,   // 指纹不匹配
    Corrupt  = 2,   // JSON 解析失败
    Valid    = 3,   // 指纹匹配且 JSON 合法
};

const char* profile_state_str(ProfileState s) noexcept;

// ===== SHA-256 工具（qualification 内部使用）=====
// 提供 SHA-256 哈希的 hex 字符串（32 字节 = 64 hex 字符）。
// 实现细节在 profile_generator.cpp（不暴露 SHA-256 内部类型）。
std::string sha256_hex(const std::string& input);

} // namespace astro::compute::qualification

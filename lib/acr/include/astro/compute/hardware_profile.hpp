// astro/compute/hardware_profile.hpp — ACR 硬件画像数据结构
// Phase E1：按 06_QUALIFICATION_BENCHMARK_SPEC.md §13 + 07_STATIC_ROUTING_AND_MIXED_EXECUTION.md §3 定义。
//
// 设计：
//   1. Curve 是分段曲线（log2 尺寸上的分段线性插值），禁止用一个全局常数代表全部尺寸
//   2. DeviceProfile 按设备和能力族组织多维能力曲线
//   3. HardwareProfile 是顶层容器，包含指纹 + 多个 DeviceProfile
//   4. 运行时只读：正式运行不修改 profile，不在线学习
//   5. 三态处理：Missing（CPU fallback + 警告）/ Stale（指纹不匹配 + 继续运行）/ Corrupt（CPU fallback + 警告）
//   6. lazy load：首次 CostEstimator 调用时加载
//   7. 公共头不暴露第三方类型
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace astro::compute {

// ===== DeviceId：设备标识（与 core/task_descriptor.hpp 一致）=====
// 0 = CPU；1..N = GPU 0..N-1
using DeviceId = std::int32_t;

constexpr DeviceId kHwCpuDeviceId = 0;
constexpr DeviceId kHwInvalidDeviceId = -1;

// ===== 设备类型 =====
enum class DeviceKind : std::uint8_t {
    Cpu = 0,
    Gpu = 1,
};

// ===== 精度（与 acr.hpp::Precision 对齐，但 hardware_profile 自包含）=====
enum class HwPrecision : std::uint8_t {
    Fp32 = 0,
    Fp64 = 1,
};

// ===== 内存层级 =====
enum class MemoryLevel : std::uint8_t {
    L1        = 0,
    L2        = 1,
    L3        = 2,
    L4        = 3,  // LLC + 远端 NUMA
    MainMem   = 4,
    Vram      = 5,  // GPU 显存
};

// ===== 数据驻留 =====
enum class MemoryResidency : std::uint8_t {
    Host          = 0,  // 普通主机内存
    HostPinned    = 1,  // pinned 主机内存
    Device        = 2,  // GPU 显存
    DeviceManaged = 3,  // managed memory
};

// ===== 传输方向 =====
enum class TransferDirection : std::uint8_t {
    H2D = 0,  // host → device
    D2H = 1,  // device → host
    D2D = 2,  // device → device（同 GPU 或 P2P）
    Bidir = 3,  // 双向并发
};

// ===== 内存类型（传输用）=====
enum class MemoryType : std::uint8_t {
    HostPlain = 0,
    HostPinned = 1,
    Device = 2,
};

// ===== 能力族 =====
enum class CapabilityFamily : std::uint8_t {
    Arithmetic  = 0,  // 算术（add/mul/fma/div/sqrt/...）
    Memory      = 1,  // 内存（STREAM 风格 copy/scale/add/triad）
    Transfer    = 2,  // 传输（H2D/D2H/D2D）
    Reduction   = 3,  // 归约（sum/dot/min/max/...）
    Convolution = 4,  // 卷积（direct/separable/fft）
    Irregular   = 5,  // 不规则（gather/scatter/histogram/atomic）
    Branch      = 6,  // 分支（uniform/variable）
    Overhead    = 7,  // 固定开销（submit/launch/event/alloc/merge）
    Library     = 8,  // 库能力（fft/gemm/...）
};

// ===== CurveKey：通用曲线键（字符串形式，每个 family 自定义格式）=====
// 序列化友好，避免复杂的变体类型。
// 格式约定（示例）：
//   arithmetic:  "fp32:add:avx2" / "fp64:fma:avx512"
//   memory:      "L1:copy" / "MainMem:triad" / "Vram:dot"
//   transfer:    "h2d:pinned" / "d2h:plain" / "d2d:p2p"
//   reduction:   "sum:fp32" / "dot:fp64"
//   convolution: "direct:3x3:fp32" / "separable:31:fp64" / "fft:7x7:fp32"
//   irregular:   "gather:random:0.05" / "scatter:atomic:hotspot" / "histogram:uniform"
//   branch:      "uniform" / "highly_variable"
//   overhead:    "submit" / "launch" / "event" / "alloc" / "merge"
//   library:     "fft" / "gemm" / "scan"
using CurveKey = std::string;

// ===== CurvePoint：曲线采样点 =====
struct CurvePoint {
    std::size_t size{0};     // 问题规模（元素数或字节数）
    double median{0.0};      // 中位耗时（纳秒）或吞吐（GB/s），由曲线族决定
    double p95{0.0};         // 95 分位
    double mad{0.0};         // 中位绝对偏差（稳定性指标）
    // 25 号计划 §3.3：样本数与置信度（合格判定依据）
    std::uint32_t sample_count{0};   // 正式样本数（不含预热）
    double confidence{0.0};          // 1 - mad/median（0..1；无样本为 0）
};

// ===== Curve：分段曲线（log2 尺寸上分段线性插值）=====
struct Curve {
    std::vector<CurvePoint> points;  // 按 size 升序
    // 25 号计划 §3.3：曲线来源与资格
    std::string source{"unavailable"};  // "measured" | "estimated" | "unavailable"
    bool qualified{false};              // 仅 full 标定且 sample_count>=7 的 measured 曲线

    // 预测给定 size 的值（分段线性插值；log2 尺寸）
    // 空曲线返回 0.0；size 超出范围用端点外推。
    double predict(std::size_t size) const {
        if (points.empty()) return 0.0;
        if (points.size() == 1) return points[0].median;
        // 找到第一个 size >= 给定 size 的点
        std::size_t i = 0;
        while (i < points.size() && points[i].size < size) ++i;
        if (i == 0) return points.front().median;  // 外推到左端
        if (i >= points.size()) return points.back().median;  // 外推到右端
        // 在 points[i-1] 和 points[i] 之间线性插值
        const auto& a = points[i - 1];
        const auto& b = points[i];
        if (b.size == a.size) return a.median;
        // log2 尺寸插值（小尺寸敏感）
        double la = std::log2(static_cast<double>(a.size > 0 ? a.size : 1));
        double lb = std::log2(static_cast<double>(b.size));
        double ls = std::log2(static_cast<double>(size > 0 ? size : 1));
        if (lb == la) return a.median;
        double t = (ls - la) / (lb - la);
        return a.median + t * (b.median - a.median);
    }

    // 预测吞吐（GB/s）：基于 size 和 predict 的耗时
    // 注：curve 存的是耗时（ns）时，throughput = bytes / (predict * 1e-9) / 1e9
    // 调用方需知道曲线族语义。此处提供通用 throughput 估算。
    double predict_throughput(std::size_t size, std::size_t bytes) const {
        double ns = predict(size);
        if (ns <= 0.0) return 0.0;
        return static_cast<double>(bytes) / ns;  // bytes/ns = GB/s
    }

    bool valid() const noexcept { return !points.empty(); }
};

// ===== FixedOverhead：固定开销（submit/launch/event/alloc/merge）=====
struct FixedOverhead {
    double median_ns{0.0};     // 中位耗时（纳秒）
    double p95_ns{0.0};        // 95 分位
    double cold_start_ns{0.0}; // 冷启动开销（首次调用）
    double warm_ns{0.0};       // 温态开销（已初始化后）
    // 25 号计划 §3.3：固定开销默认估算标记（非实测）
    std::string source{"estimated"};
};

// ===== LibraryCapability：库能力（fft/gemm/...）=====
struct LibraryCapability {
    bool available{false};
    std::string implementation;     // "cuFFT" / "FFTW" / "BLAS" / ...
    std::string version;
    std::map<std::string, Curve> size_curves;  // 按 problem_shape → 曲线
};

// ===== DeviceProfile：单设备多维能力画像 =====
struct DeviceProfile {
    DeviceId device_id{kHwCpuDeviceId};
    std::string device_name;
    DeviceKind kind{DeviceKind::Cpu};

    // 多维能力曲线（按族组织，key 是族内子类型）
    std::map<std::pair<HwPrecision, std::string>, Curve> arithmetic;     // [(precision, op:isa)]
    std::map<std::pair<MemoryLevel, MemoryResidency>, Curve> memory;     // [(level, residency)]
    std::map<std::pair<TransferDirection, MemoryType>, Curve> transfer;  // [(direction, mem_type)]
    std::map<std::pair<std::string, HwPrecision>, Curve> reduction;      // [(op, precision)]
    std::map<CurveKey, Curve> convolution;                                // [method:shape:precision]
    std::map<CurveKey, Curve> irregular;                                  // [pattern:sparsity/contention]
    std::map<CurveKey, Curve> branch;                                     // [uniformity]
    std::map<std::string, FixedOverhead> overhead;                        // [submit/launch/...]
    std::map<std::string, LibraryCapability> library;                     // [fft/gemm/...]

    // 容量信息（CostEstimator 推算 VRAM/RAM 预算用）
    std::size_t total_memory_bytes{0};     // CPU: RAM; GPU: VRAM
    std::size_t available_memory_bytes{0};
    std::uint32_t compute_units{0};        // CPU: cores; GPU: SM
    double peak_bandwidth_gbps{0.0};       // 峰值带宽（GB/s）

    // ===== 曲线查询 =====
    // 通用查询：按族 + key 查找曲线
    const Curve* get_curve(CapabilityFamily family, const CurveKey& key) const {
        switch (family) {
            case CapabilityFamily::Convolution: {
                auto it = convolution.find(key);
                return it != convolution.end() ? &it->second : nullptr;
            }
            case CapabilityFamily::Irregular: {
                auto it = irregular.find(key);
                return it != irregular.end() ? &it->second : nullptr;
            }
            case CapabilityFamily::Branch: {
                auto it = branch.find(key);
                return it != branch.end() ? &it->second : nullptr;
            }
            case CapabilityFamily::Overhead:
            case CapabilityFamily::Library:
                // overhead/library 不是 Curve，这里返回 nullptr（用 get_overhead/get_library）
                return nullptr;
            default:
                // arithmetic/memory/transfer/reduction 用复合 key，此处不解析
                return nullptr;
        }
    }

    // 预测成本（族 + key + size → 耗时 ns）
    double predict_cost(CapabilityFamily family, const CurveKey& key, std::size_t size) const {
        const Curve* c = get_curve(family, key);
        return c ? c->predict(size) : 0.0;
    }

    // 算术成本查询：precision + operation + isa → 耗时 ns
    double predict_arithmetic(HwPrecision prec, const std::string& op_isa, std::size_t size) const {
        auto it = arithmetic.find({prec, op_isa});
        return it != arithmetic.end() ? it->second.predict(size) : 0.0;
    }

    // 内存成本查询：level + residency → 耗时 ns
    double predict_memory(MemoryLevel level, MemoryResidency res, std::size_t size) const {
        auto it = memory.find({level, res});
        return it != memory.end() ? it->second.predict(size) : 0.0;
    }

    // 传输成本查询：direction + mem_type → 耗时 ns
    double predict_transfer(TransferDirection dir, MemoryType mt, std::size_t size) const {
        auto it = transfer.find({dir, mt});
        return it != transfer.end() ? it->second.predict(size) : 0.0;
    }

    // 归约成本查询：op + precision → 耗时 ns
    double predict_reduction(const std::string& op, HwPrecision prec, std::size_t size) const {
        auto it = reduction.find({op, prec});
        return it != reduction.end() ? it->second.predict(size) : 0.0;
    }

    // 固定开销查询：submit/launch/event/alloc/merge → FixedOverhead
    const FixedOverhead* get_overhead(const std::string& name) const {
        auto it = overhead.find(name);
        return it != overhead.end() ? &it->second : nullptr;
    }

    // 库能力查询：fft/gemm/...
    const LibraryCapability* get_library(const std::string& name) const {
        auto it = library.find(name);
        return it != library.end() ? &it->second : nullptr;
    }

    bool has_gpu() const noexcept { return kind == DeviceKind::Gpu; }
};

// ===== ProfileState：画像三态（与 routing::ProfileState 对齐）=====
enum class HwProfileState : std::uint8_t {
    Missing  = 0,  // 无 hardware-profile.json → CPU fallback + 警告
    Stale    = 1,  // 指纹不匹配 → 警告但继续运行
    Corrupt  = 2,  // JSON 解析失败 → CPU fallback + 警告
    Valid    = 3,  // 指纹匹配且合法
};

inline const char* hw_profile_state_str(HwProfileState s) noexcept {
    switch (s) {
        case HwProfileState::Missing: return "missing";
        case HwProfileState::Stale:   return "stale";
        case HwProfileState::Corrupt: return "corrupt";
        case HwProfileState::Valid:   return "valid";
    }
    return "unknown";
}

// ===== HardwareProfile：顶层画像容器 =====
struct HardwareProfile {
    std::string schema_version{"acr.hardware_profile.v1"};
    std::string fingerprint_sha256;
    std::string generated_at;       // ISO 8601 简化
    std::string profile_kind;       // "quick" / "standard" / "full"
    // 25 号计划 §3.1：quick 标定仅作冒烟/诊断，不得用于生产路由
    bool diagnostic_only{false};
    HwProfileState state{HwProfileState::Missing};
    bool stale{false};
    std::vector<DeviceProfile> devices;

    // 查找设备（找不到返回 nullptr）
    DeviceProfile* find_device(DeviceId id) {
        for (auto& d : devices) {
            if (d.device_id == id) return &d;
        }
        return nullptr;
    }
    const DeviceProfile* find_device(DeviceId id) const {
        for (const auto& d : devices) {
            if (d.device_id == id) return &d;
        }
        return nullptr;
    }

    // 是否有 GPU
    bool has_gpu() const noexcept {
        for (const auto& d : devices) {
            if (d.kind == DeviceKind::Gpu) return true;
        }
        return false;
    }

    // CPU 设备（第一个 kind==Cpu）
    const DeviceProfile* cpu_device() const {
        for (const auto& d : devices) {
            if (d.kind == DeviceKind::Cpu) return &d;
        }
        return nullptr;
    }

    // GPU 设备列表
    std::vector<DeviceId> gpu_device_ids() const {
        std::vector<DeviceId> ids;
        for (const auto& d : devices) {
            if (d.kind == DeviceKind::Gpu) ids.push_back(d.device_id);
        }
        return ids;
    }
};

// ===== 辅助：枚举转字符串（序列化用）=====
inline const char* device_kind_str(DeviceKind k) noexcept {
    return k == DeviceKind::Cpu ? "cpu" : "gpu";
}
inline const char* hw_precision_str(HwPrecision p) noexcept {
    return p == HwPrecision::Fp32 ? "fp32" : "fp64";
}
inline const char* memory_level_str(MemoryLevel l) noexcept {
    switch (l) {
        case MemoryLevel::L1:      return "L1";
        case MemoryLevel::L2:      return "L2";
        case MemoryLevel::L3:      return "L3";
        case MemoryLevel::L4:      return "L4";
        case MemoryLevel::MainMem: return "MainMem";
        case MemoryLevel::Vram:    return "Vram";
    }
    return "unknown";
}
inline const char* memory_residency_str(MemoryResidency r) noexcept {
    switch (r) {
        case MemoryResidency::Host:          return "host";
        case MemoryResidency::HostPinned:    return "host_pinned";
        case MemoryResidency::Device:        return "device";
        case MemoryResidency::DeviceManaged: return "device_managed";
    }
    return "unknown";
}
inline const char* transfer_direction_str(TransferDirection d) noexcept {
    switch (d) {
        case TransferDirection::H2D:    return "h2d";
        case TransferDirection::D2H:    return "d2h";
        case TransferDirection::D2D:    return "d2d";
        case TransferDirection::Bidir:  return "bidir";
    }
    return "unknown";
}
inline const char* memory_type_str(MemoryType m) noexcept {
    switch (m) {
        case MemoryType::HostPlain:  return "host_plain";
        case MemoryType::HostPinned: return "host_pinned";
        case MemoryType::Device:     return "device";
    }
    return "unknown";
}
inline const char* capability_family_str(CapabilityFamily f) noexcept {
    switch (f) {
        case CapabilityFamily::Arithmetic:  return "arithmetic";
        case CapabilityFamily::Memory:      return "memory";
        case CapabilityFamily::Transfer:    return "transfer";
        case CapabilityFamily::Reduction:   return "reduction";
        case CapabilityFamily::Convolution: return "convolution";
        case CapabilityFamily::Irregular:   return "irregular";
        case CapabilityFamily::Branch:      return "branch";
        case CapabilityFamily::Overhead:    return "overhead";
        case CapabilityFamily::Library:     return "library";
    }
    return "unknown";
}

} // namespace astro::compute

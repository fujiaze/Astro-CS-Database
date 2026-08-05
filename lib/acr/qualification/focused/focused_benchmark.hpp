// lib/acr/qualification/focused/focused_benchmark.hpp — 聚焦 Benchmark 驱动
//
// 08 号计划 §4 / 04 号规范：只测路由需要的最小信息：
//   - GPU launch/event 固定开销
//   - H2D / D2H 传输（pageable）
//   - 5 个目标合成 Operation（CPU 生产线程配置 + GPU）
//   - 尺寸：256K / 1M / 4M / 16M / 64M items
//   - 每点 3 次预热 + 7 次有效测量
//
// 输出 OperationProfile（quick=diagnostic；standard=qualified）。
#pragma once

#include "focused_operations.hpp"
#include "operation_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::qualification::focused {

// ===== Benchmark 档位 =====
enum class FocusedProfileKind : std::uint8_t {
    Quick = 0,      // 诊断（diagnostic_only，不用于生产路由）
    Standard = 1,   // 正式资格（qualified）
};

// ===== 测量结果 =====
struct FocusedMeasuredOp {
    FocusedOp op{};
    std::size_t min_items{0};
    std::size_t max_items{0};
    std::vector<std::uint64_t> cpu_ns;     // 每尺寸 CPU 耗时（中位）
    std::vector<std::uint64_t> gpu_ns;     // 每尺寸 GPU 端到端（host_roundtrip）
    std::vector<std::uint64_t> gpu_resident_ns;  // resident 计算（当前=端到端近似）
};

// ===== 传输/开销测量 =====
struct FocusedTransferMeas {
    std::vector<std::uint64_t> h2d_ns;     // 每尺寸 H2D（pageable）
    std::vector<std::uint64_t> d2h_ns;     // 每尺寸 D2H
    std::vector<std::uint64_t> launch_ns;  // launch/event/sync
    std::vector<std::size_t> sizes_bytes;
};

// ===== FocusedBenchmark =====
class FocusedBenchmark {
public:
    // 运行全部目标测量；返回操作数（0=失败）
    std::size_t run(FocusedProfileKind kind, bool enable_gpu);

    // 生成 OperationProfile（quick=diagnostic；standard=qualified）
    OperationProfile build_profile(FocusedProfileKind kind) const;

    // 留出验证：对每个 Operation，用拟合区间外的尺寸验证预测误差
    // （中位误差 ≤30%、P95 ≤60% 为合格；不达标保留 qualified=false）
    void qualify(FocusedProfileKind kind, OperationProfile& profile) const;

    const FocusedMeasuredOp* measured(FocusedOp op) const;
    const FocusedTransferMeas& transfer() const noexcept { return transfer_; }

private:
    std::vector<FocusedMeasuredOp> ops_;
    FocusedTransferMeas transfer_;
    std::vector<std::size_t> sizes_{
        1u << 18, 1u << 20, 1u << 22, 1u << 24, 1u << 26};  // 256K..64M
};

// 尺寸序列访问（供工具与测试复用）
const std::vector<std::size_t>& focused_size_sequence() noexcept;

// OperationId 字符串 → 枚举（未识别返回 ResidentChain）
FocusedOp op_id_to_enum(const std::string& id);

} // namespace astro::compute::qualification::focused

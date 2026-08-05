// lib/acr/qualification/focused/operation_profile.hpp — 聚焦 OperationProfile
//
// 08 号计划 §5/04 号规范 §5：每个目标 Operation 只保存路由需要的简单实测参数：
//   - CPU 固定开销、每 item 耗时、推荐/最小块
//   - GPU launch 开销、resident 每 item 耗时、推荐块
//   - host 与 resident 两种最小 GPU 收益规模
//   - H2D/D2H 带宽与固定延迟
//   - host/device 每 item 字节与固定 workspace
//   - 样本范围、置信状态、硬件/编译指纹
//
// Schema：schemas/operation_profile.schema.json（acr-operation-profile-1）
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace astro::compute::qualification::focused {

// ===== OperationProfile（与 schema 对齐的最小字段）=====
struct OperationProfile {
    // ---- 顶层 ----
    std::string schema_version{"acr-operation-profile-1"};
    std::string profile_state{"diagnostic"};  // diagnostic/qualified/stale/partial

    // ---- fingerprint ----
    std::string fingerprint_cpu;
    std::vector<std::string> fingerprint_gpus;
    std::string fingerprint_compiler;
    std::string fingerprint_runtime_kernel_hash;

    // ---- 单个 Operation ----
    struct DeviceCurve {
        double fixed_us{0.0};              // 固定开销（us）
        double ns_per_item{0.0};           // 每 item 耗时（ns）
        std::size_t recommended_chunk_items{0};
        std::size_t minimum_chunk_items{0};
        double median_error_ratio{0.0};    // 留出验证误差
        double p95_error_ratio{0.0};
    };

    struct Operation {
        std::string operation_id;
        std::string precision{"fp32"};     // fp32/fp64
        std::string accumulator{"none"};   // fp32/fp64/none
        bool qualified{false};

        struct SampleRange {
            std::size_t min_items{0};
            std::size_t max_items{0};
            std::size_t repeats{0};
        } sample_range;

        DeviceCurve cpu;
        struct GpuCurve : DeviceCurve {
            std::string device_id{"cuda:0"};
            double launch_us{0.0};
            std::size_t min_profitable_items_host{0};
            std::size_t min_profitable_items_resident{0};
        } gpu;

        struct Transfer {
            double h2d_fixed_us{0.0};
            double h2d_gbps{0.0};
            double d2h_fixed_us{0.0};
            double d2h_gbps{0.0};
        } transfer;

        struct Memory {
            double host_bytes_per_item{0.0};
            double device_bytes_per_item{0.0};
            std::size_t fixed_host_bytes{0};
            std::size_t fixed_device_bytes{0};
        } memory;
    };
    std::vector<Operation> operations;

    // 查找指定 operation
    Operation* find(const std::string& id) {
        for (auto& op : operations) {
            if (op.operation_id == id) return &op;
        }
        return nullptr;
    }
    const Operation* find(const std::string& id) const {
        for (const auto& op : operations) {
            if (op.operation_id == id) return &op;
        }
        return nullptr;
    }
};

// ===== 序列化 / 反序列化（JSON，手写，无第三方依赖）=====
std::string serialize_operation_profile(const OperationProfile& profile);
bool write_operation_profile_to_file(const std::string& path,
                                     const OperationProfile& profile);

// 从 JSON 文件读取（返回 false=解析失败）
bool read_operation_profile_from_file(const std::string& path,
                                      OperationProfile& out);

// ===== schema 校验（JSON Schema 2020-12，本地实现子集）=====
// 校验必需字段与枚举；返回 false 时 error 给出原因。
bool validate_operation_profile(const OperationProfile& profile,
                                std::string& error);

} // namespace astro::compute::qualification::focused

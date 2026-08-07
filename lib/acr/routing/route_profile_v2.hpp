// lib/acr/routing/route_profile_v2.hpp
//
// ACR Benchmark 驱动路由（控制包 20260807-050558，SHA d026ea30...c178537）：
// Operation Route Profile v2（schema acr-operation-route-profile-2）。
//
// 冻结语义（03/04 号规范）：
//   - 候选路径固定为 Legacy OpenMP / GPU-only Direct / CPU+GPU Mixed；
//   - 路由只由离线 Profile 预测端到端完工时间 + 运行时驻留/排队/内存状态
//     决定，不使用人工像素阈值或固定比例；
//   - 旧 fixed_us/ns_per_item/min_profitable_items 仅作诊断，不参与 Auto；
//   - Profile 保存原始样本、validated domain、分场景路径曲线、chunk 服务
//     曲线、Mixed 校验、预测误差与指纹。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::routing {

// ===== 路由场景键（03 号规范 §3）=====
enum class InputResidency : std::uint8_t {
    HostOnly = 0,
    DeviceResident = 1,
};

enum class OutputMaterialization : std::uint8_t {
    HostRequired = 0,
    KeepDevice = 1,
};

struct RouteScenarioKey {
    InputResidency input{InputResidency::HostOnly};
    OutputMaterialization output{OutputMaterialization::HostRequired};
    std::uint32_t reuse_count_hint{1};  // 1=single-shot

    // 标准场景 id（与 schema 枚举一致）
    std::string id() const;
};

// ===== 候选路径枚举 =====
enum class RouteKind : std::uint8_t {
    OpenMP = 0,
    GpuDirect = 1,
    Mixed = 2,
};

// ===== 运行时路由请求（06 号规范）=====
struct QueueSnapshot {
    double cpu_delay_ms{0.0};
    double gpu_delay_ms{0.0};
};

struct MemorySnapshot {
    std::uint64_t ram_available_bytes{0};
    std::uint64_t vram_available_bytes{0};
};

struct RouteRequest {
    std::string operation_id;
    std::uint64_t output_items{0};
    std::uint32_t frame_count{0};
    std::uint64_t input_bytes{0};
    std::uint64_t output_bytes{0};
    InputResidency input_residency{InputResidency::HostOnly};
    OutputMaterialization output_policy{OutputMaterialization::HostRequired};
    std::uint32_t reuse_count_hint{1};
    QueueSnapshot queues;
    MemorySnapshot memory;
};

// ===== 单条路径预测 =====
struct RoutePrediction {
    RouteKind route{RouteKind::OpenMP};
    double predicted_ms{0.0};
    double error_bound_ms{0.0};
    double queue_delay_ms{0.0};
    double score_ms{0.0};      // predicted + queue + error_bound
    bool feasible{false};
    std::string reason;
};

// ===== 路由决策 =====
struct RouteDecision {
    RouteKind chosen{RouteKind::OpenMP};
    RoutePrediction openmp;
    RoutePrediction gpu_direct;
    RoutePrediction mixed;
    std::uint64_t cpu_chunk_items{0};
    std::uint64_t gpu_chunk_items{0};
    std::string reason;
};

// ===== Profile v2 数据结构（对齐 schema）=====

// 原始样本点（中位正式测量绑定同一样本 stats）
struct RouteSamplePoint {
    std::uint64_t output_items{0};
    std::uint32_t frame_count{0};
    std::uint32_t reuse_count{1};
    std::uint64_t input_bytes{0};
    std::uint64_t output_bytes{0};
    std::uint64_t cpu_chunk_items{0};
    std::uint64_t gpu_chunk_items{0};
    double median_ms{0.0};
    double p90_ms{0.0};
    // 绑定 stats
    std::uint64_t cpu_items{0};
    std::uint64_t gpu_items{0};
    std::uint64_t cpu_chunks{0};
    std::uint64_t gpu_chunks{0};
    std::uint64_t setup_h2d_bytes{0};
    std::uint64_t timed_h2d_bytes{0};
    std::uint64_t timed_d2h_bytes{0};
    std::uint64_t peak_ram_bytes{0};
    std::uint64_t absolute_peak_vram_bytes{0};
};

// 单条路径（OpenMP/GPU Direct/Mixed）
struct RoutePath {
    bool eligible{false};
    std::string reason;
    std::vector<RouteSamplePoint> samples;
    std::uint64_t min_output_items{0};
    std::uint64_t max_output_items{0};
    std::vector<std::uint32_t> frame_counts;
    bool allow_tail_extrapolation{false};
    double median_error_ratio{0.0};
    double p95_error_ratio{0.0};
};

// 场景画像
struct RouteScenarioProfile {
    std::string scenario_id;   // cold_host_output 等
    RoutePath openmp;
    RoutePath gpu_direct;
    RoutePath mixed;
};

// chunk 服务曲线点（固定标定帧数下的单块服务时间）
struct ChunkServicePoint {
    std::uint64_t chunk_items{0};
    std::uint32_t frame_count{0};
    double median_ms{0.0};
};

// Operation 级画像
struct OperationRouteProfile {
    std::string operation_id;
    std::vector<std::string> workload_axes{"output_items", "frame_count"};
    std::vector<std::uint64_t> cpu_chunk_candidates;
    std::vector<std::uint64_t> gpu_chunk_candidates;
    std::vector<RouteScenarioProfile> scenarios;
    std::vector<ChunkServicePoint> cpu_chunk_service;
    std::vector<ChunkServicePoint> gpu_chunk_service;
    double mixed_fixed_overhead_ms{0.0};
    double mixed_per_token_ms{0.0};
    bool qualified{false};
    std::string qualification_reason;
};

// 顶层 Profile v2
struct RouteProfileV2 {
    std::string schema_version{"acr-operation-route-profile-2"};
    std::string profile_state{"diagnostic"};  // qualified/partial/diagnostic/stale
    std::string fingerprint_cpu;
    std::vector<std::string> fingerprint_gpus;
    std::string fingerprint_compiler;
    std::string fingerprint_runtime_kernel_hash;
    std::vector<OperationRouteProfile> operations;

    OperationRouteProfile* find_operation(const std::string& id) {
        for (auto& op : operations) {
            if (op.operation_id == id) return &op;
        }
        return nullptr;
    }
    const OperationRouteProfile* find_operation(const std::string& id) const {
        for (const auto& op : operations) {
            if (op.operation_id == id) return &op;
        }
        return nullptr;
    }
    RouteScenarioProfile* find_scenario(OperationRouteProfile& op,
                                        const std::string& sid) {
        for (auto& s : op.scenarios) {
            if (s.scenario_id == sid) return &s;
        }
        return nullptr;
    }
};

// ===== 序列化 / 反序列化 / 校验 =====
std::string serialize_route_profile_v2(const RouteProfileV2& profile);
bool write_route_profile_v2_to_file(const std::string& path,
                                    const RouteProfileV2& profile);
bool read_route_profile_v2_from_file(const std::string& path,
                                     RouteProfileV2& out);

// schema 校验（本地实现子集；返回 false 时 error 给出原因）
bool validate_route_profile_v2(const RouteProfileV2& profile,
                               std::string& error);

// 场景 id 工具
std::string scenario_id(InputResidency input,
                        OutputMaterialization output,
                        std::uint32_t reuse_count);

} // namespace astro::compute::routing

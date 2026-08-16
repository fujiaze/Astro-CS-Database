// lib/acr/routing/route_profile_v2.hpp
//
// ACR Benchmark 驱动路由（ 20260807-050558，SHA d026ea30...c178537）：
// Operation Route Profile v2（schema acr-operation-route-profile-2）。
//
// 冻结语义（03/04 号规范）：
// - 候选路径固定为 Legacy OpenMP / GPU-only Direct / CPU+GPU Mixed；
// - 路由只由离线 Profile 预测端到端完工时间 + 运行时驻留/排队/内存状态
// 决定，不使用人工像素阈值或固定比例；
// - 旧 fixed_us/ns_per_item/min_profitable_items 仅作诊断，不参与 Auto；
// - Profile 保存原始样本、validated domain、分场景路径曲线、chunk 服务
// 曲线、Mixed 校验、预测误差与指纹。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::routing {

// ===== 路由场景键=====
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

// ===== 运行时路由请求=====
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
    // ACR 基座收尾（03_RESOURCE_AND_FALLBACK.md）：本次必须新上传的输入字节
    // （resident 输入已占用显存，不计入增量 VRAM 需求）。
    std::uint64_t upload_required_bytes{0};
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

    // 相等比较（测试断言"Final 不改 samples"用；C++20 默认比较）
    bool operator==(const RouteSamplePoint&) const = default;
};

// 单条路径（OpenMP/GPU Direct/Mixed）
//
// 04 号契约（BDR Reviewed 修正）：
// - model_available：已标定、可执行、可预测。诊断 Route Replay 中所有
// model_available 候选必须参加预测，不能因误差大直接删除最快候选；
// - model_trusted：final untouched holdout 误差门（median<=10%、max<=15%）
// 通过，生产 Auto 才允许使用；
// - eligible：旧 schema 兼容字段，恒等于 model_trusted；
// - 场景未 qualified 时生产只允许 OpenMP fallback。
struct RoutePath {
    bool model_available{false};
    bool model_trusted{false};
    bool eligible{false};  // 兼容旧 schema：== model_trusted
    std::string reason;
    std::vector<RouteSamplePoint> samples;
    std::uint64_t min_output_items{0};
    std::uint64_t max_output_items{0};
    std::vector<std::uint32_t> frame_counts;
    bool allow_tail_extrapolation{false};
    // BDR 复核（08 计划 B/C）：插值模型由 holdout 选择；误差用 median/max。
    std::string interpolation_id{
        "piecewise-linear-items-frames"};  // 或 piecewise-loglog-items-frames-time
    // 兼容字段：最终 holdout 统计（== final_*）
    std::size_t holdout_count{0};
    double median_error_ratio{0.0};
    double max_error_ratio{0.0};
    // BDR Reviewed（08 计划 A/B/C/H）：refinement probe 与 final holdout 分开；
    // 最终误差只由未参与任何模型选择的 final holdout 计算。
    std::size_t refinement_probe_count{0};
    std::size_t final_holdout_count{0};
    std::size_t adaptive_rounds_used{0};
    double final_median_error_ratio{0.0};
    double final_max_error_ratio{0.0};
    double p95_error_ratio{0.0};
    bool metrics_complete{false};
};

// 场景画像
// Route Replay 单点（08 计划 H）：最终独立 holdout 上三候选实际执行，
// Router 只用冻结后的 Profile 预测，比较 chosen 与 oracle best。
struct RouteReplayPoint {
    std::uint64_t output_items{0};
    std::uint32_t frame_count{0};
    std::string chosen_route;   // legacy_openmp / gpu_direct / mixed
    std::string best_route;     // 实际最快路径
    double chosen_actual_ms{0.0};
    double actual_best_ms{0.0};
    double predicted_ms{0.0};   // chosen 路径预测（诊断模式可用 untrusted 模型）
    bool within_best_10pct{false};
};

struct RouteScenarioProfile {
    std::string scenario_id;   // cold_host_output 等
    bool supported{true};
    // Dispatcher Finalization（ CE288DBF...F7E88，08 计划 1）：
    // Route-centric 资格。场景硬门 = 独立 Final Route Replay 全部
    // chosen_actual/oracle_best <= 1.10 + 全部候选 model_available +
    // metrics 完整 + Final>=8 + 数据隔离 + chunk sanity + cold 真实。
    // 单路径 10%/15% 绝对误差降级为诊断（error guard），不再作为删除
    // 候选的硬门。routing_trusted=true 时生产 Auto 使用全部
    // model_available 候选比较。
    bool routing_trusted{false};
    // BDR Reviewed（08 计划 A）：场景级资格兼容字段。
    // 三个 required 场景全部 qualified 后 Operation 才 qualified；
    // 场景未 qualified 时生产路由只允许 OpenMP fallback。
    // 新语义下恒等于 routing_trusted（旧 Profile 反序列化时回填）。
    bool scenario_qualified{false};
    std::string qualification_reason;
    RoutePath openmp;
    RoutePath gpu_direct;
    RoutePath mixed;
    std::size_t final_holdout_count{0};
    std::size_t route_replay_count{0};
    double route_replay_max_slowdown_ratio{1.0};
    std::vector<RouteReplayPoint> route_replay;
};

// chunk 服务曲线点（固定标定帧数下的单块服务时间）
struct ChunkServicePoint {
    std::uint64_t chunk_items{0};
    std::uint32_t frame_count{0};
    double median_service_ms{0.0};   // 真实单 token/chunk 服务时间
    double p90_service_ms{0.0};
    std::size_t sample_count{0};
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

    // BDR Reviewed（08 计划 B）：标定数据集三集合隔离清单
    struct DatasetGroup {
        std::vector<std::uint64_t> fit_items;
        std::vector<std::uint32_t> fit_frames;
        std::vector<std::uint64_t> probe_items;
        std::vector<std::uint32_t> probe_frames;
        std::vector<std::uint64_t> final_items;
        std::vector<std::uint32_t> final_frames;
        bool disjoint_verified{false};
        std::string disjoint_reason;
    } datasets;
};

// 顶层 Profile v2
struct RouteProfileV2 {
    std::string schema_version{"acr-operation-route-profile-2"};
    std::string profile_state{"diagnostic"};  // qualified/partial/diagnostic/stale
    // 权威发布元数据（05_PROFILE_PUBLICATION.md）：
    // 只有 preset=="standard" 可发布 authoritative profile；quick 不得覆盖。
    std::string calibration_preset;    // "standard" / "quick" / "full" / ""
    std::string calibration_head;      // 生成 Profile 的 git HEAD
    std::string calibration_run_id;    // 唯一运行标识（调用方传入）
    std::string generated_utc;         // ISO-8601 UTC 生成时间
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

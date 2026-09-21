// astrocs 资源利用率**观测/判定** (MON-003) — 07 §3/§4/§5 分类+公式+first-10s 诊断+诊断分类
// ABI 冻结(v1)不改公共 API; 本模块纯 CLI 侧判定。硬编码禁令: 线程数/cpus 由调用方注入。
//
// §9.74 裁决 10（ASTROCS_DESIGN §3.5/§6.3）: **一般性资源超限门已取消** —— 本模块的
// 判定结果只作**记录**（resource/resource_gate 事件 + 资源产物），**不产生任何退出码**；
// 唯一资源门 = 磁盘门（lib/infrastructure/cli/disk_gate.h，exit 10 = 磁盘写满/写盘失败）。
//
// 分类(07 §3): compute | memory | io | mixed(不得用 mixed 掩盖低利用率)。
// compute 门禁: 有线程利用(selected_workers/max_active_threads) + avg_equivalent_cores 达标
//                + 非"CPU/io/mem 皆低"。
// memory 门禁: 允许 CPU 未满, 但须达 pre-frozen 带宽比例(由 BENCH-003 写入)。
// io 门禁: 允许低 CPU, 但须有 bytes/ops/await 证据。
// mixed 门禁: 必须拆出 compute/io 子区间。
//
// first-10s 快速失败(07 §4): 首 10s 若"低 CPU + 非 I/O + 非内存带宽饱和" → gate 返回 FAIL。
// 门禁失败 → exit 10(RESOURCE)。诊断分类: 给出具体 failure_kind。
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "memory_report.h"   // MON-002 RSS/allocation report 阈值常量同源
// G-RES-01 阈值唯一数值源（CMake 从 contracts/resource_gate_v1.json 生成；
// 判据语义权威 = docs/plugins/infrastructure/21_observability.md §8）。
#include "resource_gate_thresholds_generated.h"

namespace astrocs {

// 资源类别(MON-002 StageKind 同义; MON-003 用于门禁分类与公式选择)。
enum class ResKind { Compute, Memory, Io, Mixed, Unknown };

inline const char* res_kind_name(ResKind k) {
    switch (k) {
    case ResKind::Compute: return "compute";
    case ResKind::Memory:  return "memory";
    case ResKind::Io:      return "io";
    case ResKind::Mixed:   return "mixed";
    default:               return "unknown";
    }
}

// 诊断分类(门禁失败的具体原因; 单值 e 枚举, 记录到事件)。
enum class GateDiag {
    Ok,
    SingleThreaded,          // selected_workers < 2 (available>=2 时)
    LowAvgCores,             // avg_equivalent_cores < 0.85*min(selected_workers,available)
    UnannotatedPriority,     // 无 stage 标注且 wall>5s(P1, 07 §1)
    ComputeIoMemAllLow,      // CPU/io/mem 带宽皆低(禁止"单线程算法正常"解释)
    MemoryBandwidthLow,      // memory 未达 pre-frozen 带宽比例
    IoMissingEvidence,       // io 缺 bytes/ops/await 证据
    MixedUnsplit,            // mixed 未拆出 compute/io 子区间
    FastFailFirst10s,        // 首 10s 低 CPU+非 IO+非内存带宽饱和 → 协作取消(07 §4)
    GlobalLockDegradation,   // N-worker 相对 1-worker 无正向加速(全局锁退化)
    CpuP50Low,               // MON-002: CPU p50 < 90%(active window>=10s)
    CpuMeanLow,              // MON-002: CPU mean < 85%(active window>=10s)
    MemoryGrowth,            // 内存持续增长: rss_slope 超阈值(记录项; 不产生退出码)
    ProgressStall,           // 无进度(progress 停滞)
    IoWaitHigh,              // 异常 IO 等待(iowait 占比超阈值)
    // MON-001(V7 04_CPU_RESOURCE_TASKS) 逐样本判定追加(末尾追加, 不重排既有值;
    // first10s_diag 以 static_cast<int> 持久化, 追加安全):
    MonitoringMissing,       // 监控缺失/无效(无资源证据) → 直接 FAIL(V7 验收)
    UtilizationP75Low,       // 70% 样本 >= 0.85 不满足(逐样本利用率门, §10.5)
    QueueStarvedCpu,         // 队列有工作时连续 >=10s 利用率 <0.60(§10.5)
    // MON-002(V7 04_CPU_RESOURCE_TASKS) RSS/allocation report 判定追加
    // (末尾追加, 不重排既有值; 判定源 = lib/infrastructure/cli/memory_report.h 报告面):
    AllocGrowthUnbounded,    // RSS 曲线稳健斜率 >= 失败线(注入 leak 失败路径)
    AllocReclaimMissing,     // run 结束回落不可解释(残留超容差且回落低于阈值)
    // GATE-FIX-RES(R-4 D-13 item 1): 判定域不成立时返回**显式分类**, 不再静默 Ok。
    // 判定域 = 计算区间**严格大于** 10 s（契约 applicability.
    // min_active_window_seconds_exclusive）; 旧实现另有 wall<5s 静默豁免, 已删除
    // （与 Python 冻结门的 not_applicable 同义）。
    NotApplicable,
};

inline const char* gate_diag_name(GateDiag d) {
    switch (d) {
    case GateDiag::Ok:                      return "ok";
    case GateDiag::SingleThreaded:          return "single_threaded";
    case GateDiag::LowAvgCores:             return "low_avg_cores";
    case GateDiag::UnannotatedPriority:     return "unannotated_priority";
    case GateDiag::ComputeIoMemAllLow:      return "compute_io_mem_all_low";
    case GateDiag::MemoryBandwidthLow:      return "memory_bandwidth_low";
    case GateDiag::IoMissingEvidence:       return "io_missing_evidence";
    case GateDiag::MixedUnsplit:            return "mixed_unsplit";
    case GateDiag::FastFailFirst10s:        return "fast_fail_first_10s";
    case GateDiag::GlobalLockDegradation:   return "global_lock_degradation";
    case GateDiag::CpuP50Low:               return "cpu_p50_low";
    case GateDiag::CpuMeanLow:              return "cpu_mean_low";
    case GateDiag::MemoryGrowth:            return "memory_growth";
    case GateDiag::ProgressStall:           return "progress_stall";
    case GateDiag::IoWaitHigh:              return "io_wait_high";
    case GateDiag::MonitoringMissing:       return "monitoring_missing";
    case GateDiag::UtilizationP75Low:       return "utilization_p75_low";
    case GateDiag::QueueStarvedCpu:         return "queue_starved_cpu";
    case GateDiag::AllocGrowthUnbounded:    return "alloc_growth_unbounded";
    case GateDiag::AllocReclaimMissing:     return "alloc_reclaim_missing";
    case GateDiag::NotApplicable:           return "not_applicable";
    default:                                return "unknown";
    }
}

// G-RES-01 阈值（判据权威 21_observability §8; 数值唯一源
// contracts/resource_gate_v1.json, 经 resource_gate_thresholds_generated.h 引入）。
// 本文件不再出现字面量阈值: 改数值只改契约, 改语义只改 §8。
// 口径: 这里是**已分配容量百分比**; 采集端 cpu_pct 是 100×等效核
// (percent_of_one_core), 调用方必须先经 cpu_percent_of_allocated_capacity()
// 归一后再写入 cpu_p50/mean_percent。
// 旧注释引用的「宪章 §10.5/§18.2」已废止（旧宪章文件已删, 见
// docs/README-DOCS.md:29）—— 已失效引用, 现指向 ⑥ 插件文档 §8。
inline constexpr double kWorkerP50Min =
    resource_gate_contract::kMinActiveComputeThreads;
inline constexpr double kCpuP50MinPercent =
    resource_gate_contract::kP50UtilizationMinPercent;
inline constexpr double kCpuMeanMinPercent =
    resource_gate_contract::kMeanUtilizationMinPercent;

// ---- MON-001(V7 04_CPU_RESOURCE_TASKS) 哨兵与阈值纪律 ----
// 未采样哨兵: -1 表示"未采样", 不是合法测量值(批次 P p2007 先例: 哨兵是采集
// 可用性问题, 不是 CPU 低利用率证据; 禁止把 -1 当合法值参与统计判定)。
inline constexpr double kMon001NotSampled = -1.0;
inline bool mon001_sampled(double v) { return v > kMon001NotSampled; }
// 逐样本利用率阈值(与 kCpuP50MinPercent 同一口径: 100%=已分配容量用满, 见
// utilization_value())。数值源 = 契约 compute.per_sample_* / queue_low_*;
// 旧 D.6 的 75%/50% 已在契约中废止, 不得回归。
inline constexpr double kMon001UtilSampleMinPercent =
    resource_gate_contract::kPerSampleUtilizationMinPercent;   // 单样本 >=0.85
inline constexpr double kMon001UtilSampleFrac =
    resource_gate_contract::kPerSamplePassFractionMin;         // 达标样本占比 >=0.70
inline constexpr double kMon001QueueWindowSeconds =
    resource_gate_contract::kQueueLowWindowSecondsMin;         // 队列有工作连续低利用窗
inline constexpr double kMon001QueueUtilMinPercent =
    resource_gate_contract::kQueueLowUtilizationPercent;       // 窗口内利用率下限 0.60

// ---- P26(负责人 T2/2.A): 工作量下限 + 记录/裁决分离 ----
// 「重计算运行」工作量下限口径 = **线程秒(thread-seconds) = 等效核·秒**:
//   work_core_seconds := avg_equivalent_cores × active_window_seconds。
// 依据(宪章 §10.5 判定域前提): 利用率判据只在「计算区间超过 10 秒」时才成立;
// 一个刚够判定的重计算区间至少要在这个 10 s 窗口内持续占用 1 个等效核,
// 即 10 核·秒 = 10 线程秒。低于下限的运行**不进入利用率裁决**(只记录事实),
// 避免把极小冒烟/启动阶段跑成"低利用率失败"。
// 落地分工: 程序内 work_core_seconds/gate_workload_above_floor() 只做**事实标记**
// 与报告字段; PASS/FAIL/WARN 的建议由外挂 eng/tools/quality/resource_monitor.py --judge
// 按可配置阈值给出(裁决已移出程序, 见 REPORT.md 治理提示)。
// 历史复现开关 --strict-resource-gate 按定义不咨询本下限(它复现的是变更前的
// rc=10 行为, 供既有断言使用), 属测试面而非生产面。
inline constexpr double kMon003MinCoreSeconds =
    resource_gate_contract::kWorkloadFloorCoreSeconds;

struct GateConfig {
    ResKind kind = ResKind::Unknown;
    uint32_t available_cpus = 0;
    uint32_t selected_workers = 0;
    uint32_t max_active_threads = 0;
    // B2-A18 (GAP-06): 实际观测到的租约并行宽度 (峰值并发授予 token 数;
    // 由 GrantedWorkerObservation 真实累计)。哨兵: 0 = 未观测, 不是合法并行
    // 宽度；不得以配置 selected_workers 回填。U 分母优先用此观测值, 仅在未观测
    // 时回退到原 min(selected,available) 口径（保留旧行为, 阈值不变）。
    uint32_t granted_workers = 0;  // 0 = 未观测 (哨兵)
    double avg_equivalent_cores = 0.0;
    double wall_seconds = 0.0;
    bool has_stage_annotation = false;
    // memory 门禁: 预冻结吞吐比例(BENCH-003 写入, 不外推/不事后改)。
    double achieved_memory_bandwidth_frac = -1.0;   // -1=未测量
    double required_memory_bandwidth_frac = 0.0;    // pre-frozen threshold
    // io 门禁: 证据
    uint64_t io_bytes = 0;      // read+write bytes
    uint64_t io_ops = 0;        // read+write ops
    double io_await_ms = 0.0;   // 可得时
    bool io_is_short_serial = false;  // 短于5s 或 <5% 时长的串行 IO(非阻塞)
    // mixed 子区间拆份: 已拆出? 或仍单一 mixed
    bool mixed_has_compute_subrange = false;
    bool mixed_has_io_subrange = false;
    // N-worker vs 1-worker 合成加速(07 §3 全局锁校验)
    double one_worker_ns = -1.0;
    double n_worker_ns = -1.0;
    // first-10s 快照(07 §4 快速失败)
    bool first10s_low_cpu = false;
    bool first10s_non_io = false;                 // 非 IO 密集
    bool first10s_mem_not_saturated = false;
    double cpu_percent = 0.0;       // 可用于诊断的 CPU%
    double iowait_percent = 0.0;    // 可得时
    double mem_bandwidth_percent = -1.0;  // -1=未测
    // MON-002 复验追加: active 窗口采样统计(负值=未采样, 跳过对应判定)。
    double workers_p50 = -1.0;        // active workers p50(采样; 优先于 selected_workers)
    // CPU p50/mean = **已分配容量百分比**(100% = 已分配容量用满)。采集端 cpu_pct
    // 单位是 100×等效核(percent_of_one_core), 调用方必须经
    // cpu_percent_of_allocated_capacity() 归一后再写入本字段(见 commands.cpp
    // run_with_resource_gate)。M5a-G-002: 直接把采集值写进来 = 单核标度误判。
    double cpu_p50_percent = -1.0;    // CPU p50%(已分配容量归一)
    double cpu_mean_percent = -1.0;   // CPU mean%(同上; §18.2 下限 85%)
    // 横切诊断: 内存持续增长(rss_slope 可为负=收缩, 用显式开关而非负哨兵)
    bool rss_slope_measured = false;
    double rss_slope_mb_per_s = 0.0;
    // FIX-E2E B1-A6: active 计算窗口时长(秒)。判定域收口前置:
    // rss_slope/p75 等统计判据只对 active window >= kMon002MinWindowSeconds 生效;
    // 负值 = 调用方未提供(直接单元判定路径) → 视为代表性, 保持向后兼容。
    // 注意: 阈值按 §18.2 冻结为 85%/60% (另有 32MB/s/10s) 且判定式不动, 只加窗口/
    // 样本量前置。
    double active_window_seconds = -1.0;
    // 单位 MiB/s（1048576 B/s）；判据方向 >=（契约 memory.growth_predicate）。
    // GATE-FIX-RES 对齐: 旧值 32.0 与注释「MB/s」不一致, 且 evaluate_gate 用 >
    // 而 evaluate_mon002 用 >= —— 现统一为契约的 MiB/s + >=。
    double memory_growth_limit_mb_per_s =
        resource_gate_contract::kMemoryGrowthLimitMibPerS;  // 调用方可覆盖
    bool progress_stalled = false;    // 无进度(采样/节点注入)
    double io_wait_high_percent = 50.0;  // 异常 IO 等待阈值(iowait 占比)
    // MON-001(V7 04_CPU_RESOURCE_TASKS) 逐样本观测输入(供 evaluate_mon001;
    // 哨兵纪律: -1=未采样非合法值, 采样缺失跳过对应判定, 全缺失=MonitoringMissing):
    double util_samples_measured = kMon001NotSampled;  // 有效样本数(-1=未提供)
    double util_samples_pass_frac = kMon001NotSampled; // U>=0.75 样本占比(0..1; -1=未提供)
    double queue_low_run_seconds = kMon001NotSampled;  // 队列有工作且 U<0.50 的最长连续时长; -1=未观测
    double heavy_wall_seconds_total = kMon001NotSampled; // 同类 heavy span 累计 wall(防 <5s 切片规避); -1=未汇总
    bool monitor_present = false;     // 监控是否实际运行(heavy run 必须为 true, 否则 FAIL)
    // MON-002(V7 04_CPU_RESOURCE_TASKS) RSS/allocation report 输入(lib/infrastructure/cli/memory_report.h
    // 报告面填充; sampled=false/曲线零样本时保持 -1 哨兵, 跳过对应判定——
    // 采样失败不是内存低占用证据, 但 alloc 面存在且零有效样本即 FAIL, 同 MON-001 纪律):
    bool alloc_report_present = false;      // allocation report 是否实际生成
    double alloc_samples_measured = kMon001NotSampled; // 有效样本数(-1=未提供; 0=面在零样本→FAIL)
    double alloc_growth_mb_per_s = kMon001NotSampled;  // RSS 稳健斜率(MB/s); -1=未采样
    AllocReclaimVerdict alloc_reclaim_verdict = AllocReclaimVerdict::InsufficientSamples;
    // P26(负责人 2.A): 运行工作量 = 线程秒(等效核·秒)。<0 = 调用方未提供
    // (由 work_core_seconds_of() 按 avg_equivalent_cores×window 回算, 保持单元
    // 判定路径向后兼容)。仅作事实标记/报告, 不改变任何 §18.2 冻结阈值与判定式。
    double work_core_seconds = -1.0;
};

// FIX-E2E B1-A6: 统计判据(window>=10s)前置。active_window_seconds<0 = 未提供。
inline constexpr double kMon002MinWindowSeconds =
    resource_gate_contract::kMinActiveWindowSecondsExclusive;
inline bool gate_window_representative(const GateConfig& g) {
    return g.active_window_seconds < 0.0 ||
           g.active_window_seconds >= kMon002MinWindowSeconds;
}

// ---- P26(负责人 T2/2.A): 工作量下限判定 + 记录/裁决分离 ----
// 运行工作量(线程秒) = 显式输入的 work_core_seconds; 未提供时用
// avg_equivalent_cores × 判定窗(active_window_seconds, 未提供则 wall_seconds)回算。
inline double work_core_seconds_of(const GateConfig& g) {
    if (g.work_core_seconds >= 0.0) return g.work_core_seconds;
    const double win = g.active_window_seconds > 0.0 ? g.active_window_seconds
                                                     : g.wall_seconds;
    return g.avg_equivalent_cores * win;
}
inline bool gate_workload_above_floor(const GateConfig& g) {
    return work_core_seconds_of(g) >= kMon003MinCoreSeconds;
}

// 门禁处置(记录与裁决分离):
//   RecordOnly —— **唯一**处置。资源判据只记录/报告(resource_gate 事件 severity=warning),
//                 不改变进程退出码。
//   Enforced   —— **已退役**(§9.74 裁决 10 + ASTROCS_DESIGN §3.5/§6.3: 一般性资源超限门
//                 已取消，内存/CPU/线程不设门)。枚举值保留以免破坏既有 ABI/测试引用，
//                 但 gate_enforcement() 恒返回 RecordOnly —— CLI 面**不存在**由 CPU/内存
//                 判据产生 rc=10 的路径（exit 10 只属磁盘写满/写盘失败，见 disk_gate.h）。
// 注意: 工作量下限仍只作用于记录面标记与外部裁决(eng/tools/quality/resource_monitor.py --judge)。
enum class GateEnforcement { RecordOnly, Enforced };
// 「判定有无违规」的统一谓词: Ok = 判过且通过; NotApplicable = 判定域不成立(未判)。
// 二者都不是违规 —— 调用方一律用本谓词, 不得再写 d == GateDiag::Ok（那会把
// "未判"误当"违规"）。
inline bool gate_diag_is_violation(GateDiag d) {
    return d != GateDiag::Ok && d != GateDiag::NotApplicable;
}

// §9.74 裁决 10: 一般性资源超限门已取消 ⇒ 本函数恒 RecordOnly。
// strict_mode 参数保留（调用方仍解析 --strict-resource-gate / --on-resource-gate，
// 旗标登记为「历史复现开关，已退役」），但**不再**改变裁决；d 只影响记录内容。
inline GateEnforcement gate_enforcement(bool /*strict_mode*/, GateDiag /*d*/) {
    return GateEnforcement::RecordOnly;
}
inline const char* gate_enforcement_name(GateEnforcement e) {
    return e == GateEnforcement::Enforced ? "enforced" : "record_only";
}

// 已分配容量(allocated capacity)分母 —— G-RES-01「已分配容量」的单一实现点。
// 取义已在 docs/plugins/infrastructure/21_observability.md §8 定稿（原
// NEEDS_DECISION(M5a-G-002) 已由 R-4 D-12 裁决并写入契约 denominator）:
//   primary  = granted_workers（真实观测到的租约授予宽度峰值）
//   sentinel = 0（未观测; **不得**以配置值回填, include/astrocs/core/context.h:93-103）
//   fallback = min(selected_workers, available_cpus)
// 观测是权威分母, 不被 available_cpus 封顶(见 mon001_gate_test 18a)。
// 禁止以机器有效核单独充当已分配容量（这正是 run_monitored.py --gate-required
// 旧语义的结构性误报根因, R-4 E7b）。
// 与 eng/tools/monitoring/run_monitored.py::resolve_allocated_capacity 同义。
inline uint32_t allocated_capacity_cores(const GateConfig& g) {
    return (g.granted_workers > 0)
               ? g.granted_workers
               : std::min(g.selected_workers, g.available_cpus);
}

// 采集端 cpu_pct 单位 = 100×等效核(percent_of_one_core, 见 resource_recorder.h)。
// 换算为「已分配容量百分比」: 100% = 已分配容量用满。分配容量 0 → 0.0(除零安全)。
inline double cpu_percent_of_allocated_capacity(const GateConfig& g, double cpu_pct) {
    const uint32_t m = allocated_capacity_cores(g);
    return m >= 1 ? cpu_pct / static_cast<double>(m) : 0.0;
}

// 核心公式(宪章 §10.5/§18.2): compute 且 wall>=5s 时, avg_equivalent_cores
// 下限 = 85% × 已分配容量 = kCpuMeanMinPercent/100 × allocated_capacity_cores(g)
// 核。旧 D.6 的 0.80 已被 §18.2 明文废止(M5a-G-001)。
inline double compute_cores_threshold(const GateConfig& g) {
    if (g.kind != ResKind::Compute) return 0.0;
    const uint32_t m = allocated_capacity_cores(g);
    return (m >= 1 ? (kCpuMeanMinPercent / 100.0) * static_cast<double>(m) : 0.0);
}

// 逐项门禁判定; 返回 GateDiag(Ok 表示通过)。顺序: 使首错可诊断。
// MON-002 复验(v6_1_rework)判定顺序:
//   横切异常(memory_growth/progress_stall/io_wait_high, 对所有 kind 生效; 默认关闭)
//   → mixed 拆份 → 无标注>5s(P1, 07 §1) → kind 判据。
// Compute: 单线程判定前移且不受 wall<5s 固定豁免(MON-002: heavy 任务即使短于 5 秒
//   也不能因固定豁免而掩盖单线程); wall<5s 豁免只跳过统计判据(短窗 mean/p50 样本
//   不足)。I/O 串行 5s/5% 豁免仅在 Io 分支(io_is_short_serial), 不对 Compute 生效。
// CPU p50/mean 阈值仅当调用方提供 active window>=10s 采样时判定(负=未采样跳过)。
inline GateDiag evaluate_gate(const GateConfig& g) {
    // 横切异常(任何 kind; rss 未测/progress 未注/iowait=0 时不触发, 向后兼容)
    // B1-A6: 仅当 active window 达到最短判定窗(>=10s)才判 memory_growth —— <10s
    // 窗口的斜率是采样噪声, 不能作为"无界内存增长"证据(阈值不变)。
    // 方向统一为 >=（契约 memory.growth_predicate）: 旧实现此处用 > 而
    // evaluate_mon002 用 >= —— 同一阈值两个方向（R-4 §5.1 第 3 步 item 5）。
    if (g.rss_slope_measured && gate_window_representative(g) &&
        g.rss_slope_mb_per_s >= g.memory_growth_limit_mb_per_s)
        return GateDiag::MemoryGrowth;
    if (g.progress_stalled)
        return GateDiag::ProgressStall;
    if (g.iowait_percent > g.io_wait_high_percent)
        return GateDiag::IoWaitHigh;
    // mixed 必须拆份(07 §3): 未拆 start+emit 就 FAIL
    if (g.kind == ResKind::Mixed) {
        if (!g.mixed_has_compute_subrange || !g.mixed_has_io_subrange)
            return GateDiag::MixedUnsplit;
        // 拆份后按 compute 子区间继续判; 此处视为通过(compute 子区判据由调用方另行)
        return GateDiag::Ok;
    }
    // 无标注 >5s 区间 → P1(07 §1)
    if (!g.has_stage_annotation && g.wall_seconds > 5.0)
        return GateDiag::UnannotatedPriority;

    if (g.kind == ResKind::Compute) {
        // MON-002: worker p50>=2(available>=2)。采样 p50 权威; 未采样回退
        // selected_workers/max_active_threads。单线程判定不因 wall<5s 豁免。
        if (g.available_cpus >= 2) {
            if (g.workers_p50 >= 0.0) {
                if (g.workers_p50 < kWorkerP50Min) return GateDiag::SingleThreaded;
            } else if (g.selected_workers < 2 || g.max_active_threads < 2) {
                return GateDiag::SingleThreaded;
            }
        }
        // §10.5 判据域收口(OWNER-02, "追认不阻塞执行"): 冻结门的语义前提是
        // "计算区间超过 10 秒"。active window <10s 时统计利用率判据(avg/p50/mean)
        // 不成立(短窗样本不足), 一律跳过; 阈值 0.85*min(selected,available) 与
        // 85%/90% 判定式**不动**(§18.2 冻结值)。active_window_seconds<0 = 直接单元判定
        // 路径, 视为代表性, 保持既有行为(向后兼容)。
        // 判定域 = active window 严格 >10s（契约 applicability）: 不成立即返回
        // **显式 NotApplicable**（非豁免、非 Ok）。旧实现的 wall<5s 静默豁免已删除
        // —— 它使 5s≤wall<10s 或 active_window<10s 的"短而烂" run 两门都放行
        // （R-4 §4.1⑤ 漏报面）。
        if (!gate_window_representative(g)) return GateDiag::NotApplicable;
        // 短任务(未提供 active window 的直接判定路径, wall<5s): 统计利用率判据
        // 不成立 → **显式 NotApplicable**（GATE-FIX-RES: 旧实现返回 Ok, 使"未判"
        // 与"判过且通过"不可区分; MON-002 复验的"短任务不豁免单线程判定"语义保持
        // —— 单线程检查在本行之前已执行）。
        if (g.wall_seconds < 5.0) return GateDiag::NotApplicable;
        const double thr = compute_cores_threshold(g);
        if (thr > 0 && g.avg_equivalent_cores < thr)
            return GateDiag::LowAvgCores;
        // CPU/io/mem 皆低 → 禁止"单线程算法正常"解释(07 §3)
        if (g.cpu_percent < 20.0 && g.iowait_percent < 5.0 &&
            (g.mem_bandwidth_percent < 0.0 || g.mem_bandwidth_percent < 15.0))
            return GateDiag::ComputeIoMemAllLow;
        // N-worker 相对 1-worker 正向加速(全局锁退化)→ FAIL
        if (g.one_worker_ns > 0 && g.n_worker_ns > 0 && g.n_worker_ns > g.one_worker_ns)
            return GateDiag::GlobalLockDegradation;
        // MON-002: CPU p50>=90%、mean>=85%(调用方在 active window>=10s 时提供)
        if (g.cpu_p50_percent >= 0.0 && g.cpu_p50_percent < kCpuP50MinPercent)
            return GateDiag::CpuP50Low;
        if (g.cpu_mean_percent >= 0.0 && g.cpu_mean_percent < kCpuMeanMinPercent)
            return GateDiag::CpuMeanLow;
        return GateDiag::Ok;
    }
    if (g.kind == ResKind::Memory) {
        if (g.achieved_memory_bandwidth_frac < 0.0)
            return GateDiag::MemoryBandwidthLow;   // 未测量=FAIL(须证明)
        if (g.achieved_memory_bandwidth_frac < g.required_memory_bandwidth_frac)
            return GateDiag::MemoryBandwidthLow;
        return GateDiag::Ok;
    }
    if (g.kind == ResKind::Io) {
        // 短于5s 或 <5% 时长的串行 IO 不作为发布阻塞(07 §3)
        if (g.io_is_short_serial) return GateDiag::Ok;
        if (g.io_bytes == 0 && g.io_ops == 0 && g.io_await_ms <= 0.0)
            return GateDiag::IoMissingEvidence;    // 允许低 CPU, 但须证据
        return GateDiag::Ok;
    }
    if (g.kind == ResKind::Unknown) {
        // 未知类别: 若未标注且>5s 则 P1; 否则不阻塞(留给上层确认)
        if (!g.has_stage_annotation && g.wall_seconds > 5.0)
            return GateDiag::UnannotatedPriority;
        return GateDiag::Ok;
    }
    return GateDiag::Ok;
}

// first-10s 快速失败(07 §4): 首 10s 低 CPU + 非 IO + 非内存饱和 → 协作取消, exit 10。
inline bool fast_fail_first10s(const GateConfig& g) {
    return g.first10s_low_cpu && g.first10s_non_io && g.first10s_mem_not_saturated;
}

// ---- MON-001(V7 04_CPU_RESOURCE_TASKS) 资源阈值判定 ----
// 单点近似 U(0..1 分数); 完整聚合判定由 evaluate_mon001 汇总逐样本占比完成。
// 分母 = 已分配容量(见 allocated_capacity_cores); 规格: 不得以硬编码核心数
// 或配置 worker 数单独作分母。
inline double utilization_value(const GateConfig& g, double cpu_pct) {
  return cpu_percent_of_allocated_capacity(g, cpu_pct) / 100.0;
}

// 监控有效性: heavy run 必须有真实监控证据。monitor_present=false 或采样侧
// 未提供任何有效样本 → FAIL(规格: 监控缺失直接 FAIL)。
inline bool monitoring_effective(const GateConfig& g) {
    return g.monitor_present && mon001_sampled(g.util_samples_measured) &&
           g.util_samples_measured > 0.0;
}

// 逐样本聚合判定, 返回 GateDiag。与 evaluate_gate 互补(evaluate_gate 的
// 统计判据 wall>=5s 豁免不动; 本函数收口 V7 MON-001 验收的剩余三条):
//   1) 监控缺失/无效 → MonitoringMissing FAIL(不可豁免);
//   2) 平均 U>=0.85 已由 evaluate_gate LowAvgCores(avg_equivalent_cores)承担;
//   3) >=70% 样本 U>=0.85 → UtilizationP75Low;
//   4) 队列有工作时连续 >=10s U<0.60 → QueueStarvedCpu。
// 哨兵纪律(批次 P p2007 先例): 字段为 -1(未采样/未观测)时跳过对应判定;
// 未采样不是低利用率证据, 但监控缺失本身必须 FAIL。
// <5s 切片规避: 规格要求按单段和累计 wall 汇总 —— heavy_wall_seconds_total
// 由调用方提供累计值(哨兵=未汇总跳过), 供诊断呈现; 判定仍逐样本收口,
// 与切片数无关。
inline GateDiag evaluate_mon001(const GateConfig& g) {
    if (!monitoring_effective(g)) return GateDiag::MonitoringMissing;
    // B1-A6: 逐样本占比属统计判据, 仅对 >=10s active window 生效(阈值 0.70 不动)。
    if (mon001_sampled(g.util_samples_pass_frac) && gate_window_representative(g) &&
        g.util_samples_pass_frac < kMon001UtilSampleFrac)
        return GateDiag::UtilizationP75Low;
    if (mon001_sampled(g.queue_low_run_seconds) &&
        g.queue_low_run_seconds >= kMon001QueueWindowSeconds)
        return GateDiag::QueueStarvedCpu;
    return GateDiag::Ok;
}

// ---- MON-002(V7 04_CPU_RESOURCE_TASKS) RSS/allocation report 判定 ----
// 判定源 = lib/infrastructure/cli/memory_report.h 报告面(整条曲线稳健斜率+结束回落验证;
// 禁止只看峰值)。与 evaluate_mon001 互补(内存面单独收口):
//   1) report 面未接入(alloc_report_present=false) → Ok(向后兼容, 面外任务不阻塞);
//   2) 面在但零有效样本(伪造 monitor/全哨兵) → AllocReclaimMissing FAIL
//      (无内存证据, 同 MON-001 监控缺失纪律);
//   3) RSS 稳健斜率 >= kAllocGrowthUnboundedMbPerS → AllocGrowthUnbounded
//      (注入 leak 失败路径);
//   4) 结束回落不可解释(UnexplainedResidual) → AllocReclaimMissing。
// 哨兵纪律: alloc_growth_mb_per_s == -1(未采样, 如样本不足未算斜率)跳过增长判定;
// alloc_samples_measured == -1(未提供)跳过零样本拒绝 —— 显式呈现而非静默。
inline GateDiag evaluate_mon002(const GateConfig& g) {
    if (!g.alloc_report_present) return GateDiag::Ok;
    if (mon001_sampled(g.alloc_samples_measured) && g.alloc_samples_measured <= 0.0)
        return GateDiag::AllocReclaimMissing;   // 面在零有效样本 = 无内存证据
    // B1-A6: 与 rss_slope 同源 —— 分配曲线稳态斜率也是统计判据, 仅当 active
    // window >=10s 才具备 §10.5 语义; 短窗启动分配爬坡不得判"无界增长"(阈值
    // kAllocGrowthUnboundedMbPerS=32 不动)。零样本/reclaim 判定另行收口。
    if (gate_window_representative(g) &&
        g.alloc_growth_mb_per_s != kMon001NotSampled &&
        g.alloc_growth_mb_per_s >= kAllocGrowthUnboundedMbPerS)
        return GateDiag::AllocGrowthUnbounded;
    if (g.alloc_reclaim_verdict == AllocReclaimVerdict::UnexplainedResidual)
        return GateDiag::AllocReclaimMissing;
    return GateDiag::Ok;
}

// 诊断字符串: 失败判定给出可操作说明, 含各指标实测值 vs 阈值(MON-002 diagnosis)。
inline std::string diag_message(GateDiag d, const GateConfig& g) {
    switch (d) {
    case GateDiag::SingleThreaded: {
        // MON-002: 采样 worker p50 优先呈现; 未采样呈现 selected/max_active 回退值
        if (g.workers_p50 >= 0.0)
            return "compute 门禁: worker p50 " + std::to_string(g.workers_p50) +
                   " < " + std::to_string(kWorkerP50Min) +
                   " (available_cpus=" + std::to_string(g.available_cpus) +
                   ", selected_workers=" + std::to_string(g.selected_workers) +
                   ", max_active_threads=" + std::to_string(g.max_active_threads) + ")";
        return "compute 门禁: available_cpus=" + std::to_string(g.available_cpus) +
               ">=2 但 selected_workers=" + std::to_string(g.selected_workers) +
               "/max_active_threads=" + std::to_string(g.max_active_threads) + " < 2";
    }
    case GateDiag::LowAvgCores: {
        const double thr = compute_cores_threshold(g);
        return "compute 门禁: avg_equivalent_cores " + std::to_string(g.avg_equivalent_cores) +
               " < 0.85*min(workers,cpus)=" + std::to_string(thr);
    }
    case GateDiag::UnannotatedPriority:
        return "stage 未标注且 wall>5s → P1(07 §1)";
    case GateDiag::ComputeIoMemAllLow:
        return "CPU/io/mem 带宽皆低: cpu=" + std::to_string(g.cpu_percent) +
               "% iowait=" + std::to_string(g.iowait_percent) +
               "% → 禁止'单线程算法正常'解释(07 §3)";
    case GateDiag::MemoryBandwidthLow:
        return "memory 门禁: 带宽比例 " + std::to_string(g.achieved_memory_bandwidth_frac) +
               " 未达 pre-frozen 阈值 " + std::to_string(g.required_memory_bandwidth_frac) +
               "(须 BENCH-003 写入)";
    case GateDiag::IoMissingEvidence:
        return "io 门禁: 缺 bytes/ops/await 证据(允许低 CPU 但须证据)";
    case GateDiag::MixedUnsplit:
        return "mixed 门禁: 未拆出 compute/io 子区间(不得用 mixed 掩盖低利用率)";
    case GateDiag::FastFailFirst10s:
        return "first-10s 快速失败: 低 CPU+非 IO+非内存带宽饱和(07 §4)";
    case GateDiag::GlobalLockDegradation:
        return "compute 门禁: N-worker 相对 1-worker 无正向加速(全局锁退化)";
    case GateDiag::CpuP50Low:
        return "compute 门禁(MON-002): CPU p50 " + std::to_string(g.cpu_p50_percent) +
               "% < " + std::to_string(kCpuP50MinPercent) + "% 阈值(active window>=10s)";
    case GateDiag::CpuMeanLow:
        return "compute 门禁(MON-002): CPU mean " + std::to_string(g.cpu_mean_percent) +
               "% < " + std::to_string(kCpuMeanMinPercent) + "% 阈值(active window>=10s)";
    case GateDiag::MemoryGrowth:
        return "资源门禁(MON-002): 内存持续增长 rss_slope " + std::to_string(g.rss_slope_mb_per_s) +
               " MB/s > 阈值 " + std::to_string(g.memory_growth_limit_mb_per_s) + " MB/s";
    case GateDiag::ProgressStall:
        return "资源门禁(MON-002): 无进度(progress 停滞)";
    case GateDiag::IoWaitHigh:
        return "资源门禁(MON-002): 异常 IO 等待 iowait " + std::to_string(g.iowait_percent) +
               "% > " + std::to_string(g.io_wait_high_percent) + "%";
    case GateDiag::MonitoringMissing:
        return "资源门禁(MON-001): 监控缺失或无有效采样样本 — heavy run 无资源证据即 FAIL";
    case GateDiag::UtilizationP75Low:
        return "资源门禁(MON-001): U>=0.75 样本占比 " +
               (mon001_sampled(g.util_samples_pass_frac)
                    ? std::to_string(g.util_samples_pass_frac)
                    : std::string("unsampled")) +
               " < " + std::to_string(kMon001UtilSampleFrac) +
               " (有效样本 " + (mon001_sampled(g.util_samples_measured)
                                   ? std::to_string(g.util_samples_measured)
                                   : std::string("unsampled")) + ")";
    case GateDiag::QueueStarvedCpu:
        return "资源门禁(MON-001): 队列有工作时连续 " +
               (mon001_sampled(g.queue_low_run_seconds)
                    ? std::to_string(g.queue_low_run_seconds)
                    : std::string("unsampled")) +
               "s 利用率<" + std::to_string(kMon001QueueUtilMinPercent / 100.0) +
               " (窗口阈值 " + std::to_string(kMon001QueueWindowSeconds) + "s)";
    case GateDiag::AllocGrowthUnbounded:
        return "RSS/allocation report(MON-002): RSS 曲线稳健斜率 " +
               (mon001_sampled(g.alloc_growth_mb_per_s)
                    ? std::to_string(g.alloc_growth_mb_per_s)
                    : std::string("unsampled")) +
               " MB/s >= 失败线 " + std::to_string(kAllocGrowthUnboundedMbPerS) +
               " MB/s(整条曲线判定, 非峰值)";
    case GateDiag::AllocReclaimMissing:
        return "RSS/allocation report(MON-002): run 结束回落不可解释 — retained 残留超容差且回落比例低于阈值(注入 leak 类失败)";
    case GateDiag::NotApplicable:
        return "G-RES-01 判定域不成立: 计算区间未严格超过 " +
               std::to_string(kMon002MinWindowSeconds) +
               "s(或有效 CPU<2) — NOT_APPLICABLE(显式分类, 非豁免)";
    default: return "ok";
    }
}

}  // namespace astrocs

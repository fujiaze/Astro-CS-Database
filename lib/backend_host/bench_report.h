// lib/backend_host/bench_report.h — CPU-006 benchmark report (V7.1 §"benchmark cpu 命令")
//
// 规格(V7.1 FINAL3 04_CPU_RESOURCE_TASKS.md CPU-006; 验收关键词 benchmark report):
//   实现 astrocs benchmark cpu --suite quick|release --output profile.json; 记录 CPU 指纹、
//   逻辑/物理核、cache/NUMA/affinity、内存带宽、kernel sizes、warmup、重复轮次、median/MAD、
//   workers、provider、输入 hash、编译器。
//   验收: 不会改变生产 profile 直到全部 self_test+阈值通过; 没有 benchmark 时 baseline;
//   异常值剔除规则预先固定; 命令有 timeout。
//
// 层次合同(不复制测量引擎): 本文件是 CPU-003 测量引擎(generate_profile_v2)之上的
// **审计聚合层**。测量链(能力探测→Oracle 门→warmup→计时→median/MAD→候选选择)全部复用
// generate_profile_v2 唯一实现; 本层只做规格字段的聚合、阈值判定与可复读校验。
// 本层从不写生产 profile 文件(结构性: schema 为 astrocs.benchmark-report/v1, 与
// cpu-profile/v2 不兼容, verify_profile_v2 必须拒绝 report 文本)。
//
// 工程约束落实:
//   - 重计算禁止单线程: 聚合选择对 heavy workload(compute/memory) kernel 强制
//     workers>=2 当存在 >=2 worker 的合格候选(V8-CPU-001 同语义)。
//   - 线程/ISA/block 由 benchmark 选择: 聚合仅从 oracle-pass 候选测量值选择,
//     无任何源码硬编码核数/ISA/block。
//   - 异常值剔除规则预先冻结: kOutlierPolicy(单一出处, report 与 verify 共用)。
//   - 命令 timeout: BenchReportOptions::deadline_ns(lib 侧预算标注) + 命令层
//     shell timeout(调用方 CLI/脚本责任, 见 TASK_RESULT 证据)。
#ifndef ASTROCS_BENCH_REPORT_H
#define ASTROCS_BENCH_REPORT_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "profile_gen.h"   // RawCandidate / ProfileBundle / generate_profile_v2

namespace astrocs::backend_host {

// ── 预冻结规则(唯一出处; CPU-006 验收: 异常值剔除规则预先固定) ──
// 计时统计: 3 warmup(不计时) + 7 measure(单调钟) → median/MAD/p05/p95。
// 不做逐点剔除: median 对离群点天然稳健; p05/p95 仅记录供审计。
// 离散度阈值: 胜出候选 MAD/median > 0.35 → 判 dispersion, 不得通过阈值门。
inline constexpr int kBenchWarmup = 3;
inline constexpr int kBenchSamples = 7;
inline constexpr double kMadMedianRelMax = 0.35;
const char* benchmark_outlier_policy();

// suite 语义(V7.1 规格 --suite quick|release):
//   quick   = 代表 kernel(calibration-pixel-transform) × medium 单规模;
//   release = 全部注册 kernel × {small, medium, large} 全规模(映射引擎 full 模式)。
// 返回 "" 表示 suite 非法。
std::string benchmark_suite_mode(const std::string& suite);   // "quick"|"full"

struct BenchReportOptions {
    std::string suite;            // "quick" | "release"
    std::string build_id;         // X.Y.Z[-pre]+g<hash12>(调用方注入; 单测用中性占位)
    std::string source_commit;    // 40hex
    std::string benchmark_binary_sha256;  // 运行二进制实测 hash(64hex; CLI 传入)
    std::string backends_dir;     // 含 backends.manifest.json 目录; 空=仅内置 baseline
    uint64_t deadline_ns = 0;     // 0=不限; >0: 生成后标注是否超预算(标注性, 见头注释)
};

// per (kernel, size) 的胜出候选(仅 oracle_pass 可胜出; 错误路径结构性不可胜出)。
struct AggWinner {
    std::string provider;        // baseline|avx2|avx512
    uint32_t workers = 0;
    uint64_t block = 0;
    double median_ns = 0, mad_ns = 0, p05_ns = 0, p95_ns = 0;
    uint32_t candidates_all = 0;      // 该 (kernel,size) 全部候选数
    uint32_t candidates_ok = 0;       // oracle 通过候选数
    bool dispersion_ok = false;       // MAD/median <= kMadMedianRelMax
    std::string fallback_reason;      // 空=正常; 非空=无合格候选原因
};

// 纯聚合函数(可独立注入单测; 生产 report 与负向样例共用同一实现):
// 从原始候选聚合每 (kernel,size) 胜出者。规则(预冻结):
//   1) 仅 oracle_pass 候选进入选择;
//   2) best = min(median_ns);
//   3) heavy(compute/memory) kernel: 若存在 oracle_pass 且 workers>=2 的候选, 禁选
//      workers==1 候选(重计算不能退化为单线程);
//   4) 平手(相对差 <1e-2): 保守 provider 序 baseline<avx2<avx512, worker 少者优先。
// 无任何 OK 候选 → provider=baseline + fallback_reason(无 benchmark 时 baseline 语义)。
std::map<std::string, std::map<std::string, AggWinner>> aggregate_benchmark_kernels(
    const std::vector<RawCandidate>& raw);

struct BenchVerdict {
    bool all_oracle_passed = false;      // 每 kernel 每 size 都存在 oracle-pass 候选
    bool thresholds_passed = false;      // oracle 全过 且 每 winner MAD/median <= 0.35
    bool eligible_for_profile = false;   // 上两条 && 未超时(生产 profile 只在此才可写)
};
// 纯判定函数(阈值门; 预冻结): 生产 profile 只在 eligible_for_profile 才可落盘。
BenchVerdict benchmark_report_verdict(
    const std::map<std::string, std::map<std::string, AggWinner>>& kernels,
    bool timeout_reached);

struct BenchReportOutcome {
    std::string json;                          // astrocs.benchmark-report/v1 全文
    std::vector<RawCandidate> raw;             // 全部原始候选(透传, 审计)
    std::map<std::string, std::map<std::string, AggWinner>> kernels;
    std::string raw_samples_sha256;            // 引擎原始候选序列化 hash(输入样本聚合指纹)
    uint32_t available_logical_cpus = 0;       // affinity∩cgroup 有效配额(硬件画像快照)
    bool timeout_reached = false;              // deadline 预算标注
    BenchVerdict verdict;
    bool production_profile_touched = false;   // 恒 false(结构性; verify 强制)
};

// 生成 benchmark report(同步; 单线程聚合, 测量链内部经 host budget 多线程)。
// suite 非法 → 返回 outcome.json 为空(负向)。
BenchReportOutcome generate_benchmark_report(const BenchReportOptions& opt);

// 独立复读: 校验 report 结构/必填字段/冻结规则一致性。返回 "" 表示合法, 否则错误描述。
// 校验含: suite 枚举、warmup==3、samples>=7、outlier_policy 全文一致、
// production_profile_touched==false、verdict 字段齐全、kernels 非空、
// 每 size 项 provider/workers/median 合法、build.source_commit 40hex。
std::string verify_benchmark_report(const std::string& json_text);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_BENCH_REPORT_H

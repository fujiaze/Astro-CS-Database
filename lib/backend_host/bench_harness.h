// lib/backend_host/bench_harness.h — benchmark harness (06 §1/§4) — BENCH-002
// 顺序合同: 正确性筛选(独立 scalar Oracle) → 预热(不计时) → 多次计时(单调钟)
// → 稳健统计(median/MAD/p05/p95) → 选择(仅 verdict==OK 候选可胜出)。
// 速度永远不能使错误路径获胜: Oracle/selftest 失败的 backend 直接禁用。
#ifndef ASTROCS_BENCH_HARNESS_H
#define ASTROCS_BENCH_HARNESS_H

#include <string>
#include <vector>

#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"

namespace astrocs::backend_host {

struct BenchResult {
    std::string backend_id;
    std::string verdict;            // "OK" | "ORACLE_FAIL" | "SELFTEST_FAIL" | "ERROR"
    std::string reason;
    int samples = 0;
    double median_ns = 0, mad_ns = 0, p05_ns = 0, p95_ns = 0;
    uint32_t workers = 0;
    std::string correctness_hash;   // 输出 sha256(06 §4); Oracle 失败时为空
};

/* 对单 kernel 做完整 harness 流程。
 * expected_ref: 独立 scalar 参考输出(与 kernel 实现不同路径); caller 提供。
 * warmup 次数不计时; samples≥7(06 §4)。 */
BenchResult bench_kernel(const astrocs_host_services_v1* host,
                         const char* backend_id,
                         acs_status (*fn)(
                             const astrocs_host_services_v1*, const void*, uint32_t,
                             const void*, void*),
                         const acs_baseline_params_v1& params,
                         const std::vector<double>& expected_ref,
                         double tol_rel, int warmup, int samples);

/* 从 OK 候选中选 median 最小者; 无 OK 候选→空串。错误 backend 结构性不可胜出。 */
std::string select_winner(const std::vector<BenchResult>& results);

/* ── 候选生成(06 §3; 全部派生, 源码零硬编码核数/机器数值) ── */

/* worker 候选: {1, 中位(≈物理核级), 全部有效 CPU} 去重升序; avail=3→{1,2,3} */
std::vector<uint32_t> worker_candidates(uint32_t available_cpus);

/* block 候选: 由 L2 字节与元素尺寸派生的几何序列(公比 4), 机器无关 */
std::vector<uint64_t> block_candidates(uint64_t l2_bytes, uint64_t elt_size);

struct MemoryReport {
    double read_gbs = 0, write_gbs = 0, copy_gbs = 0, triad32_gbs = 0, triad64_gbs = 0;
    uint64_t rss_delta_bytes = 0;
};

/* 内存带宽基线(06 §3 末段): read/write/copy/triad(FP32+FP64), median-of-reps */
MemoryReport bench_memory(uint64_t n_elements, int reps);

/* 采集当前进程 RSS 字节(资源指标) */
uint64_t current_rss_bytes();

}  // namespace astrocs::backend_host

#endif  // ASTROCS_BENCH_HARNESS_H

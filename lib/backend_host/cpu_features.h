// lib/backend_host/cpu_features.h — CPU/OS 状态检测 (05 §3) — ABI-002
// 检测序: CPUID feature bits + OSXSAVE + XGETBV(XCR0) + affinity 可用 CPU。
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* feature bits(v1 冻结); bit0=SSE2 为最低 amd64 基线, 恒置位(x86) */
#define ACS_FEAT_SSE2     (1ull << 0)
#define ACS_FEAT_SSE4_1   (1ull << 1)
#define ACS_FEAT_AVX      (1ull << 2)
#define ACS_FEAT_AVX2     (1ull << 3)
#define ACS_FEAT_FMA      (1ull << 4)
#define ACS_FEAT_AVX512F  (1ull << 5)

/* 实测 CPUID+OSXSAVE+XGETBV: AVX 系仅在 OS 保存对应状态时置位(05 §3-2/3)。
 * reentrant=yes; threadsafe=yes; internal_parallel=none。 */
uint64_t astrocs_cpu_detect_features_v1(void);

/* 当前进程可用 CPU 数(affinity ∩ cgroup ∩ Job Object, 非机器总核数; 05 §3-4)。
 * reentrant=yes; threadsafe=yes。 */
uint32_t astrocs_cpu_affinity_count_v1(void);

#ifdef __cplusplus
}
#endif

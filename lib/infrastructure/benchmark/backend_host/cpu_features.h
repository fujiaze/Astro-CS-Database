// lib/infrastructure/benchmark/backend_host/cpu_features.h — CPU/OS 状态检测 — ABI-002
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
/* TRUTHFUL-CONCLUSION-01（一页纸 S2-C）：AVX-512 **子集位**（尾部追加，既有位值不变）。
 * 理由：AVX-512F 不蕴含 BW/DQ/VL（KNL 一类 F+CD+ER+PF 机器有 F 而无 BW/DQ/VL），
 * 而 astrocs_cpu_avx512 由根 CMakeLists.txt 以 -mavx512f -mavx512bw -mavx512vl
 * -mavx512dq 编译。声明层若只写 F，就是「声明 ⊊ 编译」⇒ 出厂机器上加载放行、
 * 执行撞非法指令。子集位就位后，声明／编译／检测三侧才能逐位同源。 */
#define ACS_FEAT_AVX512CD (1ull << 6)
#define ACS_FEAT_AVX512BW (1ull << 7)
#define ACS_FEAT_AVX512DQ (1ull << 8)
#define ACS_FEAT_AVX512VL (1ull << 9)

/* avx512 provider 的**声明需求位**（唯一拼写点）：与 avx512_backend.cpp 的
 * ASTROCS_BACKEND_REQUIRED_FEATURES、cpu_routing.cpp 的 provider_supported 判定
 * 共用同一组位，改一处即三处同步。 */
#define ACS_FEAT_AVX512_PROVIDER_REQUIRED \
    (ACS_FEAT_AVX512F | ACS_FEAT_AVX512BW | ACS_FEAT_AVX512DQ | ACS_FEAT_AVX512VL)

/* 实测 CPUID+OSXSAVE+XGETBV: AVX 系仅在 OS 保存对应状态时置位(05 §3-2/3)。
 * reentrant=yes; threadsafe=yes; internal_parallel=none。 */
uint64_t astrocs_cpu_detect_features_v1(void);

/* 当前进程可用 CPU 数(affinity ∩ cgroup ∩ Job Object, 非机器总核数; 05 §3-4)。
 * reentrant=yes; threadsafe=yes。 */
uint32_t astrocs_cpu_affinity_count_v1(void);

#ifdef __cplusplus
}
#endif
